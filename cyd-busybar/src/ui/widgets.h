// widgets.h — DESIGN.md §8. One `vis` byte in, pixels out.
//
// The state set is CLOSED and every component honours all of it. Never branch
// on `vis` locally -- a component that rolls its own is how a pressed chip and
// a pressed toggle end up looking like different interactions.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

enum BtnVis : uint8_t {
    BV_INACTIVE = 0, // available, not selected. 0 so a memset means "inactive"
    BV_ACTIVE,       // selected / current state
    BV_ACTIVE_OFF,   // selected, and what it selected is OFF -- DESIGN.md §3.1
    BV_PRESSED,      // held -- a tactility flash, not a state
    BV_DISABLED,     // unavailable: there is no state, so none is implied
    BV_ERR,          // the last command on this surface failed
    // NEW STATES GO ON THE END. The raw ordinal is what every snapshot stores
    // and compares against next frame, so inserting in the middle would
    // silently change what "unchanged" means across a partial rebuild.
};

struct CtlColour { uint16_t fill, edge, fg; };

// Resolved once, here. Every selected state is a solid fill with dark text,
// and only WHICH fill depends on what was selected -- that invariant is why
// BV_ACTIVE_OFF is a state rather than a colour the caller passes in.
CtlColour ctlColour(uint8_t vis);

void widgetsBegin(TFT_eSPI *tft);

// The container everything sits in. A surface groups its contents without
// drawing a line. Pass fill == edge for the unfilled variant.
void wCard(int x, int y, int w, int h, uint16_t fill, uint16_t edge);

// The workhorse control. SP_1 padding each side; the LABEL BUDGET, not the
// chip width, decides whether a caption fits. Labels are uppercase: caps have
// no descenders, so they centre cleanly in the chip.
void wChip(int x, int y, int w, int h, const char *label, uint8_t vis);

// A settings toggle: a square knob at one end of a square track. No press
// flash -- the flip IS the feedback, and it is immediate because nothing here
// touches the network.
void wToggle(int x, int y, int w, int h, int knob, bool on, uint16_t behind);

// A navigation tab. Only the selected one draws a mark: a 2px underline under
// a full-strength label, against C_TEXT3 for the rest. The header is chrome and
// stays quiet unless something is wrong, so the mark is the underline rather
// than a filled pill competing with the body.
void wTab(int x, int y, int w, int h, const char *label, bool selected, int underlineH);

// One splash pip. A ring, not a dim disc: a disc dark enough to read as "off"
// is also dark enough to be invisible across a dark room.
void wPip(int cx, int cy, int r, bool on);
