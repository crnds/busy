#include "net/WifiSetup.h"
#include "settings/Settings.h"
#include "apps/Chrome.h"
#include "display/DisplayHAL.h"
#include "canvas/Fonts.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

static DNSServer dns;
static WebServer portal(80);
static bool sPortal = false;
static bool sConnected = false;
static char sApName[24] = "CYD-BusyBar";
static uint32_t sConnectStart = 0;

static String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
  return out;
}

static void drawPortal() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "WIFI SETUP", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 8, "Join Wi-Fi:", COL_MUTED, FontId::Small);
  Fonts::drawText(DisplayId::Back, 4, 20, sApName, COL_TEXT, FontId::Normal);
  Fonts::drawText(DisplayId::Back, 4, 36, "then open", COL_MUTED, FontId::Small);
  Fonts::drawText(DisplayId::Back, 4, 48, "192.168.4.1", COL_ACCENT, FontId::Normal);
  Display.composite();
}

static void handlePortalRoot() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED || n == WIFI_SCAN_RUNNING) {
    WiFi.scanNetworks(true);
    n = 0;
  }
  String html = F("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
                  "<title>CYD BUSY Wi-Fi</title>"
                  "<style>body{font-family:system-ui;background:#08090d;color:#f1f5f9;margin:24px}"
                  "input,select,button{font:inherit;padding:10px;border-radius:6px;width:100%;"
                  "box-sizing:border-box;margin:6px 0}button{background:#F4620E;color:#fff;border:0}</style>"
                  "<h1>CYD BUSY</h1><p>Connect this display to your Wi-Fi.</p>"
                  "<form method=POST action='/save'><label>Network</label><select name=ssid>");
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    html += "<option value=\"";
    html += htmlEscape(ssid);
    html += "\">";
    html += htmlEscape(ssid);
    html += "</option>";
  }
  html += F("</select><p>or type SSID</p><input name=ssid2 placeholder=SSID>"
            "<input type=password name=pass placeholder=Password>"
            "<button>Save and reboot</button></form>");
  portal.send(200, "text/html", html);
}

static void handlePortalSave() {
  String ssid = portal.arg("ssid2");
  if (!ssid.length()) ssid = portal.arg("ssid");
  String pass = portal.arg("pass");
  strlcpy(Settings.d.wifiSsid, ssid.c_str(), sizeof(Settings.d.wifiSsid));
  strlcpy(Settings.d.wifiPass, pass.c_str(), sizeof(Settings.d.wifiPass));
  Settings.save();
  portal.send(200, "text/html", F("<p>Saved. Rebooting...</p>"));
  delay(500);
  ESP.restart();
}

static bool sBegun = false;

void WifiSetup::startPortal() {
  if (sBegun) {
    Settings.d.wifiSsid[0] = 0;
    Settings.save();
    ESP.restart();
    return;
  }
  sPortal = true;
  sConnected = false;
  WiFi.mode(WIFI_AP_STA);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(sApName, sizeof(sApName), "CYD-BusyBar-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(sApName);
  dns.start(53, "*", WiFi.softAPIP());
  portal.on("/", HTTP_GET, handlePortalRoot);
  portal.on("/save", HTTP_POST, handlePortalSave);
  portal.onNotFound(handlePortalRoot);
  portal.begin();
  WiFi.scanNetworks(true);
  drawPortal();
}

void WifiSetup::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("cyd-busybar");
  if (Settings.d.wifiSsid[0] == 0) {
    startPortal();
    sBegun = true;
    return;
  }
  WiFi.begin(Settings.d.wifiSsid, Settings.d.wifiPass);
  sConnectStart = millis();
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "Wi-Fi...", COL_TEXT, FontId::Bold);
  Display.composite();
  while (WiFi.status() != WL_CONNECTED && millis() - sConnectStart < 15000) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    sConnected = true;
    sPortal = false;
  } else {
    startPortal();
  }
  sBegun = true;
}

void WifiSetup::poll() {
  if (sPortal) {
    dns.processNextRequest();
    portal.handleClient();
    return;
  }
  sConnected = WiFi.status() == WL_CONNECTED;
}

bool WifiSetup::connected() { return sConnected; }
bool WifiSetup::inPortal() { return sPortal; }
const char* WifiSetup::apName() { return sApName; }
IPAddress WifiSetup::ip() { return sPortal ? WiFi.softAPIP() : WiFi.localIP(); }

void WifiSetup::forget() {
  Settings.d.wifiSsid[0] = 0;
  Settings.d.wifiPass[0] = 0;
  Settings.save();
  ESP.restart();
}
