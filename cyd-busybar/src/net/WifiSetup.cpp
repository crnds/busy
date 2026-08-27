#include "Net.h"
#include "../settings/Settings.h"
#include "../../include/config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <string.h>

static NetState  s_state    = NET_CONNECTING;
static uint32_t  s_deadline = 0;
static uint32_t  s_retryAt  = 0;

// ── CREDENTIALS (NVS) ────────────────────────────────────────────────────
// A namespace of its own, so nothing else sharing the NVS partition can
// collide with these two keys. WiFi.persistent(false) is set in wifiBegin(),
// so the Arduino WiFi stack never writes its own copy here either.
static Preferences s_prefs;
static const char *CREDS_NS = "wifi";

static void credsLoad() {
    // Read-write, not read-only, even though this never writes: opening a
    // namespace that has never existed in READ-ONLY mode is a genuine
    // nvs_open() failure and the IDF logs it at error level regardless of
    // Preferences::begin()'s own return-value check -- on every cold boot of
    // a device that has never been given credentials, forever. Read-write
    // mode creates an empty namespace on that same first call instead, which
    // is a harmless few-byte write and the only way to make "nothing stored
    // yet" not look like a fault in the log.
    if (!s_prefs.begin(CREDS_NS, false)) return;   // nothing stored yet: CFG stays zeroed
    // getString() on a key that has never been written hits the identical
    // nvs_get_str NOT_FOUND path, logged the identical way -- isKey() sidesteps
    // it instead of just moving the noise down one call. Both keys are always
    // written together (wifiSetCreds()/credsClear() never touch just one), so
    // checking "ssid" alone is enough to gate both reads.
    String ssid, pass;
    if (s_prefs.isKey("ssid")) {
        ssid = s_prefs.getString("ssid", "");
        pass = s_prefs.getString("pass", "");
    }
    s_prefs.end();
    strncpy(CFG.ssid, ssid.c_str(), sizeof(CFG.ssid) - 1); CFG.ssid[sizeof(CFG.ssid) - 1] = '\0';
    strncpy(CFG.pass, pass.c_str(), sizeof(CFG.pass) - 1); CFG.pass[sizeof(CFG.pass) - 1] = '\0';
    if (CFG.ssid[0]) {
        Serial.printf("[net] credentials found: \"%s\"\n", CFG.ssid);
    } else {
        Serial.println("[net] credentials none stored");
    }
}

bool wifiSetCreds(const char *ssid, const char *pass) {
    if (!s_prefs.begin(CREDS_NS, false)) {
        Serial.println("[net] NVS open failed, credentials NOT saved");
        return false;
    }
    const char *s = ssid ? ssid : "";
    const char *p = pass ? pass : "";
    s_prefs.putString("ssid", s);
    s_prefs.putString("pass", p);
    String readSsid = s_prefs.getString("ssid", "");
    String readPass = s_prefs.getString("pass", "");
    s_prefs.end();

    if (readSsid != s || readPass != p) {
        Serial.printf("[net] credentials NOT saved: readback mismatch for \"%s\"\n", s);
        return false;
    }

    strncpy(CFG.ssid, s, sizeof(CFG.ssid) - 1); CFG.ssid[sizeof(CFG.ssid) - 1] = '\0';
    strncpy(CFG.pass, p, sizeof(CFG.pass) - 1); CFG.pass[sizeof(CFG.pass) - 1] = '\0';
    Serial.printf("[net] credentials saved for \"%s\"\n", CFG.ssid);
    return true;
}

static void credsClear() {
    if (s_prefs.begin(CREDS_NS, false)) {
        s_prefs.clear();
        s_prefs.end();
    }
    CFG.ssid[0] = '\0';
    CFG.pass[0] = '\0';
}

// ── SETUP ACCESS POINT ───────────────────────────────────────────────────
static DNSServer s_dns;
static bool      s_apUp       = false;
static bool      s_apFallback = false;   // no credentials: this is the only way in
static uint32_t  s_apUntil    = 0;       // 0 = never closes

// The station and the AP share one radio. Choosing the mode from whether
// credentials exist -- rather than from whether the station happens to be
// connected right now -- keeps a reconnect from tearing the AP down mid-setup.
static void applyMode() {
    if (s_apUp) WiFi.mode(CFG.ssid[0] ? WIFI_AP_STA : WIFI_AP);
    else        WiFi.mode(WIFI_STA);
}

void apStart(bool timed) {
    s_apFallback = !CFG.ssid[0];
    s_apUp       = true;
    s_apUntil    = (timed && !s_apFallback) ? (millis() + AP_TIMEOUT_MS) : 0;

    applyMode();
    WiFi.softAP(AP_SSID);
    s_dns.start(53, "*", WiFi.softAPIP());

    Serial.printf("[net] AP up: %s at %s (%s)\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str(),
                  s_apUntil ? "times out" : "no timeout");
}

void apStop() {
    if (!s_apUp) return;
    // The fallback AP is the only route in, so refuse to close it. A caller
    // that could would be a caller that could strand the device.
    if (s_apFallback) { Serial.println("[net] AP stays up: no credentials stored"); return; }

    s_dns.stop();
    WiFi.softAPdisconnect(true);
    s_apUp    = false;
    s_apUntil = 0;
    applyMode();
    Serial.println("[net] AP down");
}

bool        apActive()      { return s_apUp; }
bool        apIsFallback()  { return s_apUp && s_apFallback; }
const char *apSsid()        { return AP_SSID; }
IPAddress   apIP()          { return WiFi.softAPIP(); }

uint32_t apMsRemaining() {
    if (!s_apUp || !s_apUntil) return 0;
    int32_t left = (int32_t)(s_apUntil - millis());
    return left > 0 ? (uint32_t)left : 0;
}

// ── STATION ──────────────────────────────────────────────────────────────
static void joinStation(uint32_t now) {
    WiFi.setHostname(CFG.host);
    WiFi.begin(CFG.ssid, CFG.pass);
    s_state    = NET_CONNECTING;
    s_deadline = now + WIFI_CONNECT_MS;
    Serial.printf("[net] joining %s\n", CFG.ssid);
}

void wifiBegin() {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    // NVS, not /config.json -- this is what makes credentials survive a
    // reflash of either the app or the filesystem, and it has to run before
    // anything below looks at CFG.ssid.
    credsLoad();

    if (!CFG.ssid[0]) {
        apStart(false);          // fallback: the AP is the only way in
        s_state = NET_PORTAL;
        return;
    }
    applyMode();
    joinStation(millis());
}

void wifiReconnect() {
    if (!CFG.ssid[0]) return;
    WiFi.disconnect();
    joinStation(millis());
}

void wifiForget() {
    credsClear();
    WiFi.disconnect();
    // Call apStart() unconditionally rather than only when the AP is not
    // already up. If it WAS already up -- raised manually while still
    // connected -- its s_apFallback flag was computed against the credentials
    // that just vanished, and is now wrong: it reads "not fallback", so
    // apStop() would let it be closed, which is exactly the state that would
    // strand the device. apStart() recomputes s_apFallback from the current
    // (now empty) CFG.ssid, correcting it to true either way.
    apStart(false);
    s_state = NET_PORTAL;
}

void wifiTick(uint32_t now) {
    if (s_apUp) {
        s_dns.processNextRequest();
        if (s_apUntil && (int32_t)(now - s_apUntil) >= 0) apStop();
    }

    // With no credentials there is nothing to join, so the state stays PORTAL
    // and the AP stays up.
    if (!CFG.ssid[0]) { s_state = NET_PORTAL; return; }

    if (WiFi.status() == WL_CONNECTED) {
        if (s_state != NET_ONLINE) {
            s_state = NET_ONLINE;
            Serial.printf("[net] online at %s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    if (s_state == NET_ONLINE) {
        // Dropped. Keep trying rather than raising the AP: doing that would
        // take the device off the network it is being looked for on.
        s_state    = NET_CONNECTING;
        s_deadline = now + WIFI_CONNECT_MS;
        return;
    }

    if ((int32_t)(now - s_deadline) >= 0 && (int32_t)(now - s_retryAt) >= 0) {
        Serial.println("[net] join timed out, retrying");
        WiFi.disconnect();
        joinStation(now);
        s_retryAt = now + WIFI_CONNECT_MS;
    }
}

NetState wifiState() { return s_state; }

IPAddress wifiIP() {
    if (s_state == NET_ONLINE) return WiFi.localIP();
    if (s_apUp)                return WiFi.softAPIP();
    return WiFi.localIP();
}

// ── SCAN ─────────────────────────────────────────────────────────────────
// Asynchronous throughout. WiFi.scanNetworks() blocks for two to four seconds,
// and the only two places that would call it are loop() -- which owns the
// render budget -- and the async web task.
static ScanState s_scan = SCAN_IDLE;

void scanStart() {
    if (s_scan == SCAN_RUNNING) return;
    WiFi.scanDelete();
    s_scan = SCAN_RUNNING;
    WiFi.scanNetworks(true, true);   // async, include hidden
}

ScanState scanState() {
    if (s_scan == SCAN_RUNNING) {
        int n = WiFi.scanComplete();
        if (n >= 0)                    s_scan = SCAN_DONE;
        else if (n == WIFI_SCAN_FAILED) s_scan = SCAN_IDLE;
    }
    return s_scan;
}

int scanCount() {
    int n = WiFi.scanComplete();
    return n > 0 ? n : 0;
}

bool scanResult(int i, char *ssid, size_t n, int32_t &rssi, bool &secure) {
    if (i < 0 || i >= scanCount()) return false;
    String s = WiFi.SSID(i);
    strncpy(ssid, s.c_str(), n - 1);
    ssid[n - 1] = '\0';
    rssi   = WiFi.RSSI(i);
    secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    return true;
}
