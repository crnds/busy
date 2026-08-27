// VDisplay.h — the virtual panel.
//
// ONE framebuffer: 160x80 RGB565, presented at a flat 2x. The CYD has a single
// screen, so the two-panel emulation is gone -- what is kept is the BUSY Bar
// back panel's geometry (the layouts were designed for it, and it is the real
// device's resolution, so draw coordinates stay compatible) with the front
// panel's full-colour rendering.
//
// Content is stored in real colour and pushed through themeMap() at present
// time, so night mode recolours a caller's arbitrary RGB along with the chrome.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>
#include "../../include/config.h"

enum VFont : uint8_t { VF_TINY = 0, VF_SMALL = 1 };

// The 9-point align used by canvas elements, matching the original API.
enum VAlign : uint8_t {
    VA_TL = 0, VA_TC, VA_TR,
    VA_ML,     VA_MC, VA_MR,
    VA_BL,     VA_BC, VA_BR
};

namespace vd {

void begin();

inline int width()  { return VD_W; }
inline int height() { return VD_H; }

void clear(uint16_t c = 0);
void pixel(int x, int y, uint16_t c);
void fillRect(int x, int y, int w, int h, uint16_t c);
void drawRect(int x, int y, int w, int h, uint16_t c);

int  glyphW(VFont f);
int  glyphH(VFont f);
int  textW(VFont f, const char *s, uint8_t scale);
// x,y is the anchor; align says which corner of the text box sits on it.
void text(int x, int y, const char *s, uint16_t c,
          VFont f, uint8_t scale, VAlign align);

// Raw RGB565 image data, row-major.
void blit565(int x, int y, int w, int h, const uint16_t *src);
// Packed 1bpp, MSB-first per row, rows padded to a byte. Used by xpm elements.
void blit1(int x, int y, int w, int h, const uint8_t *src, uint16_t c);

// Dirty tracking is a bounding box in SOURCE coordinates, so present() only
// upscales what actually changed.
bool dirty();
void invalidate();
void present(TFT_eSPI *tft, int dx, int dy);

const uint16_t *fb();

} // namespace vd
