#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include <pgmspace.h>

#include "canvas/font5x7.inc"

int Fonts::glyphW(FontId id) {
  switch (id) {
    case FontId::Tiny: return 3;
    case FontId::Small: return 4;
    case FontId::Condensed: return 4;
    case FontId::Large: return 6;
    case FontId::ExtraLarge: return 7;
    case FontId::Global: return 8;
    default: return 5;
  }
}

int Fonts::glyphH(FontId id) {
  switch (id) {
    case FontId::Tiny:
    case FontId::Small: return 5;
    case FontId::Large: return 9;
    case FontId::ExtraLarge: return 10;
    case FontId::Global: return 11;
    default: return 7;
  }
}

int Fonts::textHeight(FontId id) { return glyphH(id); }

int Fonts::textWidth(const char* s, FontId id) {
  if (!s || !*s) return 0;
  int n = (int)strlen(s);
  return n * glyphW(id) + (n - 1);
}

static void glyphCols(char ch, uint8_t out[5]) {
  unsigned idx = (unsigned)(uint8_t)ch;
  if (idx < 0x20 || idx > 0x7E) idx = (unsigned)'?';
  idx -= 0x20;
  for (int i = 0; i < 5; i++) out[i] = pgm_read_byte(&FONT5X7[idx][i]);
}

static bool sample5x7(const uint8_t cols[5], int sx, int sy, bool bold) {
  if ((unsigned)sx >= 5u || (unsigned)sy >= 7u) return false;
  bool on = cols[sx] & (1 << sy);
  if (bold && sx + 1 < 5) on = on || (cols[sx + 1] & (1 << sy));
  return on;
}

static void blitGlyph(DisplayId d, int x, int y, char ch, uint16_t color, FontId id,
                      int clipX, int clipY, int clipW, int clipH, bool clip) {
  uint8_t cols[5];
  glyphCols(ch, cols);
  int dw = Fonts::glyphW(id);
  int dh = Fonts::glyphH(id);
  bool bold = (id == FontId::Bold);
  int W = Display.width(d), H = Display.height(d);
  for (int dy = 0; dy < dh; dy++) {
    // Map onto the 5x7 source inclusively so Tiny/Small keep the midline
    // (truncating dy*7/dh skipped row 3 and made CLOCK/STATUS unreadable).
    int sy = (dh <= 1) ? 0 : (dy * 6 + (dh - 1) / 2) / (dh - 1);
    if (sy > 6) sy = 6;
    int py = y + dy;
    if ((unsigned)py >= (unsigned)H) continue;
    if (clip && (py < clipY || py >= clipY + clipH)) continue;
    for (int dx = 0; dx < dw; dx++) {
      int sx = (dw <= 1) ? 0 : (dx * 4 + (dw - 1) / 2) / (dw - 1);
      if (sx > 4) sx = 4;
      int px = x + dx;
      if ((unsigned)px >= (unsigned)W) continue;
      if (clip && (px < clipX || px >= clipX + clipW)) continue;
      if (sample5x7(cols, sx, sy, bold)) Display.pixel(d, px, py, color);
    }
  }
}

void Fonts::drawText(DisplayId d, int x, int y, const char* s, uint16_t color, FontId id) {
  if (!s) return;
  int adv = glyphW(id) + 1;
  for (const char* p = s; *p; p++) {
    blitGlyph(d, x, y, *p, color, id, 0, 0, 0, 0, false);
    x += adv;
  }
}

void Fonts::drawTextClip(DisplayId d, int clipX, int clipY, int clipW, int clipH,
                         int originX, const char* s, uint16_t color, FontId id) {
  if (!s) return;
  int adv = glyphW(id) + 1;
  int x = originX;
  for (const char* p = s; *p; p++) {
    blitGlyph(d, x, clipY, *p, color, id, clipX, clipY, clipW, clipH, true);
    x += adv;
  }
}

void Fonts::drawIconWifi(int tftX, int tftY, int bars, uint16_t color) {
  // 8×8 status-bar icon drawn on the physical TFT.
  auto p = [&](int x, int y) { Display.tft.drawPixel(tftX + x, tftY + y, color); };
  // base dot
  p(3, 6); p(4, 6); p(3, 7); p(4, 7);
  if (bars >= 1) { p(2, 5); p(5, 5); p(3, 4); p(4, 4); }
  if (bars >= 2) { p(1, 3); p(6, 3); p(2, 2); p(5, 2); }
  if (bars >= 3) { p(0, 1); p(7, 1); p(1, 0); p(6, 0); p(2, 0); p(5, 0); p(3, 0); p(4, 0); }
}

void Fonts::drawIconSun(int tftX, int tftY, uint16_t color) {
  auto p = [&](int x, int y) { Display.tft.drawPixel(tftX + x, tftY + y, color); };
  p(3, 3); p(4, 3); p(3, 4); p(4, 4);
  p(3, 0); p(4, 0); p(3, 7); p(4, 7);
  p(0, 3); p(0, 4); p(7, 3); p(7, 4);
  p(1, 1); p(6, 1); p(1, 6); p(6, 6);
}
