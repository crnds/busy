// gfx.h — typography. DESIGN.md §4.
//
// ROLES over TFT_eSPI's built-in BITMAP faces, selected by number. A component
// asks for the job the text is doing, never for a size.
//
// F_TITLE and F_BODY are the SAME face at the same size: the built-in set has
// no bold and nothing between Font 2 and Font 4, so hierarchy here is carried
// by colour tier alone. They stay separate roles because they express intent,
// and because a future face could distinguish them again.
//
// The faces are numbers (setTextFont(2)), so no glyph table is ever pulled into
// a translation unit and nothing can duplicate them in flash. Do not include a
// GFX font header anywhere in this project.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>
#include <stddef.h>

enum FontRole : uint8_t {
    F_TITLE = 0, // Font 2, 10px caps. Card titles, the clock
    F_BODY,      // Font 2, 10px caps. Live state, chip labels, tabs, captions
    F_MICRO,     // Font 1, GLCD 6x8. Last-resort fit only, never a first choice
    F_COUNT
};

// THREE roles, not the four the design system started with. The fourth was a
// large-number role over Font 4, and nothing in this firmware's chrome draws a
// large number -- the two virtual panels carry that content, in their own
// faces. An unused role is 8.5 KB of flash and a table row that can drift, and
// DESIGN.md already names Font 4 as the first thing to trim. LOAD_FONT4 is off
// in platformio.ini to match; turning it back on without adding a role that
// uses it would just re-add the weight.

// The F_BODY ink extents, as compile-time constants, so config.h's row
// geometry can be static_assert()ed against them. Keep in step with the
// tables in gfx.cpp -- font_metrics.py --check verifies those.
constexpr int F_BODY_INK_TOP = -7;
constexpr int F_BODY_INK_BOT =  7;

void gfxBegin(TFT_eSPI *tft);

// Ink extents, relative to a vertically-centred datum, AFTER the per-role
// offset is applied. Ask for ink, not for ascent: a built-in face's ink is not
// symmetric about its datum, so cy - ascent/2 only finds the top by accident.
int  fontInkTop(FontRole r);     // negative -- rows above cy
int  fontInkBottom(FontRole r);  // positive -- rows below cy
int  fontCap(FontRole r);
int  fontBaseline(FontRole r);

int  textW(FontRole r, const char *s);

// datum is a TFT_eSPI datum: ML_DATUM, MC_DATUM or MR_DATUM. y is the vertical
// CENTRE in every case -- the per-role box offset is applied here so no caller
// has to know that TFT_eSPI centres a built-in face on its full box.
void drawText(FontRole r, const char *s, int x, int cy, uint16_t fg, uint8_t datum);

// Text degrades, it never overflows.
FontRole textFit(FontRole r, const char *s, int budget);
void     textTrunc(char *dst, size_t n, FontRole r, const char *src, int budget);
