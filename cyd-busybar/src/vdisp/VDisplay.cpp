#include "VDisplay.h"
#include "../ui/theme.h"
#include <string.h>

#include "../canvas/vfont.inc"

namespace {

uint16_t s_fb[VD_W * VD_H];       // 25,600 bytes

struct Dirty { int x0, y0, x1, y1; bool any; };
Dirty s_dirty;

inline void touch(int x, int y) {
    if (!s_dirty.any) { s_dirty.x0 = s_dirty.x1 = x; s_dirty.y0 = s_dirty.y1 = y; s_dirty.any = true; return; }
    if (x < s_dirty.x0) s_dirty.x0 = x;
    if (x > s_dirty.x1) s_dirty.x1 = x;
    if (y < s_dirty.y0) s_dirty.y0 = y;
    if (y > s_dirty.y1) s_dirty.y1 = y;
}

inline void touchRect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w - 1, y1 = y + h - 1;
    if (x1 >= VD_W) x1 = VD_W - 1;
    if (y1 >= VD_H) y1 = VD_H - 1;
    if (x0 > x1 || y0 > y1) return;
    touch(x0, y0);
    touch(x1, y1);
}

const uint8_t *glyph(VFont f, char ch) {
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    int i = ch - 0x20;
    return (f == VF_TINY) ? VFONT_3X5[i] : VFONT_5X7[i];
}

} // namespace

namespace vd {

int glyphW(VFont f) { return f == VF_TINY ? 3 : 5; }
int glyphH(VFont f) { return f == VF_TINY ? 5 : 7; }
// Advance is the glyph plus a 1px gap, so a caller can lay text out by eye.
static int advance(VFont f) { return glyphW(f) + 1; }

void begin() {
    memset(s_fb, 0, sizeof(s_fb));
    invalidate();
}

void clear(uint16_t c) {
    if (c == 0) memset(s_fb, 0, sizeof(s_fb));
    else for (int i = 0; i < VD_W * VD_H; i++) s_fb[i] = c;
    invalidate();
}

void pixel(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= VD_W || y >= VD_H) return;
    s_fb[y * VD_W + x] = c;
    touch(x, y);
}

void fillRect(int x, int y, int w, int h, uint16_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > VD_W) x1 = VD_W;
    if (y1 > VD_H) y1 = VD_H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *row = &s_fb[yy * VD_W];
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
    touch(x0, y0);
    touch(x1 - 1, y1 - 1);
}

void drawRect(int x, int y, int w, int h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    fillRect(x, y, w, 1, c);
    fillRect(x, y + h - 1, w, 1, c);
    fillRect(x, y, 1, h, c);
    fillRect(x + w - 1, y, 1, h, c);
}

int textW(VFont f, const char *s, uint8_t scale) {
    if (!s || !*s || !scale) return 0;
    int n = (int)strlen(s);
    // n glyphs and n-1 gaps: the trailing gap is not part of the ink.
    return (n * advance(f) - 1) * scale;
}

void text(int x, int y, const char *s, uint16_t c,
          VFont f, uint8_t scale, VAlign align) {
    if (!s || !*s) return;
    if (!scale) scale = 1;

    int tw = textW(f, s, scale);
    int th = glyphH(f) * scale;

    switch (align % 3) {
        case 1: x -= tw / 2; break;
        case 2: x -= tw;     break;
        default: break;
    }
    switch (align / 3) {
        case 1: y -= th / 2; break;
        case 2: y -= th;     break;
        default: break;
    }

    int gw = glyphW(f), gh = glyphH(f);
    int pen = x;
    for (const char *p = s; *p; p++, pen += advance(f) * scale) {
        const uint8_t *g = glyph(f, *p);
        for (int col = 0; col < gw; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < gh; row++) {
                if (!((bits >> row) & 1)) continue;
                if (scale == 1) pixel(pen + col, y + row, c);
                else            fillRect(pen + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

void blit565(int x, int y, int w, int h, const uint16_t *src) {
    if (!src) return;
    for (int r = 0; r < h; r++)
        for (int cx = 0; cx < w; cx++)
            pixel(x + cx, y + r, src[r * w + cx]);
    touchRect(x, y, w, h);
}

void blit1(int x, int y, int w, int h, const uint8_t *src, uint16_t c) {
    if (!src) return;
    int stride = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
        const uint8_t *row = src + (size_t)r * stride;
        for (int cx = 0; cx < w; cx++) {
            if ((row[cx >> 3] >> (7 - (cx & 7))) & 1) pixel(x + cx, y + r, c);
        }
    }
    touchRect(x, y, w, h);
}

bool dirty() { return s_dirty.any; }

void invalidate() { s_dirty = { 0, 0, VD_W - 1, VD_H - 1, true }; }

const uint16_t *fb() { return s_fb; }

// ── PRESENT ──────────────────────────────────────────────────────────────
// One destination row at a time through a line buffer, over the dirty box
// only. A full-panel sprite would be 51 KB of heap for no gain: present()
// never runs over more than what changed, and the panel is the slow part.
void present(TFT_eSPI *tft, int dx, int dy) {
    if (!s_dirty.any || !tft) return;

    static uint16_t line[VD_DRAW_W];
    int sw = s_dirty.x1 - s_dirty.x0 + 1;
    int dw = sw * VD_SCALE;

    for (int sy = s_dirty.y0; sy <= s_dirty.y1; sy++) {
        const uint16_t *row = &s_fb[sy * VD_W + s_dirty.x0];
        for (int i = 0; i < sw; i++) {
            uint16_t c = themeMap(row[i]);
            for (int k = 0; k < VD_SCALE; k++) line[i * VD_SCALE + k] = c;
        }
        int dyy = dy + sy * VD_SCALE;
        for (int k = 0; k < VD_SCALE; k++)
            tft->pushImage(dx + s_dirty.x0 * VD_SCALE, dyy + k, dw, 1, line);
    }
    s_dirty.any = false;
}

} // namespace vd
