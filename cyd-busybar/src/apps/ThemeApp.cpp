#include "apps/ThemeApp.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "settings/Settings.h"

static const ThemeInfo kThemes[] = {
    {"on_air",  "BUSY",    "RECORDING", rgb565(0xFF, 0x2A, 0x2A), rgb565(0x28, 0x00, 0x00), "pulse_bars"},
    {"meeting", "MEETING", "IN A CALL", rgb565(0x3D, 0x7F, 0xFF), rgb565(0x00, 0x10, 0x28), "dots"},
    {"dnd",     "DND",     "DO NOT DISTURB", rgb565(0xB0, 0x80, 0xFF), rgb565(0x10, 0x00, 0x20), "moon"},
    {"coding",  "CODING",  "HEADS DOWN", rgb565(0x22, 0xC5, 0x5E), rgb565(0x00, 0x18, 0x08), "code"},
    {"lunch",   "LUNCH",   "BACK SOON", rgb565(0xF4, 0xC2, 0x0E), rgb565(0x28, 0x18, 0x00), "utensils"},
};
static const int kThemeN = sizeof(kThemes) / sizeof(kThemes[0]);
static int sIndex = 0;

static int findId(const char* id) {
  for (int i = 0; i < kThemeN; i++) if (!strcmp(kThemes[i].id, id)) return i;
  return 0;
}

void ThemeApp::begin() { sIndex = findId(Settings.d.theme); }

const ThemeInfo* ThemeApp::current() { return &kThemes[sIndex]; }
const ThemeInfo* ThemeApp::list(int& n) { n = kThemeN; return kThemes; }
int ThemeApp::index() { return sIndex; }

void ThemeApp::setById(const char* id) {
  sIndex = findId(id);
  strlcpy(Settings.d.theme, kThemes[sIndex].id, sizeof(Settings.d.theme));
}

static void animPulseBars(const ThemeInfo* t) {
  Display.clear(DisplayId::Front, t->bg);
  int frame = (millis() / 80) % 8;
  for (int i = 0; i < 4; i++) {
    int h = 4 + ((frame + i * 2) % 8);
    if (h > 14) h = 14;
    Display.fillRect(DisplayId::Front, 2 + i * 3, FRONT_H - 1 - h, 2, h, t->color);
    Display.fillRect(DisplayId::Front, FRONT_W - 4 - i * 3, FRONT_H - 1 - h, 2, h, t->color);
  }
  int tw = Fonts::textWidth(t->label, FontId::Bold);
  Fonts::drawText(DisplayId::Front, (FRONT_W - tw) / 2, 4, t->label, COL_TEXT, FontId::Bold);
}

static void animDots(const ThemeInfo* t) {
  Display.clear(DisplayId::Front, t->bg);
  int tw = Fonts::textWidth(t->label, FontId::Bold);
  Fonts::drawText(DisplayId::Front, 2, 4, t->label, COL_TEXT, FontId::Bold);
  int n = 1 + (millis() / 400) % 3;
  for (int i = 0; i < n; i++) {
    Display.fillRect(DisplayId::Front, 4 + tw + 4 + i * 5, 8, 2, 2, t->color);
  }
}

static void animMoon(const ThemeInfo* t) {
  Display.clear(DisplayId::Front, t->bg);
  int cx = 10, cy = 8, r = 5;
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) Display.pixel(DisplayId::Front, cx + x, cy + y, t->color);
  int ox = 3 - (int)((millis() / 80) % 5);
  for (int y = -4; y <= 4; y++)
    for (int x = -4; x <= 4; x++)
      if (x * x + y * y <= 16)
        Display.pixel(DisplayId::Front, cx + ox + x, cy + y - 1, t->bg);
  int tw = Fonts::textWidth(t->label, FontId::Bold);
  Fonts::drawText(DisplayId::Front, 20, 4, t->label, COL_TEXT, FontId::Bold);
}

static void animCode(const ThemeInfo* t) {
  Display.clear(DisplayId::Front, t->bg);
  const char* bits[] = {"{ }", "</>", "fn()", "=>", "ok;"};
  int i = (millis() / 350) % 5;
  Fonts::drawText(DisplayId::Front, 2, 1, bits[i], t->color, FontId::Small);
  int tw = Fonts::textWidth(t->label, FontId::Bold);
  Fonts::drawText(DisplayId::Front, FRONT_W - tw - 2, 8, t->label, COL_TEXT, FontId::Bold);
}

static void animUtensils(const ThemeInfo* t) {
  Display.clear(DisplayId::Front, t->bg);
  // fork
  Display.vLine(DisplayId::Front, 6, 2, 12, t->color);
  Display.vLine(DisplayId::Front, 4, 2, 5, t->color);
  Display.vLine(DisplayId::Front, 8, 2, 5, t->color);
  Display.hLine(DisplayId::Front, 4, 7, 5, t->color);
  // knife
  Display.vLine(DisplayId::Front, 14, 2, 12, COL_MUTED);
  Display.fillRect(DisplayId::Front, 13, 2, 3, 6, COL_TEXT);
  int tw = Fonts::textWidth(t->label, FontId::Bold);
  Fonts::drawText(DisplayId::Front, FRONT_W - tw - 2, 4, t->label, COL_TEXT, FontId::Bold);
}

void ThemeApp::draw() {
  const ThemeInfo* t = current();
  if (!strcmp(t->anim, "pulse_bars")) animPulseBars(t);
  else if (!strcmp(t->anim, "dots")) animDots(t);
  else if (!strcmp(t->anim, "moon")) animMoon(t);
  else if (!strcmp(t->anim, "code")) animCode(t);
  else animUtensils(t);

  Display.clear(DisplayId::Back, t->bg);
  Fonts::drawText(DisplayId::Back, 4, 4, "STATUS", COL_MUTED, FontId::Small);
  int tw = Fonts::textWidth(t->label, FontId::Global);
  Fonts::drawText(DisplayId::Back, (BACK_W - tw) / 2, 20, t->label, t->color, FontId::Global);
  int sw = Fonts::textWidth(t->backSub, FontId::Normal);
  Fonts::drawText(DisplayId::Back, (BACK_W - sw) / 2, 38, t->backSub, COL_TEXT, FontId::Normal);
  Fonts::drawText(DisplayId::Back, 8, 54, "< prev", COL_MUTED, FontId::Tiny);
  Fonts::drawText(DisplayId::Back, BACK_W - 40, 54, "next >", COL_MUTED, FontId::Tiny);

  Display.setRgb((t->color >> 8) & 0xF8, (t->color >> 3) & 0xFC, (t->color << 3) & 0xF8);
}

void ThemeApp::handleKey(Key k) {
  if (k == Key::Up || k == Key::Custom) {
    sIndex = (sIndex + 1) % kThemeN;
  } else if (k == Key::Down) {
    sIndex = (sIndex + kThemeN - 1) % kThemeN;
  }
  strlcpy(Settings.d.theme, kThemes[sIndex].id, sizeof(Settings.d.theme));
  Settings.save();
}
