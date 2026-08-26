#include "apps/AppManager.h"
#include "apps/ClockApp.h"
#include "apps/ThemeApp.h"
#include "apps/SettingsApp.h"
#include "apps/Chrome.h"
#include "canvas/Canvas.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "net/WifiSetup.h"
#include "net/TimeSync.h"
#include "settings/Settings.h"
#include <WiFi.h>

AppManager Apps;

void AppManager::begin() {
  current_ = AppId::Clock;
  ThemeApp::begin();
}

void AppManager::switchTo(AppId id, bool wipe) {
  if (id == current_ && !wiping_) return;
  pending_ = id;
  settingsLeaf_ = false;
  if (id != AppId::Theme) Display.rgbOff();
  if (wipe) {
    wiping_ = true;
    wipeStart_ = millis();
  } else {
    current_ = id;
    Display.markDirty();
  }
}

void AppManager::handleKey(Key k) {
  if (k == Key::None) return;

  if (Canvas.ownsScreen() && (k == Key::Back || k == Key::Off)) {
    Canvas.clearApp(nullptr, nullptr, 0);
    Display.rgbOff();
    Display.markDirty();
    return;
  }

  if (k == Key::Start) { switchTo(AppId::Clock); return; }
  if (k == Key::Busy) { switchTo(AppId::Theme); return; }
  if (k == Key::Apps) { switchTo(AppId::Apps); return; }
  if (k == Key::Settings) { switchTo(AppId::Settings); return; }

  switch (current_) {
    case AppId::Clock: ClockApp::handleKey(k); break;
    case AppId::Theme: ThemeApp::handleKey(k); break;
    case AppId::Settings: SettingsApp::handleKey(k); break;
    default: break;
  }
  Display.markDirty();
}

void AppManager::drawTabBar() {
  const char* labels[4] = {"CLK", "STAT", "APP", "SET"};
  AppId ids[4] = {AppId::Clock, AppId::Theme, AppId::Apps, AppId::Settings};
  int y = BACK_H - 12;
  Display.fillRect(DisplayId::Back, 0, y, BACK_W, 12, rgb565(0x10, 0x12, 0x18));
  for (int i = 0; i < 4; i++) {
    int x = i * 40;
    bool on = (current_ == ids[i]);
    if (on) Display.fillRect(DisplayId::Back, x, y, 40, 12, rgb565(0x30, 0x18, 0x08));
    int tw = Fonts::textWidth(labels[i], FontId::Tiny);
    Fonts::drawText(DisplayId::Back, x + (40 - tw) / 2, y + 3, labels[i],
                    on ? COL_ACCENT : COL_MUTED, FontId::Tiny);
  }
}

void AppManager::draw() {
  if (Canvas.ownsScreen()) {
    Canvas.draw();
    return;
  }
  switch (current_) {
    case AppId::Clock: ClockApp::draw(); break;
    case AppId::Theme: ThemeApp::draw(); break;
    case AppId::Apps: Chrome::drawAppsScreen(); break;
    case AppId::Settings: SettingsApp::draw(); break;
  }
  drawTabBar();
}

void AppManager::tick() {
  Canvas.tick();

  if (wiping_) {
    uint32_t dt = millis() - wipeStart_;
    int x = (int)(dt * TFT_W / 280);
    Display.setWipe(x);
    if (dt > 140) current_ = pending_;
    if (dt > 280) {
      wiping_ = false;
      Display.setWipe(-1);
      current_ = pending_;
    }
    Display.markDirty();
  }

  char clk[16];
  TimeSync::formatClock(clk, sizeof(clk), false, true, false);
  if (!TimeSync::ready()) snprintf(clk, sizeof(clk), "--:--:--");
  char left[24] = "";
  if (WifiSetup::connected()) {
    strlcpy(left, WifiSetup::ip().toString().c_str(), sizeof(left));
  } else if (WifiSetup::inPortal()) {
    strlcpy(left, "AP setup", sizeof(left));
  } else {
    strlcpy(left, "offline", sizeof(left));
  }
  Display.setStatus(left, clk, WifiSetup::connected(), WiFi.RSSI());

  uint32_t now = millis();
  if (now - lastDrawMs_ > 80) {
    lastDrawMs_ = now;
    draw();
  }
}
