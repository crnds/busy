// Settings.h — persisted to /config.json on LittleFS.
//
// EXCEPT ssid/pass: those live in NVS (see Net.h), not here, because
// `pio run -t uploadfs` overwrites this entire file's storage on every
// filesystem reflash and Wi-Fi credentials are the one thing that should
// survive it. The fields stay in this struct -- everything already reads
// CFG.ssid/CFG.pass -- wifiBegin() is just what populates them now.
#pragma once

#include <stdint.h>
#include "../../include/config.h"

struct Config {
    char    ssid[33];        // NOT round-tripped through /config.json -- see above
    char    pass[65];        // ditto
    char    host[32];
    char    tz[48];          // POSIX TZ string, e.g. "ICT-7"
    char    theme[24];       // active status theme folder name
    char    apiToken[33];    // empty = open mode
    bool    clock24;
    bool    night;
    uint8_t blMode;          // 0 = auto (LDR), 1..4 = 25/50/75/100 %
};

extern Config CFG;

void settingsBegin();        // mounts LittleFS and loads, or writes defaults
bool settingsSave();
void settingsDefaults();

// Backlight duty for the current mode, given the last LDR reading.
uint8_t settingsDutyFor(uint16_t ldrRaw);
const char *settingsBrightnessLabel();
