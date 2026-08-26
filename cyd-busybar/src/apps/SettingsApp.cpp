#include "apps/SettingsApp.h"
#include "apps/AppManager.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "display/Backlight.h"
#include "net/WifiSetup.h"
#include "net/TimeSync.h"
#include "settings/Settings.h"
#include "Version.h"
#include <WiFi.h>

static const char* kRows[] = {"Wi-Fi", "Time", "Brightness", "About"};
static const int kN = 4;

int SettingsApp::rowCount() { return kN; }
const char* SettingsApp::rowLabel(int i) { return kRows[i]; }

static void drawList() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "SETTINGS", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 3, "SETTINGS", COL_MUTED, FontId::Small);
  int sel = Apps.settingsIndex();
  for (int i = 0; i < kN; i++) {
    int y = 16 + i * 12;
    if (i == sel) Display.fillRect(DisplayId::Back, 2, y - 1, BACK_W - 4, 11, rgb565(0x28, 0x18, 0x08));
    Fonts::drawText(DisplayId::Back, 8, y, kRows[i], i == sel ? COL_ACCENT : COL_TEXT, FontId::Normal);
    Fonts::drawText(DisplayId::Back, BACK_W - 10, y, ">", i == sel ? COL_ACCENT : COL_MUTED, FontId::Normal);
  }
}

static void drawWifiLeaf() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "WIFI", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 4, "Wi-Fi", COL_MUTED, FontId::Small);
  char line[40];
  if (WifiSetup::connected()) {
    snprintf(line, sizeof(line), "SSID %s", Settings.d.wifiSsid);
    Fonts::drawText(DisplayId::Back, 4, 18, line, COL_TEXT, FontId::Small);
    snprintf(line, sizeof(line), "IP %s", WifiSetup::ip().toString().c_str());
    Fonts::drawText(DisplayId::Back, 4, 30, line, COL_TEXT, FontId::Small);
    Fonts::drawText(DisplayId::Back, 4, 48, "OK: forget  BACK: exit", COL_MUTED, FontId::Tiny);
  } else if (WifiSetup::inPortal()) {
    snprintf(line, sizeof(line), "AP %s", WifiSetup::apName());
    Fonts::drawText(DisplayId::Back, 4, 18, line, COL_TEXT, FontId::Small);
    Fonts::drawText(DisplayId::Back, 4, 30, "Join AP, open 192.168.4.1", COL_MUTED, FontId::Tiny);
  } else {
    Fonts::drawText(DisplayId::Back, 4, 18, "Not connected", COL_TEXT, FontId::Normal);
    Fonts::drawText(DisplayId::Back, 4, 48, "OK: start portal", COL_MUTED, FontId::Tiny);
  }
}

static void drawTimeLeaf() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "TIME", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 4, "Time", COL_MUTED, FontId::Small);
  char line[40];
  TimeSync::formatIso(line, sizeof(line));
  Fonts::drawText(DisplayId::Back, 4, 16, line, COL_TEXT, FontId::Tiny);
  snprintf(line, sizeof(line), "TZ  %s", Settings.d.tzName);
  Fonts::drawText(DisplayId::Back, 4, 28, line, COL_TEXT, FontId::Small);
  snprintf(line, sizeof(line), "fmt %s", Settings.d.hour12 ? "12-hour" : "24-hour");
  Fonts::drawText(DisplayId::Back, 4, 40, line, COL_TEXT, FontId::Small);
  Fonts::drawText(DisplayId::Back, 4, 54, "OK: 12/24  UP/DN: TZ", COL_MUTED, FontId::Tiny);
}

static void drawBrightLeaf() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "BRIGHT", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 4, "Brightness", COL_MUTED, FontId::Small);
  char line[40];
  if (Backlight.mode() == BrightnessMode::Auto) {
    snprintf(line, sizeof(line), "auto  LDR %d  pwm %d", Backlight.ldrRaw(), Backlight.duty());
  } else {
    snprintf(line, sizeof(line), "manual  %d%%", Settings.d.brightness);
  }
  Fonts::drawText(DisplayId::Back, 4, 20, line, COL_TEXT, FontId::Small);
  int pct = (Backlight.mode() == BrightnessMode::Auto) ? Backlight.percent() : Settings.d.brightness;
  Display.fillRect(DisplayId::Back, 4, 36, 152, 8, rgb565(0x20, 0x20, 0x28));
  Display.fillRect(DisplayId::Back, 4, 36, pct * 152 / 100, 8, COL_ACCENT);
  Fonts::drawText(DisplayId::Back, 4, 50, "OK: auto/man  UP/DN: +/-", COL_MUTED, FontId::Tiny);
}

static void drawAboutLeaf() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, FW_NAME, COL_ACCENT, FontId::Small);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 4, "About", COL_MUTED, FontId::Small);
  char line[48];
  snprintf(line, sizeof(line), "%s %s", FW_NAME, FW_VERSION);
  Fonts::drawText(DisplayId::Back, 4, 16, line, COL_TEXT, FontId::Small);
  snprintf(line, sizeof(line), "API %s", API_SEMVER);
  Fonts::drawText(DisplayId::Back, 4, 28, line, COL_TEXT, FontId::Small);
  snprintf(line, sizeof(line), "heap %u", (unsigned)ESP.getFreeHeap());
  Fonts::drawText(DisplayId::Back, 4, 40, line, COL_MUTED, FontId::Small);
  Fonts::drawText(DisplayId::Back, 4, 52, "CYD 2.8  ESP32-2432S028R", COL_MUTED, FontId::Tiny);
}

void SettingsApp::draw() {
  if (!Apps.inLeaf()) {
    drawList();
    return;
  }
  switch (Apps.settingsIndex()) {
    case 0: drawWifiLeaf(); break;
    case 1: drawTimeLeaf(); break;
    case 2: drawBrightLeaf(); break;
    default: drawAboutLeaf(); break;
  }
}

void SettingsApp::handleKey(Key k) {
  if (!Apps.inLeaf()) {
    if (k == Key::Down) Apps.setSettingsIndex((Apps.settingsIndex() + 1) % kN);
    else if (k == Key::Up) Apps.setSettingsIndex((Apps.settingsIndex() + kN - 1) % kN);
    else if (k == Key::Ok) Apps.setLeaf(true);
    else if (k == Key::Back) Apps.switchTo(AppId::Clock);
    return;
  }
  if (k == Key::Back) {
    Apps.setLeaf(false);
    Settings.save();
    return;
  }
  int row = Apps.settingsIndex();
  if (row == 0) {
    if (k == Key::Ok) {
      if (WifiSetup::connected()) WifiSetup::forget();
      else WifiSetup::startPortal();
    }
  } else if (row == 1) {
    if (k == Key::Ok) {
      Settings.d.hour12 = !Settings.d.hour12;
    } else if (k == Key::Up || k == Key::Down) {
      size_t n = 0;
      const TzEntry* list = Settings.tzList(n);
      int idx = 0;
      for (size_t i = 0; i < n; i++) if (!strcmp(list[i].name, Settings.d.tzName)) idx = (int)i;
      idx = (k == Key::Up) ? (idx + 1) % (int)n : (idx + (int)n - 1) % (int)n;
      Settings.setTz(list[idx].name);
      TimeSync::applyTz();
    }
  } else if (row == 2) {
    if (k == Key::Ok) {
      auto m = Backlight.mode() == BrightnessMode::Auto ? BrightnessMode::Manual
                                                        : BrightnessMode::Auto;
      Backlight.setMode(m);
    } else if (k == Key::Up) {
      Backlight.setMode(BrightnessMode::Manual);
      Backlight.setManual((uint8_t)clampi(Settings.d.brightness + 5, 5, 100));
    } else if (k == Key::Down) {
      Backlight.setMode(BrightnessMode::Manual);
      Backlight.setManual((uint8_t)clampi(Settings.d.brightness - 5, 5, 100));
    }
  }
}
