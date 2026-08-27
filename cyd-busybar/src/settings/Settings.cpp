#include "Settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

Config CFG;

static const char *PATH = "/config.json";

void settingsDefaults() {
    memset(&CFG, 0, sizeof(CFG));
    strcpy(CFG.host, DEFAULT_HOSTNAME);
    strcpy(CFG.tz, "UTC0");
    strcpy(CFG.theme, "busy");
    CFG.clock24 = true;
    CFG.night   = false;
    CFG.blMode  = 0;          // auto
}

static void loadFrom(JsonObjectConst o) {
    auto cp = [](char *d, size_t n, const char *s) {
        if (!s) return; strncpy(d, s, n - 1); d[n - 1] = '\0';
    };
    // ssid/pass are NOT read here -- they live in NVS. wifiBegin() populates
    // them after this runs, and any "ssid"/"pass" key surviving in an old
    // config.json is simply never looked at again.
    cp(CFG.host,  sizeof(CFG.host),  o["host"]  | (const char *)nullptr);
    cp(CFG.tz,    sizeof(CFG.tz),    o["tz"]    | (const char *)nullptr);
    cp(CFG.theme, sizeof(CFG.theme), o["theme"] | (const char *)nullptr);
    cp(CFG.apiToken, sizeof(CFG.apiToken), o["api_token"] | (const char *)nullptr);
    CFG.clock24 = o["clock24"] | CFG.clock24;
    CFG.night   = o["night"]   | CFG.night;
    CFG.blMode  = (uint8_t)(o["bl_mode"] | (int)CFG.blMode);
    if (CFG.blMode > 4) CFG.blMode = 0;
}

void settingsBegin() {
    settingsDefaults();
    if (!LittleFS.begin(true)) {
        Serial.println("[cfg] LittleFS mount failed, running on defaults");
        return;
    }
    File f = LittleFS.open(PATH, "r");
    if (!f) {
        Serial.println("[cfg] no config.json, writing defaults");
        settingsSave();
        return;
    }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, f);
    f.close();
    if (e) {
        Serial.printf("[cfg] config.json is not valid JSON (%s), using defaults\n", e.c_str());
        return;
    }
    loadFrom(doc.as<JsonObjectConst>());
    Serial.printf("[cfg] loaded: host=%s tz=%s theme=%s\n", CFG.host, CFG.tz, CFG.theme);
}

bool settingsSave() {
    // ssid/pass are NOT written here -- see loadFrom(). Writing them into a
    // file that -t uploadfs overwrites every reflash would defeat the entire
    // point of storing them in NVS instead.
    JsonDocument doc;
    doc["host"]      = CFG.host;
    doc["tz"]        = CFG.tz;
    doc["theme"]     = CFG.theme;
    doc["api_token"] = CFG.apiToken;
    doc["clock24"]   = CFG.clock24;
    doc["night"]     = CFG.night;
    doc["bl_mode"]   = CFG.blMode;

    File f = LittleFS.open(PATH, "w");
    if (!f) { Serial.println("[cfg] save failed: cannot open"); return false; }
    bool ok = serializeJsonPretty(doc, f) > 0;
    f.close();
    return ok;
}

const char *settingsBrightnessLabel() {
    switch (CFG.blMode) {
        case 1: return "25%";
        case 2: return "50%";
        case 3: return "75%";
        case 4: return "100%";
        default: return "Auto";
    }
}

uint8_t settingsDutyFor(uint16_t ldrRaw) {
    // Night mode overrides everything: red at 1% is the whole point.
    if (CFG.night) return BL_MIN_DUTY;

    if (CFG.blMode) {
        static const uint8_t STEP[4] = { 64, 128, 192, 255 };
        return STEP[CFG.blMode - 1];
    }

    // Auto. The CYD's LDR sits in a divider that reads HIGH in the dark, so the
    // curve is inverted against the raw ADC. Verified on hardware -- board
    // revisions differ here, and this is the line to flip if yours does.
    uint32_t dark = ldrRaw;                    // 0..4095, larger = darker
    if (dark > 4095) dark = 4095;
    uint32_t light = 4095 - dark;              // larger = brighter room
    // Gamma 2.0, so the low end has resolution where the eye does.
    uint32_t duty = (light * light) / (4095u * 4095u / 255u);
    if (duty < 24)  duty = 24;                 // never so dim it looks dead
    if (duty > 255) duty = 255;
    return (uint8_t)duty;
}
