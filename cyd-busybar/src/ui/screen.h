// screen.h — the CYD's own display. DESIGN.md §9, §11.
//
// Nothing in screen.cpp computes a position from a literal; every coordinate
// comes from the LAYOUT block of config.h.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

void screenBegin(TFT_eSPI *tft);

// Force a full repaint. Both a palette swap and a page change need this: a
// palette swap alters no value a dirty-region compare looks at, and a page
// change replaces content the previous page's rects will never clear.
void screenInvalidate();

void screenSetNight(bool on);
void screenTick(uint32_t now);

// The boot screen. pip cycles 0..2.
void screenSplash(uint32_t now, uint8_t pip);

// One tap. Returns true if it landed on something.
bool screenTouch(int x, int y, uint32_t now);

// The panel handle, for Chrome.cpp's transition. Nothing else should need it.
TFT_eSPI *screenTft();
