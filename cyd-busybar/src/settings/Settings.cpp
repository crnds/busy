#include "settings/Settings.h"
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>

using fs::File;

SettingsStore Settings;

const TzEntry TZ_TABLE[] = {
    {"Bangkok",  "ICT-7",                              "+07:00", "ICT"},
    {"UTC",      "GMT0",                               "+00:00", "UTC"},
    {"London",   "GMT0BST,M3.5.0/1,M10.5.0",           "+00:00", "GMT"},
    {"Berlin",   "CET-1CEST,M3.5.0,M10.5.0/3",         "+01:00", "CET"},
    {"New_York", "EST5EDT,M3.2.0,M11.1.0",             "-05:00", "EST"},
    {"Chicago",  "CST6CDT,M3.2.0,M11.1.0",             "-06:00", "CST"},
    {"Denver",   "MST7MDT,M3.2.0,M11.1.0",             "-07:00", "MST"},
    {"Los_Angeles","PST8PDT,M3.2.0,M11.1.0",           "-08:00", "PST"},
    {"Tokyo",    "JST-9",                              "+09:00", "JST"},
    {"Singapore","SGT-8",                              "+08:00", "SGT"},
    {"Sydney",   "AEST-10AEDT,M10.1.0,M4.1.0/3",       "+10:00", "AEST"},
    {"Mumbai",   "IST-5:30",                           "+05:30", "IST"},
};
const size_t TZ_TABLE_LEN = sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0]);

void SettingsStore::applyDefaults() {
  memset(&d, 0, sizeof(d));
  d.hour12 = true;
  d.showSeconds = true;
  d.blinkColons = true;
  d.showDate = true;
  d.brightnessMode = BrightnessMode::Auto;
  d.brightness = 80;
  d.rotation = 1;
  strlcpy(d.tzName, "Bangkok", sizeof(d.tzName));
  strlcpy(d.tzPosix, "ICT-7", sizeof(d.tzPosix));
  strlcpy(d.theme, "on_air", sizeof(d.theme));
}

void SettingsStore::begin() {
  applyDefaults();
  load();
}

bool SettingsStore::load() {
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (doc["wifi_ssid"]) strlcpy(d.wifiSsid, doc["wifi_ssid"], sizeof(d.wifiSsid));
  if (doc["wifi_pass"]) strlcpy(d.wifiPass, doc["wifi_pass"], sizeof(d.wifiPass));
  if (doc["tz_name"]) strlcpy(d.tzName, doc["tz_name"], sizeof(d.tzName));
  if (doc["tz"]) strlcpy(d.tzPosix, doc["tz"], sizeof(d.tzPosix));
  if (doc["api_token"]) strlcpy(d.apiToken, doc["api_token"], sizeof(d.apiToken));
  if (doc["theme"]) strlcpy(d.theme, doc["theme"], sizeof(d.theme));
  if (!doc["hour12"].isNull()) d.hour12 = doc["hour12"];
  if (!doc["show_seconds"].isNull()) d.showSeconds = doc["show_seconds"];
  if (!doc["blink_colons"].isNull()) d.blinkColons = doc["blink_colons"];
  if (!doc["show_date"].isNull()) d.showDate = doc["show_date"];
  if (doc["brightness_mode"]) {
    const char* m = doc["brightness_mode"];
    d.brightnessMode = (!strcmp(m, "manual")) ? BrightnessMode::Manual : BrightnessMode::Auto;
  }
  if (!doc["brightness"].isNull()) d.brightness = (uint8_t)doc["brightness"].as<int>();
  if (!doc["rotation"].isNull()) d.rotation = (uint8_t)doc["rotation"].as<int>();
  return true;
}

bool SettingsStore::save() {
  JsonDocument doc;
  doc["wifi_ssid"] = d.wifiSsid;
  doc["wifi_pass"] = d.wifiPass;
  doc["tz_name"] = d.tzName;
  doc["tz"] = d.tzPosix;
  doc["api_token"] = d.apiToken;
  doc["theme"] = d.theme;
  doc["hour12"] = d.hour12;
  doc["show_seconds"] = d.showSeconds;
  doc["blink_colons"] = d.blinkColons;
  doc["show_date"] = d.showDate;
  doc["brightness_mode"] = (d.brightnessMode == BrightnessMode::Auto) ? "auto" : "manual";
  doc["brightness"] = d.brightness;
  doc["rotation"] = d.rotation;
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

const TzEntry* SettingsStore::findTz(const char* name) const {
  if (!name) return nullptr;
  for (size_t i = 0; i < TZ_TABLE_LEN; i++) {
    if (!strcasecmp(TZ_TABLE[i].name, name)) return &TZ_TABLE[i];
  }
  return nullptr;
}

const TzEntry* SettingsStore::tzList(size_t& count) const {
  count = TZ_TABLE_LEN;
  return TZ_TABLE;
}

void SettingsStore::setTz(const char* name) {
  const TzEntry* e = findTz(name);
  if (!e) return;
  strlcpy(d.tzName, e->name, sizeof(d.tzName));
  strlcpy(d.tzPosix, e->posix, sizeof(d.tzPosix));
}
