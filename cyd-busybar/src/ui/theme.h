// theme.h — semantic colour tokens. DESIGN.md §3.
//
// A page never picks a hex value, it asks for a role. The tokens are RUNTIME
// values, not constants, because night mode recolours the palette in place --
// so a C_* name must never appear in a static or constexpr initializer.
#pragma once

#include <stdint.h>

enum ThemeTok : uint8_t {
    T_BG = 0,   // the ground behind everything
    T_SURFACE,  // a filled card
    T_ELEVATED, // a control sitting ON a card
    T_BORDER,   // a card's edge
    T_DIVIDER,  // rules and tracks -- felt, not read
    T_TEXT,     // primary: what a thing IS
    T_TEXT2,    // secondary: live values, chip labels
    T_TEXT3,    // tertiary: captions, unselected tabs
    T_DISABLED, // no state to show
    T_DIM,      // a STALE reading -- distinct from disabled
    T_ACCENT,   // selected. The only saturated colour at rest
    T_NEUTRAL,  // selected, but what it selected is OFF (§3.1)
    T_SUCCESS,  // online
    T_WARNING,  // degraded
    T_ERROR,    // failed / offline
    T_COUNT
};

extern uint16_t TH[T_COUNT];

#define C_BG       TH[T_BG]
#define C_SURFACE  TH[T_SURFACE]
#define C_ELEVATED TH[T_ELEVATED]
#define C_BORDER   TH[T_BORDER]
#define C_DIVIDER  TH[T_DIVIDER]
#define C_TEXT     TH[T_TEXT]
#define C_TEXT2    TH[T_TEXT2]
#define C_TEXT3    TH[T_TEXT3]
#define C_DISABLED TH[T_DISABLED]
#define C_DIM      TH[T_DIM]
#define C_ACCENT   TH[T_ACCENT]
#define C_NEUTRAL  TH[T_NEUTRAL]
#define C_SUCCESS  TH[T_SUCCESS]
#define C_WARNING  TH[T_WARNING]
#define C_ERROR    TH[T_ERROR]

void     themeInit();
void     themeSetNight(bool on);
bool     themeIsNight();

// Blend a toward b. RGB565 has no alpha, so every "translucent" fill in this
// UI is a precomputed blend against whatever is behind it.
uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t);

uint16_t rgb888to565(uint32_t rgb);

// Map an arbitrary full-colour value through the active palette mode. Canvas
// content arrives from the network in real colours; at night it must not paint
// green and blue onto a red-only screen.
uint16_t themeMap(uint16_t c565);
