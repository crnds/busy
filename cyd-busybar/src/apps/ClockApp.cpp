#include "apps/ClockApp.h"
#include "apps/AppManager.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "net/TimeSync.h"
#include "settings/Settings.h"

static void drawCalendar(int x, int y, int day) {
  Display.fillRect(DisplayId::Front, x, y, 13, 14, COL_TEXT);
  Display.fillRect(DisplayId::Front, x + 1, y + 3, 11, 10, rgb565(0x20, 0x20, 0x28));
  Display.fillRect(DisplayId::Front, x + 1, y, 11, 3, COL_RED);
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", day);
  int tw = Fonts::textWidth(buf, FontId::Tiny);
  Fonts::drawText(DisplayId::Front, x + (13 - tw) / 2, y + 5, buf, COL_TEXT, FontId::Tiny);
}

void ClockApp::draw() {
  Display.clear(DisplayId::Front, COL_BG);
  Display.clear(DisplayId::Back, COL_BG);

  struct tm t;
  TimeSync::localTm(t);
  bool ready = TimeSync::ready();

  char clock[20];
  TimeSync::formatClock(clock, sizeof(clock), Settings.d.hour12, Settings.d.showSeconds,
                        Settings.d.blinkColons);
  if (!ready) snprintf(clock, sizeof(clock), "--:--");

  if (Settings.d.showDate) drawCalendar(1, 1, ready ? t.tm_mday : 0);

  FontId f = Settings.d.showSeconds ? FontId::Bold : FontId::ExtraLarge;
  int tw = Fonts::textWidth(clock, f);
  int th = Fonts::textHeight(f);
  int x = Settings.d.showDate ? 16 : (FRONT_W - tw) / 2;
  int y = (FRONT_H - th) / 2;
  Fonts::drawText(DisplayId::Front, x, y, clock, COL_TEXT, f);

  if (Settings.d.hour12 && ready) {
    const char* mer = (t.tm_hour >= 12) ? "PM" : "AM";
    Fonts::drawText(DisplayId::Front, x + tw + 2, y + 2, mer, COL_MUTED, FontId::Tiny);
  }

  // Back: big time
  Fonts::drawText(DisplayId::Back, 4, 4, "CLOCK", COL_MUTED, FontId::Small);
  char big[12];
  TimeSync::formatClock(big, sizeof(big), Settings.d.hour12, false, Settings.d.blinkColons);
  if (!ready) snprintf(big, sizeof(big), "--:--");
  int bw = Fonts::textWidth(big, FontId::Global);
  Fonts::drawText(DisplayId::Back, (BACK_W - bw) / 2, 22, big, COL_TEXT, FontId::Global);

  if (Settings.d.hour12 && ready) {
    const char* mer = (t.tm_hour >= 12) ? "PM" : "AM";
    Fonts::drawText(DisplayId::Back, (BACK_W - bw) / 2 + bw + 3, 26, mer, COL_MUTED, FontId::Small);
  }

  char date[24];
  TimeSync::formatDate(date, sizeof(date));
  if (!ready) snprintf(date, sizeof(date), "syncing NTP...");
  int dw = Fonts::textWidth(date, FontId::Normal);
  Fonts::drawText(DisplayId::Back, (BACK_W - dw) / 2, 42, date, COL_MUTED, FontId::Normal);
}

void ClockApp::handleKey(Key k) {
  if (k == Key::Ok) Settings.d.hour12 = !Settings.d.hour12;
}
