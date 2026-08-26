#include "apps/Chrome.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "display/Backlight.h"
#include "net/WifiSetup.h"
#include "canvas/Canvas.h"
#include "Version.h"

void Chrome::boot() {
  Display.clear(DisplayId::Front, COL_BG);
  Display.clear(DisplayId::Back, COL_BG);
  Display.composite();
  Backlight.begin();

  for (int x = 0; x <= FRONT_W; x += 4) {
    Display.clear(DisplayId::Front, COL_BG);
    Display.fillRect(DisplayId::Front, 0, 0, x, FRONT_H, COL_ACCENT);
    Display.clear(DisplayId::Back, COL_BG);
    Display.fillRect(DisplayId::Back, 0, 0, x * BACK_W / FRONT_W, 8, COL_ACCENT);
    Fonts::drawText(DisplayId::Back, 8, 28, "cyd-busybar", COL_TEXT, FontId::Large);
    Fonts::drawText(DisplayId::Back, 8, 44, FW_VERSION, COL_MUTED, FontId::Small);
    Display.composite();
    delay(18);
  }
  Display.clear(DisplayId::Front, COL_BG);
  int tw = Fonts::textWidth("BUSY", FontId::Bold);
  Fonts::drawText(DisplayId::Front, (FRONT_W - tw) / 2, 4, "BUSY", COL_TEXT, FontId::Bold);
  Display.composite();
  delay(400);
}

void Chrome::drawAppsScreen() {
  Display.clear(DisplayId::Front, COL_BG);
  Fonts::drawText(DisplayId::Front, 4, 4, "APPS", COL_ACCENT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 4, "APPS", COL_MUTED, FontId::Small);

  char line[48];
  if (WifiSetup::connected()) {
    snprintf(line, sizeof(line), "http://%s/", WifiSetup::ip().toString().c_str());
  } else if (WifiSetup::inPortal()) {
    snprintf(line, sizeof(line), "http://192.168.4.1/");
  } else {
    snprintf(line, sizeof(line), "offline");
  }
  Fonts::drawText(DisplayId::Back, 4, 18, line, COL_TEXT, FontId::Tiny);
  Fonts::drawText(DisplayId::Back, 4, 30, "cyd-busybar.local", COL_MUTED, FontId::Small);

  if (Canvas.ownsScreen()) {
    snprintf(line, sizeof(line), "canvas: %s p%d", Canvas.appId(), Canvas.priority());
  } else {
    snprintf(line, sizeof(line), "canvas: idle");
  }
  Fonts::drawText(DisplayId::Back, 4, 44, line, COL_ACCENT, FontId::Tiny);
  Fonts::drawText(DisplayId::Back, 4, 54, "POST /api/display/draw", COL_MUTED, FontId::Tiny);
}

void Chrome::drawMessage(const char* front, const char* back) {
  Display.clear(DisplayId::Front, COL_BG);
  int tw = Fonts::textWidth(front, FontId::Bold);
  Fonts::drawText(DisplayId::Front, (FRONT_W - tw) / 2, 4, front, COL_TEXT, FontId::Bold);
  Display.clear(DisplayId::Back, COL_BG);
  Fonts::drawText(DisplayId::Back, 4, 24, back, COL_TEXT, FontId::Normal);
}
