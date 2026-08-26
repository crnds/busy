#include <Arduino.h>
#include <LittleFS.h>
#include "display/DisplayHAL.h"
#include "display/Backlight.h"
#include "canvas/Canvas.h"
#include "apps/AppManager.h"
#include "apps/Chrome.h"
#include "apps/TouchInput.h"
#include "settings/Settings.h"
#include "net/WifiSetup.h"
#include "net/Mdns.h"
#include "net/TimeSync.h"
#include "api/HttpApi.h"
#include "Version.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s %s  API %s\n", FW_NAME, FW_VERSION, API_SEMVER);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  Settings.begin();
  Display.begin();
  if (Settings.d.rotation == 3) Display.tft.setRotation(3);
  Chrome::boot();
  TouchInput::begin();
  Canvas.begin();
  Apps.begin();

  WifiSetup::begin();
  if (!WifiSetup::inPortal()) {
    TimeSync::begin();
    Mdns::begin();
    HttpApi::begin();
    Serial.printf("http://%s/  (cyd-busybar.local)\n",
                  WifiSetup::ip().toString().c_str());
  } else {
    Serial.printf("Wi-Fi portal AP %s  http://192.168.4.1/\n", WifiSetup::apName());
  }
}

void loop() {
  WifiSetup::poll();
  if (!WifiSetup::inPortal()) HttpApi::poll();
  TouchInput::poll();
  Backlight.poll();
  Apps.tick();
  Display.tick();
}
