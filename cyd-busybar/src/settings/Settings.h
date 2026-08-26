#pragma once

#include "Types.h"

struct TzEntry {
  const char* name;
  const char* posix;
  const char* offset;
  const char* abbr;
};

struct SettingsData {
  char wifiSsid[33];
  char wifiPass[65];
  char tzName[24];
  char tzPosix[48];
  char apiToken[49];
  char theme[24];
  bool hour12;
  bool showSeconds;
  bool blinkColons;
  bool showDate;
  BrightnessMode brightnessMode;
  uint8_t brightness;          // 0-100
  uint8_t rotation;            // 1 or 3
};

class SettingsStore {
 public:
  SettingsData d;

  void begin();
  bool load();
  bool save();
  void applyDefaults();
  const TzEntry* findTz(const char* name) const;
  const TzEntry* tzList(size_t& count) const;
  void setTz(const char* name);
};

extern SettingsStore Settings;
extern const TzEntry TZ_TABLE[];
extern const size_t TZ_TABLE_LEN;
