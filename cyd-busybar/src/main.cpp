// cyd-busybar — a BUSY Bar clone for the CYD 2.8" (ESP32-2432S028R).
//
// The two BUSY Bar panels are emulated at native resolution (72x16 RGB and
// 160x80 mono) and composited onto the CYD's 320x240 ILI9341, under the design
// system in DESIGN.md.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>

#include "../include/config.h"
#include "ui/theme.h"
#include "ui/screen.h"
#include "vdisp/VDisplay.h"
#include "canvas/Canvas.h"
#include "apps/Apps.h"
#include "api/HttpApi.h"
#include "net/Net.h"
#include "hw/Hw.h"
#include "settings/Settings.h"

static TFT_eSPI tft = TFT_eSPI();

static uint32_t s_blNext = 0;

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.printf("\n%s %s\n", FW_NAME, FW_VERSION);

    // Backlight OFF first. Settings must load before the panel lights, or the
    // screen flashes at 100% for the length of the LittleFS mount.
    blBegin();

    settingsBegin();
    themeInit();
    if (CFG.night) themeSetNight(true);

    tft.init();
    tft.setRotation(SCR_ROTATION);
    tft.fillScreen(C_BG);

    screenBegin(&tft);
    vd::begin();
    canvasBegin();
    appBegin();

    ledBegin();
    ldrBegin();
    touchBegin();

    blSet(settingsDutyFor(ldrRaw()));

    wifiBegin();

    // Splash until the network resolves one way or the other. Both outcomes
    // look identical on purpose: the screen never implies the wrong one.
    uint32_t until = millis() + WIFI_CONNECT_MS;
    uint8_t  pip   = 0;
    while (wifiState() == NET_CONNECTING && (int32_t)(millis() - until) < 0) {
        wifiTick(millis());
        chromeSplash(millis(), pip++);
        delay(SPLASH_PIP_MS);
    }

    timeBegin();
    mdnsBegin();
    apiBegin();

    screenInvalidate();
    Serial.printf("[boot] ready, heap %u\n", ESP.getFreeHeap());
}

void loop() {
    uint32_t now = millis();

    wifiTick(now);
    timeTick(now);
    ldrTick(now);
    apiTick(now);

    if ((int32_t)(now - s_blNext) >= 0) {
        s_blNext = now + LDR_PERIOD_MS;
        blSet(settingsDutyFor(ldrRaw()));
    }

    Touch t;
    if (touchPoll(t, now)) screenTouch(t.x, t.y, now);

    appTick(now);
    screenTick(now);

    delay(5);
}
