#include "HttpApi.h"
#include "../../include/config.h"
#include "../apps/Apps.h"
#include "../canvas/Canvas.h"
#include "../hw/Hw.h"
#include "../net/Net.h"
#include "../settings/Settings.h"
#include "../ui/screen.h"
#include "../vdisp/VDisplay.h"

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <time.h>

static AsyncWebServer s_srv(80);
static fs::File           s_upload;

enum PendingKind : uint8_t {
    PK_NONE = 0,
    PK_SAVE_WIFI,
    PK_FORGET,
    PK_RECONNECT,
    PK_AP_ON,
    PK_AP_OFF
};
static volatile PendingKind s_pendKind = PK_NONE;
static uint32_t s_pendAt = 0;
static char     s_pendSsid[33], s_pendPass[65];
static bool     s_pendApTimed = true;

// ── AUTH ─────────────────────────────────────────────────────────────────
// Mirrors the original's open/token modes: an empty token means open.
static bool authed(AsyncWebServerRequest *r) {
    if (!CFG.apiToken[0]) return true;
    if (!r->hasHeader("X-API-Token")) return false;
    return r->header("X-API-Token") == CFG.apiToken;
}

static void sendErr(AsyncWebServerRequest *r, int code, const char *msg) {
    JsonDocument d;
    d["error"] = msg;
    String out;
    serializeJson(d, out);
    r->send(code, "application/json", out);
}

static bool guard(AsyncWebServerRequest *r) {
    if (authed(r)) return true;
    sendErr(r, 401, "X-API-Token required");
    return false;
}

// ── BMP SCREENSHOT ───────────────────────────────────────────────────────
// Generated on demand into the response's own chunks. A back-panel BMP is
// 38 KB and the heap has better uses than a second copy of it.
//
// This reads the framebuffer from the web-server task while loop() may be
// writing it, so a screenshot can catch a half-drawn frame. That is accepted
// rather than locked: the endpoint is a preview, a torn frame costs one
// refresh at 2 fps, and a mutex here would put network latency in the render
// path -- which is the one thing the dirty-region model exists to keep out.
struct BmpCtx { int w, h, rowRaw, rowPad; uint32_t dataSize; uint8_t hdr[54]; };

static void bmpHeader(BmpCtx &c) {
    memset(c.hdr, 0, sizeof(c.hdr));
    uint32_t total = 54 + c.dataSize;
    c.hdr[0] = 'B'; c.hdr[1] = 'M';
    memcpy(&c.hdr[2], &total, 4);
    uint32_t off = 54;   memcpy(&c.hdr[10], &off, 4);
    uint32_t hs  = 40;   memcpy(&c.hdr[14], &hs, 4);
    int32_t  w   = c.w;  memcpy(&c.hdr[18], &w, 4);
    int32_t  h   = c.h;  memcpy(&c.hdr[22], &h, 4);
    uint16_t pl  = 1;    memcpy(&c.hdr[26], &pl, 2);
    uint16_t bpp = 24;   memcpy(&c.hdr[28], &bpp, 2);
    memcpy(&c.hdr[34], &c.dataSize, 4);
}

static void handleScreen(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    // `display` is still accepted so BUSY Bar tooling keeps working, but the
    // board has one screen and both values return the same image.
    if (r->hasParam("display")) {
        int which = r->getParam("display")->value().toInt();
        if (which < 0 || which > 1) { sendErr(r, 400, "display must be 0 or 1"); return; }
    }

    auto *c = new BmpCtx();
    c->w = vd::width();
    c->h = vd::height();
    c->rowRaw = c->w * 3;
    c->rowPad = ((c->rowRaw + 3) / 4) * 4;
    c->dataSize = (uint32_t)c->rowPad * c->h;
    bmpHeader(*c);

    AsyncWebServerResponse *resp = r->beginChunkedResponse(
        "image/bmp",
        [c](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
            uint32_t total = 54 + c->dataSize;
            if (index >= total) return 0;
            size_t n = 0;
            while (n < maxLen && index + n < total) {
                uint32_t p = index + n;
                uint8_t out;
                if (p < 54) {
                    out = c->hdr[p];
                } else {
                    uint32_t q   = p - 54;
                    int row = (int)(q / c->rowPad);
                    int col = (int)(q % c->rowPad);
                    if (col >= c->rowRaw) {
                        out = 0;                       // row padding to 4 bytes
                    } else {
                        int y = c->h - 1 - row;        // BMP rows run bottom-up
                        int x = col / 3;
                        uint16_t px = vd::fb()[y * VD_W + x];
                        uint8_t R = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
                        uint8_t G = (uint8_t)(((px >> 5)  & 0x3F) * 255 / 63);
                        uint8_t B = (uint8_t)((px & 0x1F) * 255 / 31);
                        out = (col % 3 == 0) ? B : (col % 3 == 1) ? G : R;
                    }
                }
                buf[n++] = out;
            }
            return n;
        });
    resp->addHeader("Cache-Control", "no-store");
    // The context outlives the handler, so it is freed when the response is.
    r->onDisconnect([c]() { delete c; });
    r->send(resp);
}

// ── ROUTES ───────────────────────────────────────────────────────────────

static void routeStatus(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    JsonDocument d;
    d["name"]     = FW_NAME;
    d["version"]  = FW_VERSION;
    d["api"]      = FW_API_COMPAT;
    d["uptime_s"] = (uint32_t)(millis() / 1000);
    d["heap_free"]= ESP.getFreeHeap();
    d["heap_min"] = ESP.getMinFreeHeap();

    JsonObject net = d["network"].to<JsonObject>();
    net["state"] = wifiState() == NET_ONLINE ? "online"
                 : wifiState() == NET_PORTAL ? "portal" : "connecting";
    net["ip"]    = wifiIP().toString();
    net["ssid"]  = CFG.ssid;
    net["host"]  = CFG.host;
    net["rssi"]  = WiFi.RSSI();

    JsonObject ap = net["ap"].to<JsonObject>();
    ap["active"]     = apActive();
    ap["fallback"]   = apIsFallback();   // no credentials: cannot be turned off
    ap["ssid"]       = apSsid();
    ap["ip"]         = apIP().toString();
    ap["ms_left"]    = apMsRemaining();  // 0 when it does not time out
    ap["clients"]    = WiFi.softAPgetStationNum();

    JsonObject disp = d["display"].to<JsonObject>();
    disp["brightness"]  = blGet();
    disp["mode"]        = CFG.blMode ? "manual" : "auto";
    disp["ldr_raw"]     = ldrRaw();
    disp["night"]       = CFG.night;

    JsonObject p = d["panel"].to<JsonObject>();
    p["width"]    = vd::width();
    p["height"]   = vd::height();
    p["scale"]    = VD_SCALE;
    p["owner"]    = canvasOwns() ? canvasOwner() : "";
    p["priority"] = canvasPriority();

    d["elements"] = canvasElementCount();
    d["app"]      = appActive() == APP_CLOCK ? "clock"
                  : appActive() == APP_STATUS ? "status" : "settings";
    d["theme"]    = themeCurrent().name;
    d["time_synced"] = timeSynced();

    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

static void routeVersion(AsyncWebServerRequest *r) {
    JsonDocument d;
    d["name"] = FW_NAME;
    d["version"] = FW_VERSION;
    d["api_version"] = FW_API_COMPAT;
    d["hardware"] = "ESP32-2432S028R";
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

static void routeTimeGet(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    JsonDocument d;
    struct tm t;
    bool have = timeNow(t);
    d["synced"] = have;
    d["tz"]     = CFG.tz;
    d["epoch"]  = (uint32_t)time(nullptr);
    d["clock24"]= CFG.clock24;
    if (have) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
        d["local"] = buf;
    }
    JsonArray tzs = d["tz_common"].to<JsonArray>();
    tzs.add("UTC0");
    tzs.add("ICT-7");
    tzs.add("GMT0BST,M3.5.0/1,M10.5.0");
    tzs.add("CET-1CEST,M3.5.0,M10.5.0/3");
    tzs.add("EST5EDT,M3.2.0,M11.1.0");
    tzs.add("PST8PDT,M3.2.0,M11.1.0");
    tzs.add("JST-9");
    tzs.add("AEST-10AEDT,M10.1.0,M4.1.0/3");
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

static void routeWifiGet(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    JsonDocument d;
    d["state"] = wifiState() == NET_ONLINE ? "online"
               : wifiState() == NET_PORTAL ? "portal" : "connecting";
    d["ssid"]  = CFG.ssid;
    d["ip"]    = wifiIP().toString();
    d["host"]  = CFG.host;
    d["rssi"]  = WiFi.RSSI();
    JsonObject ap = d["ap"].to<JsonObject>();
    ap["active"]   = apActive();
    ap["fallback"] = apIsFallback();
    ap["ssid"]     = apSsid();
    ap["ip"]       = apIP().toString();
    ap["ms_left"]  = apMsRemaining();
    ap["clients"]  = WiFi.softAPgetStationNum();
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

// Kicks a scan off on the first call and reports progress on the rest, so the
// caller polls instead of holding a connection open for the two to four
// seconds a scan takes.
static void routeScan(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    ScanState st = scanState();
    if (st == SCAN_IDLE) { scanStart(); st = SCAN_RUNNING; }

    JsonDocument d;
    d["scanning"] = (st == SCAN_RUNNING);
    JsonArray a = d["networks"].to<JsonArray>();
    if (st == SCAN_DONE) {
        int n = scanCount();
        for (int i = 0; i < n && i < 32; i++) {
            char ssid[33]; int32_t rssi; bool secure;
            if (!scanResult(i, ssid, sizeof(ssid), rssi, secure)) continue;
            if (!ssid[0]) continue;                 // hidden network
            JsonObject o = a.add<JsonObject>();
            o["ssid"]   = ssid;
            o["rssi"]   = rssi;
            o["secure"] = secure;
        }
    }
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

static void routeScanRestart(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    scanStart();
    r->send(202, "application/json", "{\"scanning\":true}");
}

static void routeInput(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("key")) { sendErr(r, 400, "key is required"); return; }
    String k = r->getParam("key")->value();
    if      (k == "ok")   appSetActive((AppId)((appActive() + 1) % APP_COUNT));
    else if (k == "back") appSetActive(APP_CLOCK);
    else if (k == "up")   { appSetActive(APP_STATUS); themeNext(-1); }
    else if (k == "down") { appSetActive(APP_STATUS); themeNext(1); }
    else { sendErr(r, 400, "key must be ok, back, up or down"); return; }
    r->send(200, "application/json", "{\"ok\":true}");
}

static void routeThemeGet(AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    JsonDocument d;
    d["active"] = themeCurrent().name;
    d["label"]  = themeCurrent().label;
    JsonArray a = d["available"].to<JsonArray>();
    for (uint8_t i = 0; i < themeCount(); i++) a.add(themeNameAt(i));
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
}

// ── UPLOAD ───────────────────────────────────────────────────────────────
static void onUpload(AsyncWebServerRequest *r, const String &filename, size_t index,
                     uint8_t *data, size_t len, bool final) {
    if (!authed(r)) return;
    if (index == 0) {
        if (!LittleFS.exists("/assets")) LittleFS.mkdir("/assets");
        String path = "/assets/" + filename;
        s_upload = LittleFS.open(path, "w");
        Serial.printf("[api] upload %s\n", path.c_str());
    }
    if (s_upload) s_upload.write(data, len);
    if (final && s_upload) { s_upload.close(); }
}

// ── SETUP ────────────────────────────────────────────────────────────────
void apiBegin() {
    // ── display ──
    auto *draw = new AsyncCallbackJsonWebHandler("/api/display/draw",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            char err[96] = "";
            DrawResult res = canvasDraw(json.as<JsonObjectConst>(), err, sizeof(err));
            switch (res) {
                case DR_OK:
                    ledSet(canvasLedColour());
                    r->send(200, "application/json", "{\"ok\":true}");
                    break;
                case DR_CONFLICT:    sendErr(r, 409, err); break;
                case DR_FULL:        sendErr(r, 507, err); break;
                default:             sendErr(r, 400, err); break;
            }
        });
    draw->setMethod(HTTP_POST);
    s_srv.addHandler(draw);

    s_srv.on("/api/display/draw", HTTP_DELETE, [](AsyncWebServerRequest *r) {
        if (!guard(r)) return;
        const char *app = r->hasParam("application_name")
                        ? r->getParam("application_name")->value().c_str() : nullptr;
        canvasClear(app);
        ledSet(canvasLedColour());
        screenInvalidate();
        r->send(200, "application/json", "{\"ok\":true}");
    });

    auto *bright = new AsyncCallbackJsonWebHandler("/api/display/brightness",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            JsonObjectConst o = json.as<JsonObjectConst>();
            if (o["auto"].is<bool>() && o["auto"].as<bool>()) {
                CFG.blMode = 0;
            } else if (o["value"].is<int>()) {
                int v = o["value"].as<int>();               // 0..100 percent
                if (v < 0 || v > 100) { sendErr(r, 400, "value must be 0..100"); return; }
                // Snap to the same four manual steps the on-device card
                // offers, so the API and the screen can never disagree about
                // the state. 0 lands on the dimmest step rather than off: the
                // backlight never goes fully dark outside night mode.
                CFG.blMode = (uint8_t)(v <= 37 ? 1 : v <= 62 ? 2 : v <= 87 ? 3 : 4);
            } else {
                sendErr(r, 400, "expected {\"value\":0..100} or {\"auto\":true}");
                return;
            }
            settingsSave();
            screenInvalidate();
            r->send(200, "application/json", "{\"ok\":true}");
        });
    bright->setMethod(HTTP_PUT);
    s_srv.addHandler(bright);

    s_srv.on("/api/screen", HTTP_GET, handleScreen);

    // ── device ──
    s_srv.on("/api/status",  HTTP_GET,  routeStatus);
    s_srv.on("/api/version", HTTP_GET,  routeVersion);
    s_srv.on("/api/time",    HTTP_GET,  routeTimeGet);
    s_srv.on("/api/input",   HTTP_POST, routeInput);
    s_srv.on("/api/themes",  HTTP_GET,  routeThemeGet);

    // /api/wifi/scan MUST be registered before /api/wifi (GET): AsyncWebServer
    // matches routes in registration order with prefix-matching, so registering
    // /api/wifi first causes GET /api/wifi/scan to be shadowed by routeWifiGet.
    s_srv.on("/api/wifi/scan",  HTTP_GET,  routeScan);
    s_srv.on("/api/wifi/scan",  HTTP_POST, routeScanRestart);
    s_srv.on("/api/wifi",       HTTP_GET,  routeWifiGet);

    auto *timePut = new AsyncCallbackJsonWebHandler("/api/time",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            JsonObjectConst o = json.as<JsonObjectConst>();
            if (o["tz"].is<const char *>()) {
                strncpy(CFG.tz, o["tz"], sizeof(CFG.tz) - 1);
                CFG.tz[sizeof(CFG.tz) - 1] = '\0';
                timeResync();
            }
            if (o["clock24"].is<bool>()) CFG.clock24 = o["clock24"];
            settingsSave();
            screenInvalidate();
            r->send(200, "application/json", "{\"ok\":true}");
        });
    timePut->setMethod(HTTP_PUT);
    s_srv.addHandler(timePut);

    auto *themePut = new AsyncCallbackJsonWebHandler("/api/themes",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            const char *name = json["name"] | (const char *)nullptr;
            if (!name) { sendErr(r, 400, "name is required"); return; }
            if (!themeLoad(name)) { sendErr(r, 404, "no such theme"); return; }
            strncpy(CFG.theme, name, sizeof(CFG.theme) - 1);
            settingsSave();
            appSetActive(APP_STATUS);
            r->send(200, "application/json", "{\"ok\":true}");
        });
    themePut->setMethod(HTTP_PUT);
    s_srv.addHandler(themePut);

    auto *apPost = new AsyncCallbackJsonWebHandler("/api/wifi/ap",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            bool on = json["enabled"] | true;
            if (!on && apIsFallback()) {
                sendErr(r, 409, "no credentials stored: the AP is the only way in");
                return;
            }
            s_pendApTimed = json["timed"] | true;
            s_pendAt = millis() + API_DEFER_MS;
            s_pendKind = on ? PK_AP_ON : PK_AP_OFF;

            JsonDocument d;
            d["active"]  = on;
            d["ssid"]    = apSsid();
            d["ip"]      = apIP().toString();
            d["ms_left"] = (on && s_pendApTimed && !apIsFallback()) ? AP_TIMEOUT_MS : 0;
            String out; serializeJson(d, out);
            r->send(200, "application/json", out);
        });
    apPost->setMethod(HTTP_POST);
    s_srv.addHandler(apPost);

    s_srv.on("/api/wifi/reconnect", HTTP_POST, [](AsyncWebServerRequest *r) {
        if (!guard(r)) return;
        if (!CFG.ssid[0]) { sendErr(r, 409, "no credentials stored"); return; }
        s_pendAt = millis() + API_DEFER_MS;
        s_pendKind = PK_RECONNECT;
        r->send(200, "application/json", "{\"ok\":true}");
    });

    s_srv.on("/api/wifi/forget", HTTP_POST, [](AsyncWebServerRequest *r) {
        if (!guard(r)) return;
        s_pendAt = millis() + API_DEFER_MS;
        s_pendKind = PK_FORGET;
        r->send(200, "application/json", "{\"ok\":true}");
    });

    auto *wifiPut = new AsyncCallbackJsonWebHandler("/api/wifi",
        [](AsyncWebServerRequest *r, JsonVariant &json) {
            if (!guard(r)) return;
            JsonObjectConst o = json.as<JsonObjectConst>();
            const char *ssid = o["ssid"] | (const char *)nullptr;
            if (!ssid || !*ssid) { sendErr(r, 400, "ssid is required"); return; }
            strncpy(s_pendSsid, ssid, sizeof(s_pendSsid) - 1);
            s_pendSsid[sizeof(s_pendSsid) - 1] = '\0';
            const char *pass = o["pass"] | "";
            strncpy(s_pendPass, pass, sizeof(s_pendPass) - 1);
            s_pendPass[sizeof(s_pendPass) - 1] = '\0';
            s_pendAt = millis() + API_DEFER_MS;
            s_pendKind = PK_SAVE_WIFI;
            r->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
        });
    wifiPut->setMethod(HTTP_PUT);
    s_srv.addHandler(wifiPut);

    // ── assets ──
    s_srv.on("/api/assets/upload", HTTP_POST,
             [](AsyncWebServerRequest *r) {
                 if (!guard(r)) return;
                 r->send(200, "application/json", "{\"ok\":true}");
             },
             onUpload);

    s_srv.on("/api/assets", HTTP_GET, [](AsyncWebServerRequest *r) {
        if (!guard(r)) return;
        JsonDocument d;
        JsonArray a = d["assets"].to<JsonArray>();
        fs::File dir = LittleFS.open("/assets");
        if (dir) {
            for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
                JsonObject o = a.add<JsonObject>();
                o["path"] = String("/assets/") + f.name();
                o["size"] = f.size();
            }
        }
        String out; serializeJson(d, out);
        r->send(200, "application/json", out);
    });

    // ── web UI ──
    s_srv.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    s_srv.onNotFound([](AsyncWebServerRequest *r) {
        // In portal mode every unknown host is us, so send the setup page
        // rather than a 404 the captive-portal detector cannot use.
        if (wifiState() == NET_PORTAL) { r->redirect("/"); return; }
        sendErr(r, 404, "not found");
    });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type,X-API-Token");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");

    s_srv.begin();
    Serial.println("[api] listening on :80");
}

void apiTick(uint32_t now) {
    if (s_pendKind == PK_NONE) return;
    if ((int32_t)(now - s_pendAt) < 0) return;

    PendingKind kind = s_pendKind;
    s_pendKind = PK_NONE;

    switch (kind) {
        case PK_SAVE_WIFI:
            wifiSetCreds(s_pendSsid, s_pendPass);
            s_srv.end();
            delay(50);
            ESP.restart();
            break;
        case PK_FORGET:
            wifiForget();
            break;
        case PK_RECONNECT:
            wifiReconnect();
            break;
        case PK_AP_ON:
            apStart(s_pendApTimed);
            setupInvalidate();
            screenInvalidate();
            break;
        case PK_AP_OFF:
            apStop();
            setupInvalidate();
            screenInvalidate();
            break;
        default:
            break;
    }
}
