#include "api/HttpApi.h"
#include "Version.h"
#include "canvas/Canvas.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "display/Backlight.h"
#include "apps/AppManager.h"
#include "apps/ThemeApp.h"
#include "net/WifiSetup.h"
#include "net/TimeSync.h"
#include "settings/Settings.h"
#include <WebServer.h>
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>

using fs::File;
#include <WiFi.h>
#include <esp_system.h>

static WebServer server(80);

static const char* HDRS[] = {"Content-Length", "Content-Type", "X-API-Token"};

static void cors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Token");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
}

static void sendOk() {
  cors();
  server.send(200, "application/json", "{\"result\":\"OK\"}");
}

static void sendErr(int code, const char* msg) {
  cors();
  JsonDocument d;
  d["error"] = msg;
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}

static bool authOk() {
  if (Settings.d.apiToken[0] == 0) return true;
  String tok = server.header("X-API-Token");
  if (!tok.length()) tok = server.arg("x-api-token");
  return tok == Settings.d.apiToken;
}

static bool requireAuth() {
  if (authOk()) return true;
  sendErr(401, "Unauthorized");
  return false;
}

static bool saneName(const char* s, int maxLen) {
  if (!s || !*s) return false;
  int n = 0;
  for (const char* p = s; *p; p++, n++) {
    if (n >= maxLen) return false;
    char c = *p;
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

static bool sanePath(const char* s) {
  if (!s || !*s || strstr(s, "..")) return false;
  for (const char* p = s; *p; p++) {
    char c = *p;
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-' || c == '/';
    if (!ok) return false;
  }
  return true;
}

static String body() { return server.arg("plain"); }

static void handleOptions() {
  cors();
  server.send(204);
}

static void handleVersion() {
  if (!requireAuth()) return;
  cors();
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"api_semver\":\"%s\"}", API_SEMVER);
  server.send(200, "application/json", buf);
}

static void handleTransport() {
  if (!requireAuth()) return;
  cors();
  server.send(200, "application/json", "{\"type\":\"wifi\"}");
}

static void appendUptime(char* buf, size_t n) {
  uint32_t s = millis() / 1000;
  uint32_t d = s / 86400; s %= 86400;
  uint32_t h = s / 3600; s %= 3600;
  uint32_t m = s / 60; s %= 60;
  snprintf(buf, n, "%02ud %02uh %02um %02us", d, h, m, s);
}

static void macStr(char* buf, const uint8_t* m) {
  snprintf(buf, 18, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void handleStatus() {
  if (!requireAuth()) return;
  cors();
  JsonDocument doc;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macs[18], serial[18];
  macStr(macs, mac);
  snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  auto dev = doc["device"].to<JsonObject>();
  dev["serial_number"] = serial;
  dev["usb_mac"] = macs;
  dev["wifi_mac"] = macs;
  dev["otp_valid"] = false;
  dev["otp_model"] = DEVICE_MODEL;
  dev["firmware_security"] = "insecure";
  auto fw = doc["firmware"].to<JsonObject>();
  fw["version"] = FW_VERSION;
  fw["target"] = FW_TARGET;
  fw["branch"] = FW_BRANCH;
  fw["build_date"] = __DATE__;
  fw["commit_hash"] = "cyd-busybar";
  fw["intercom_version"] = "none";
  auto sys = doc["system"].to<JsonObject>();
  sys["api_semver"] = API_SEMVER;
  char up[24];
  appendUptime(up, sizeof(up));
  sys["uptime"] = up;
  sys["boot_time"] = (int)(TimeSync::nowUtc() - (time_t)(millis() / 1000));
  sys["auto_update_enabled"] = false;
  auto pwr = doc["power"].to<JsonObject>();
  pwr["state"] = "charged";
  pwr["battery_charge"] = 100;
  pwr["battery_voltage"] = 5000;
  pwr["battery_current"] = 0;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleTimeGet() {
  if (!requireAuth()) return;
  cors();
  char iso[40];
  TimeSync::formatIso(iso, sizeof(iso));
  JsonDocument d;
  d["timestamp"] = iso;
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleTimePut() {
  if (!requireAuth()) return;
  String ts = server.arg("timestamp");
  if (!ts.length()) {
    JsonDocument d;
    if (!deserializeJson(d, body()) && d["timestamp"]) ts = d["timestamp"].as<String>();
  }
  if (!ts.length() || !TimeSync::setFromIso(ts.c_str())) {
    sendErr(400, "Invalid timestamp");
    return;
  }
  sendOk();
}

static void handleTzGet() {
  if (!requireAuth()) return;
  cors();
  const TzEntry* e = Settings.findTz(Settings.d.tzName);
  JsonDocument d;
  d["name"] = Settings.d.tzName;
  d["offset"] = e ? e->offset : "+00:00";
  d["abbr"] = e ? e->abbr : "UTC";
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleTzSet() {
  if (!requireAuth()) return;
  String name = server.arg("timezone");
  if (!name.length()) {
    JsonDocument d;
    if (!deserializeJson(d, body()) && d["timezone"]) name = d["timezone"].as<String>();
  }
  if (!Settings.findTz(name.c_str())) {
    sendErr(400, "Unknown timezone");
    return;
  }
  Settings.setTz(name.c_str());
  TimeSync::applyTz();
  Settings.save();
  sendOk();
}

static void handleTzList() {
  if (!requireAuth()) return;
  cors();
  JsonDocument d;
  JsonArray arr = d["list"].to<JsonArray>();
  size_t n = 0;
  const TzEntry* list = Settings.tzList(n);
  for (size_t i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = list[i].name;
    o["offset"] = list[i].offset;
    o["abbr"] = list[i].abbr;
  }
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleBrightnessGet() {
  if (!requireAuth()) return;
  cors();
  JsonDocument d;
  if (Backlight.mode() == BrightnessMode::Auto) d["value"] = "auto";
  else {
    char b[8];
    snprintf(b, sizeof(b), "%u", (unsigned)Settings.d.brightness);
    d["value"] = b;
  }
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleBrightnessSet() {
  if (!requireAuth()) return;
  String v = server.arg("value");
  if (!v.length()) {
    JsonDocument d;
    if (!deserializeJson(d, body()) && !d["value"].isNull()) v = d["value"].as<String>();
  }
  if (v == "auto") {
    Backlight.setMode(BrightnessMode::Auto);
  } else {
    int n = v.toInt();
    if (n < 0 || n > 100 || v.length() == 0) {
      sendErr(400, "Invalid brightness");
      return;
    }
    Backlight.setMode(BrightnessMode::Manual);
    Backlight.setManual((uint8_t)n);
  }
  Settings.save();
  sendOk();
}

static void handleInput() {
  if (!requireAuth()) return;
  Key k = parseKey(server.arg("key").c_str());
  if (k == Key::None) {
    sendErr(400, "Invalid key");
    return;
  }
  Apps.handleKey(k);
  sendOk();
}

static void sendBmp(const uint16_t* fb, int w, int h) {
  int rowSize = (w * 3 + 3) & ~3;
  uint32_t imgSize = (uint32_t)rowSize * h;
  uint32_t fileSize = 54 + imgSize;
  uint8_t hdr[54];
  memset(hdr, 0, 54);
  hdr[0] = 'B'; hdr[1] = 'M';
  hdr[2] = fileSize & 0xFF; hdr[3] = (fileSize >> 8) & 0xFF;
  hdr[4] = (fileSize >> 16) & 0xFF; hdr[5] = (fileSize >> 24) & 0xFF;
  hdr[10] = 54;
  hdr[14] = 40;
  hdr[18] = w & 0xFF; hdr[19] = (w >> 8) & 0xFF;
  hdr[22] = h & 0xFF; hdr[23] = (h >> 8) & 0xFF;
  hdr[26] = 1;
  hdr[28] = 24;
  cors();
  server.setContentLength(fileSize);
  server.send(200, "image/bmp", "");
  WiFiClient client = server.client();
  client.write(hdr, 54);
  uint8_t row[160 * 3 + 4];
  for (int y = h - 1; y >= 0; y--) {
    memset(row, 0, rowSize);
    for (int x = 0; x < w; x++) {
      uint8_t r, g, b;
      rgb565split(fb[y * w + x], r, g, b);
      row[x * 3 + 0] = b;
      row[x * 3 + 1] = g;
      row[x * 3 + 2] = r;
    }
    client.write(row, rowSize);
  }
}

static void handleScreen() {
  if (!requireAuth()) return;
  int d = server.arg("display").toInt();
  if (d == 0) sendBmp(Display.frontBuf(), FRONT_W, FRONT_H);
  else if (d == 1) sendBmp(Display.backBuf(), BACK_W, BACK_H);
  else sendErr(400, "Wrong display");
}

static bool parseOneElement(JsonObject o, CanvasElem& e, const char* app, int32_t& defaultZ) {
  memset(&e, 0, sizeof(e));
  const char* id = o["id"];
  const char* type = o["type"];
  if (!id || !type) return false;
  if (!saneName(id, 23)) return false;
  strlcpy(e.id, id, sizeof(e.id));
  strlcpy(e.app, app, sizeof(e.app));
  e.x = o["x"] | 0;
  e.y = o["y"] | 0;
  e.timeoutSec = o["timeout"] | 0;
  if (o["display_until"].is<const char*>()) e.displayUntil = atoll(o["display_until"]);
  else if (o["display_until"].is<long>()) e.displayUntil = o["display_until"];
  if (e.timeoutSec && e.displayUntil) return false;
  if (o["z_index"].isNull()) {
    e.z = defaultZ;
    defaultZ += 10;
  } else {
    e.z = o["z_index"];
    if (e.z < 0) return false;
  }
  const char* al = o["align"] | "top_left";
  e.align = parseAlign(al);
  const char* disp = o["display"] | "front";
  if (!strcmp(disp, "back")) e.display = DisplayId::Back;
  else if (!strcmp(disp, "front")) e.display = DisplayId::Front;
  else return false;
  e.opacity = o["opacity"] | 100;
  e.color = 0xFFFF;
  e.fillC[0] = 0xFFFF;
  e.fillC[1] = 0;
  e.borderC = 0xFFFF;
  e.borderW = 1;

  if (!strcmp(type, "text")) {
    e.type = ElemType::Text;
    const char* text = o["text"];
    const char* font = o["font"];
    if (!text || !font || !text[0]) return false;
    strlcpy(e.text, text, sizeof(e.text));
    e.font = parseFont(font);
    if (o["color"]) {
      uint16_t c; uint8_t a;
      if (!parseHexColor(o["color"], c, &a)) return false;
      e.color = c;
    }
    e.boxW = o["width"] | 0;
    e.scrollRate = o["scroll_rate"] | 0;
    e.scrollStartMs = o["scroll_start_delay"] | 0;
    e.scrollRepeatMs = o["scroll_repeat_delay"] | 0;
    return true;
  }
  if (!strcmp(type, "countdown")) {
    e.type = ElemType::Countdown;
    const char* ts = o["timestamp"];
    const char* dir = o["direction"];
    const char* hrs = o["show_hours"];
    if (!ts || !dir || !hrs) return false;
    e.timestamp = atoll(ts);
    if (!strcmp(dir, "time_left")) e.cdDir = 0;
    else if (!strcmp(dir, "time_since")) e.cdDir = 1;
    else return false;
    if (!strcmp(hrs, "always")) e.cdHours = 1;
    else if (!strcmp(hrs, "when_non_zero")) e.cdHours = 0;
    else return false;
    if (o["color"]) {
      uint16_t c;
      if (!parseHexColor(o["color"], c)) return false;
      e.color = c;
    }
    return true;
  }
  if (!strcmp(type, "rectangle")) {
    e.type = ElemType::Rect;
    e.rw = o["width"] | 0;
    e.rh = o["height"] | 0;
    if (e.rw == 0 || e.rh == 0) return false;
    e.radius = o["radius"] | 0;
    e.borderW = o["border_width"] | 1;
    FillMode fm;
    if (!CanvasEngine::parseFill(o["fill"] | "none", fm)) return false;
    e.fill = fm;
    if (o["fill_colors"].is<JsonArray>()) {
      JsonArray a = o["fill_colors"];
      if (a.size() >= 1) parseHexColor(a[0], e.fillC[0]);
      if (a.size() >= 2) parseHexColor(a[1], e.fillC[1]);
    }
    if (o["border_color"]) parseHexColor(o["border_color"], e.borderC);
    return true;
  }
  if (!strcmp(type, "image") || !strcmp(type, "animation")) {
    e.type = (!strcmp(type, "image")) ? ElemType::Image : ElemType::Anim;
    e.loop = o["loop"] | false;
    const char* path = o["path"];
    const char* stock = o["stock_path"];
    if (path && stock) return false;
    if (path) {
      if (!sanePath(path)) return false;
      snprintf(e.path, sizeof(e.path), "/user_assets/%s/%s", app, path);
    } else if (stock) {
      if (!sanePath(stock)) return false;
      snprintf(e.path, sizeof(e.path), "/stock/%s", stock);
    } else if (e.type == ElemType::Image) {
      return false;
    }
    return true;
  }
  if (!strcmp(type, "xpmbitmap")) {
    e.type = ElemType::Xpm;
    const char* data = o["data"];
    if (!data || !*data) return false;
    e.xpm = strdup(data);
    if (!e.xpm) return false;
    return true;
  }
  return false;
}

static void handleDraw() {
  if (!requireAuth()) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body());
  if (err) {
    sendErr(400, "Invalid JSON");
    return;
  }
  const char* app = doc["application_name"];
  if (!app || !saneName(app, 32)) {
    sendErr(400, "Missing application_name");
    return;
  }
  int priority = doc["priority"] | DRAW_DEFAULT_PRIORITY;
  if (priority < 1 || priority > CANVAS_MAX_PRIORITY) {
    sendErr(400, "Priority must be 1-100");
    return;
  }
  JsonArray els = doc["elements"];
  if (els.isNull() || els.size() == 0) {
    sendErr(400, "Missing or invalid elements array");
    return;
  }
  if ((int)els.size() > CANVAS_MAX_ELEMENTS) {
    sendErr(400, "Elements number limit exceeded");
    return;
  }
  bool blink = false;
  uint16_t led = 0;
  if (doc["led_notification_color"]) {
    if (!parseHexColor(doc["led_notification_color"], led)) {
      sendErr(400, "Invalid LED notification color");
      return;
    }
    blink = true;
  }
  int nEls = (int)els.size();
  CanvasElem* tmp = (CanvasElem*)calloc(nEls, sizeof(CanvasElem));
  if (!tmp) {
    sendErr(400, "Out of memory");
    return;
  }
  int n = 0;
  int32_t z = 0;
  bool ok = true;
  for (JsonObject o : els) {
    if (!parseOneElement(o, tmp[n], app, z)) {
      ok = false;
      break;
    }
    n++;
  }
  CanvasResult r = CanvasResult::BadParameters;
  if (ok) r = Canvas.show(app, priority, tmp, n, led, blink);
  else {
    for (int i = 0; i < n; i++) if (tmp[i].xpm) free(tmp[i].xpm);
  }
  free(tmp);
  if (!ok) sendErr(400, "Bad request");
  else if (r == CanvasResult::LowPriority) sendErr(409, "Not drawn due to low priority");
  else if (r != CanvasResult::Ok) sendErr(400, "Bad request");
  else sendOk();
}

static void handleClear() {
  if (!requireAuth()) return;
  const char* app = nullptr;
  char appBuf[33] = {0};
  String q = server.arg("application_name");
  const char* ids[CANVAS_MAX_ELEMENTS];
  char idStore[CANVAS_MAX_ELEMENTS][24];
  int nIds = 0;
  if (body().length()) {
    JsonDocument d;
    if (deserializeJson(d, body())) {
      sendErr(400, "Invalid deletion data");
      return;
    }
    if (d["application_name"]) {
      strlcpy(appBuf, d["application_name"], sizeof(appBuf));
      app = appBuf;
    }
    if (d["element_ids"].is<JsonArray>()) {
      for (JsonVariant v : d["element_ids"].as<JsonArray>()) {
        if (nIds >= CANVAS_MAX_ELEMENTS) break;
        strlcpy(idStore[nIds], v.as<const char*>(), 24);
        ids[nIds] = idStore[nIds];
        nIds++;
      }
    }
  }
  if (q.length()) {
    strlcpy(appBuf, q.c_str(), sizeof(appBuf));
    app = appBuf;
  }
  CanvasResult r = Canvas.clearApp(app, nIds ? ids : nullptr, nIds);
  if (r == CanvasResult::NonexistentElementId) sendErr(400, "Unknown element id");
  else if (r == CanvasResult::WrongAppId) sendErr(400, "Wrong application_name");
  else sendOk();
}

static void ensureDir(const char* path) {
  char tmp[96];
  strlcpy(tmp, path, sizeof(tmp));
  for (char* p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (!LittleFS.exists(tmp)) LittleFS.mkdir(tmp);
      *p = '/';
    }
  }
}

static void handleAssetUpload() {
  if (!requireAuth()) return;
  String app = server.arg("application_name");
  String file = server.arg("file");
  if (!saneName(app.c_str(), 32) || !sanePath(file.c_str())) {
    sendErr(400, "Invalid parameters");
    return;
  }
  char path[96];
  snprintf(path, sizeof(path), "/user_assets/%s/%s", app.c_str(), file.c_str());
  ensureDir(path);
  File f = LittleFS.open(path, "w");
  if (!f) {
    sendErr(508, "Failed to write uploaded file");
    return;
  }
  if (server.hasArg("plain")) {
    const String& b = server.arg("plain");
    if (b.length() > 48 * 1024) {
      f.close();
      LittleFS.remove(path);
      sendErr(413, "File too large");
      return;
    }
    f.write((const uint8_t*)b.c_str(), b.length());
  }
  f.close();
  sendOk();
}

static void handleAssetDelete() {
  if (!requireAuth()) return;
  String app = server.arg("application_name");
  if (!saneName(app.c_str(), 32)) {
    sendErr(400, "Invalid request parameters");
    return;
  }
  char dir[64];
  snprintf(dir, sizeof(dir), "/user_assets/%s", app.c_str());
  File root = LittleFS.open(dir);
  if (root && root.isDirectory()) {
    File e = root.openNextFile();
    while (e) {
      char p[96];
      snprintf(p, sizeof(p), "%s/%s", dir, e.name());
      e.close();
      LittleFS.remove(p);
      e = root.openNextFile();
    }
  }
  LittleFS.rmdir(dir);
  sendOk();
}

static void handleConfigGet() {
  if (!requireAuth()) return;
  cors();
  JsonDocument d;
  d["theme"] = Settings.d.theme;
  d["hour12"] = Settings.d.hour12;
  d["show_seconds"] = Settings.d.showSeconds;
  d["blink_colons"] = Settings.d.blinkColons;
  d["tz_name"] = Settings.d.tzName;
  d["brightness_mode"] = Settings.d.brightnessMode == BrightnessMode::Auto ? "auto" : "manual";
  d["brightness"] = Settings.d.brightness;
  d["wifi_ssid"] = Settings.d.wifiSsid;
  d["ip"] = WifiSetup::ip().toString();
  d["connected"] = WifiSetup::connected();
  d["has_token"] = Settings.d.apiToken[0] != 0;
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleConfigPut() {
  if (!requireAuth()) return;
  JsonDocument d;
  if (deserializeJson(d, body())) {
    sendErr(400, "Invalid JSON");
    return;
  }
  if (d["theme"]) ThemeApp::setById(d["theme"]);
  if (!d["hour12"].isNull()) Settings.d.hour12 = d["hour12"];
  if (!d["show_seconds"].isNull()) Settings.d.showSeconds = d["show_seconds"];
  if (!d["blink_colons"].isNull()) Settings.d.blinkColons = d["blink_colons"];
  if (d["tz_name"]) {
    Settings.setTz(d["tz_name"]);
    TimeSync::applyTz();
  }
  if (d["api_token"].is<const char*>())
    strlcpy(Settings.d.apiToken, d["api_token"], sizeof(Settings.d.apiToken));
  Settings.save();
  sendOk();
}

static void handleThemes() {
  if (!requireAuth()) return;
  cors();
  int n = 0;
  const ThemeInfo* list = ThemeApp::list(n);
  JsonDocument d;
  JsonArray arr = d["themes"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = list[i].id;
    o["label"] = list[i].label;
  }
  d["current"] = ThemeApp::current()->id;
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleThemeSet() {
  if (!requireAuth()) return;
  String name = server.arg("name");
  if (!name.length()) {
    JsonDocument d;
    if (!deserializeJson(d, body()) && d["name"]) name = d["name"].as<String>();
  }
  if (!name.length()) {
    sendErr(400, "Missing name");
    return;
  }
  ThemeApp::setById(name.c_str());
  Settings.save();
  Apps.switchTo(AppId::Theme, false);
  sendOk();
}

static const char FALLBACK_HTML[] PROGMEM = R"html(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>CYD BUSY</title>
<style>
:root{--bg:#08090d;--surface:#0f1117;--text:#f1f5f9;--muted:#94a3b8;--accent:#F4620E;--border:rgba(255,255,255,.08);--radius:10px}
body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--text)}
main{max-width:720px;margin:0 auto;padding:24px}
h1{font-size:1.2rem}
textarea,input,select,button{font:inherit;background:var(--surface);color:var(--text);border:1px solid var(--border);border-radius:6px;padding:8px}
button{background:var(--accent);border:0;color:#fff}
canvas{image-rendering:pixelated;background:#000;border-radius:6px}
</style>
<h1>CYD BUSY</h1>
<p>LittleFS web UI missing. Flash with <code>pio run -t uploadfs</code>. Draw API still works.</p>
<p><a href="/api/status">/api/status</a></p>
)html";

static bool streamFile(const char* path, const char* mime) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  cors();
  server.streamFile(f, mime);
  f.close();
  return true;
}

static void handleRoot() {
  if (streamFile("/www/index.html", "text/html")) return;
  cors();
  server.send_P(200, "text/html", FALLBACK_HTML);
}

static void handleCss() {
  if (!streamFile("/www/style.css", "text/css")) server.send(404, "text/plain", "no");
}
static void handleJs() {
  if (!streamFile("/www/app.js", "application/javascript")) server.send(404, "text/plain", "no");
}

void HttpApi::begin() {
  server.collectHeaders(HDRS, 3);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCss);
  server.on("/app.js", HTTP_GET, handleJs);

  server.on("/api/version", HTTP_GET, handleVersion);
  server.on("/api/transport", HTTP_GET, handleTransport);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/status/device", HTTP_GET, handleStatus);
  server.on("/api/time", HTTP_GET, handleTimeGet);
  server.on("/api/time", HTTP_PUT, handleTimePut);
  server.on("/api/time/timestamp", HTTP_POST, handleTimePut);
  server.on("/api/time/timezone", HTTP_GET, handleTzGet);
  server.on("/api/time/timezone", HTTP_POST, handleTzSet);
  server.on("/api/time/tzlist", HTTP_GET, handleTzList);
  server.on("/api/display/brightness", HTTP_GET, handleBrightnessGet);
  server.on("/api/display/brightness", HTTP_POST, handleBrightnessSet);
  server.on("/api/display/brightness", HTTP_PUT, handleBrightnessSet);
  server.on("/api/display/draw", HTTP_POST, handleDraw);
  server.on("/api/display/draw", HTTP_DELETE, handleClear);
  server.on("/api/screen", HTTP_GET, handleScreen);
  server.on("/api/input", HTTP_POST, handleInput);
  server.on("/api/assets/upload", HTTP_POST, handleAssetUpload);
  server.on("/api/assets/upload", HTTP_DELETE, handleAssetDelete);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_PUT, handleConfigPut);
  server.on("/api/themes", HTTP_GET, handleThemes);
  server.on("/api/theme", HTTP_POST, handleThemeSet);

  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) { handleOptions(); return; }
    cors();
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
  server.begin();
}

void HttpApi::poll() { server.handleClient(); }
