// icons.h — DESIGN.md §7.
//
// Drawn from primitives, never stored. Night mode swaps the palette at runtime,
// so a baked RGB565 sprite would render in day colours over a red-only UI.
//
// THE GRID: every icon centres on (cx, cy), fits a 15x15 box, and never draws
// outside it. Stroke is 1px throughout except where a shape is solid by nature.
// Every icon here is used, and every one carries state.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

void iconsBegin(TFT_eSPI *tft);

// The entire connectivity readout. bars is 3 (up) or 0 (down); 0 draws the
// same three shapes hollow, so the state changes shape AND colour, never
// colour alone.
void icoWifi(int cx, int cy, uint8_t bars, uint16_t fg);

// An overlay dot on another glyph -- the time-not-synced badge. Takes the
// colour behind it so it punches a ring out of whatever it sits on.
void icoBadge(int cx, int cy, uint16_t fg, uint16_t behind);

void icoSun(int cx, int cy, uint16_t fg);

// A crescent is a disc minus a disc. With no alpha the bite must be painted in
// the surface's own fill, so this one takes the colour BEHIND it.
void icoMoon(int cx, int cy, uint16_t fg, uint16_t behind);

void icoClock(int cx, int cy, uint16_t fg);
