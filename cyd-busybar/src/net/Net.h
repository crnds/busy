// Net.h — Wi-Fi, mDNS and time. One header over the three modules the brief
// names (WifiSetup.cpp, Mdns.cpp, TimeSync.cpp) so callers have one include.
#pragma once

#include <stdint.h>
#include <IPAddress.h>

enum NetState : uint8_t {
    NET_CONNECTING = 0,
    NET_ONLINE,
    NET_PORTAL      // no credentials stored: the AP is the only way in
};

// ── WifiSetup.cpp ────────────────────────────────────────────────────────
void      wifiBegin();
void      wifiTick(uint32_t now);
NetState  wifiState();
IPAddress wifiIP();          // the station address, or the AP's in portal mode

// ── Setup access point ───────────────────────────────────────────────────
// The AP is INDEPENDENT of the station: turning it on does not disconnect and
// does not touch stored credentials, so a tap that starts it is fully
// reversible. That is what lets the control live on the device at all --
// wiping credentials behind a single tap would not have been safe, and this
// UI has no confirmation step to protect it with.
//
// Two ways it comes up:
//   - FALLBACK, when no credentials are stored. Never times out; closing it
//     would leave no way to reach the device.
//   - ON REQUEST, from the Settings card or the API. Runs AP+STA and closes
//     itself after AP_TIMEOUT_MS.
void        apStart(bool timed);
void        apStop();
bool        apActive();
bool        apIsFallback();
const char *apSsid();
IPAddress   apIP();
uint32_t    apMsRemaining();     // 0 when it does not time out

// Ask the station to re-join now, without a reboot.
void wifiReconnect();

// ── Credentials ──────────────────────────────────────────────────────────
// Stored in NVS (via the `Preferences` library), a flash PARTITION distinct
// from both the app (what `pio run -t upload` writes) and the LittleFS image
// (what `-t uploadfs` writes) -- so unlike every other setting, which lives in
// /config.json and is expected to reset on a fresh image, these survive a
// REFLASH of either, not only a restart. wifiBegin() loads them from here
// before it looks at CFG.ssid at all.
//
// Pure mutators: neither reboots. PUT /api/wifi must answer its caller before
// it restarts, so that ordering stays at the call site, same as it always
// has; the on-device Forget chip needs no such choreography and just calls
// wifiForget() directly.
bool wifiSetCreds(const char *ssid, const char *pass);   // write NVS + CFG, returns true on verified write
void wifiForget();          // erase NVS + CFG, drop the station, raise the AP as fallback

// ── Wi-Fi scan (asynchronous — a blocking scan would stall the caller) ────
enum ScanState : uint8_t { SCAN_IDLE = 0, SCAN_RUNNING, SCAN_DONE };
void      scanStart();
ScanState scanState();
int       scanCount();
bool      scanResult(int i, char *ssid, size_t n, int32_t &rssi, bool &secure);

// ── Mdns.cpp ─────────────────────────────────────────────────────────────
void mdnsBegin();

// ── TimeSync.cpp ─────────────────────────────────────────────────────────
void timeBegin();
void timeTick(uint32_t now);
bool timeSynced();
void timeResync();
bool timeNow(struct tm &out);
