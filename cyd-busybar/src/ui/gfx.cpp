#include "gfx.h"
#include <string.h>

static TFT_eSPI *T = nullptr;

// The built-in face number backing each role.
static const uint8_t ROLE_FACE[F_COUNT] = { 2, 2, 1 };

// TFT_eSPI centres a built-in font on its FULL BOX with an M* datum, and the
// box carries blank rows the ink does not. ROLE_DY corrects for that per role.
// Both remaining faces happen to want 0; the hook stays because it is what a
// new face would need, and because font_metrics.py prints it.
static const int8_t ROLE_DY[F_COUNT] = { 0, 0, 0 };

// MEASURED, not read off the font headers. The headers give the nominal box
// (Font 2: 16 tall / baseline 13; Font 4: 26 / 19), but every built-in glyph
// sits inset inside that box -- Font 2's caps start 3 rows down, Font 4's 1.
// scripts/font_metrics.py decodes the glyph data and prints these three tables;
// re-run it rather than adjusting by eye.
static const int8_t INK_TOP[F_COUNT]  = { F_BODY_INK_TOP, F_BODY_INK_TOP, -4 };
static const int8_t INK_BOT[F_COUNT]  = { F_BODY_INK_BOT, F_BODY_INK_BOT,  3 };
static const int8_t CAP_H[F_COUNT]    = {  10,  10,   7 };
static const int8_t BASELINE[F_COUNT] = {   5,   5,   3 };

void gfxBegin(TFT_eSPI *tft) { T = tft; }

int fontInkTop(FontRole r)    { return INK_TOP[r]; }
int fontInkBottom(FontRole r) { return INK_BOT[r]; }
int fontCap(FontRole r)       { return CAP_H[r]; }
int fontBaseline(FontRole r)  { return BASELINE[r]; }

int textW(FontRole r, const char *s) {
    if (!T || !s) return 0;
    return T->textWidth(s, ROLE_FACE[r]);
}

void drawText(FontRole r, const char *s, int x, int cy, uint16_t fg, uint8_t datum) {
    if (!T || !s) return;
    T->setTextFont(ROLE_FACE[r]);
    T->setTextColor(fg);          // no background: callers clear their own rect
    T->setTextDatum(datum);
    T->drawString(s, x, cy + ROLE_DY[r]);
}

FontRole textFit(FontRole r, const char *s, int budget) {
    if (textW(r, s) <= budget) return r;
    // Font 2 is the only body face, so the only step down from any role is the
    // 6x8 fallback.
    if (r != F_MICRO && textW(F_MICRO, s) <= budget) return F_MICRO;
    return r;   // caller truncates
}

void textTrunc(char *dst, size_t n, FontRole r, const char *src, int budget) {
    if (!n) return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
    if (textW(r, dst) <= budget) return;

    // The built-in faces are proportional, so drop characters and re-measure
    // with the ellipsis attached rather than assuming a fixed advance. The
    // measurement happens in dst itself: a fixed-size probe buffer would
    // silently measure a truncated copy for any string longer than the probe.
    size_t len = strlen(dst);
    while (len > 0) {
        if (len + 3 <= n) {
            dst[len] = '.'; dst[len + 1] = '.'; dst[len + 2] = '\0';
            if (textW(r, dst) <= budget) return;
        }
        dst[len] = '\0';
        len--;
    }
    // Everything dropped. The ellipsis alone still says there was more, which
    // is the one thing an empty string would not.
    if (n >= 3) { dst[0] = '.'; dst[1] = '.'; dst[2] = '\0'; }
    else        { dst[0] = '\0'; }
}
