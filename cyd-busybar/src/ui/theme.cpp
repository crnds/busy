#include "theme.h"

uint16_t TH[T_COUNT];

static bool s_night = false;

// Day palette. DESIGN.md §3 -- the table there is the source, this is it in
// RGB565. C_BG is 0x0041 (#07090D); §3.2 pins it.
static const uint32_t DAY[T_COUNT] = {
    0x07090D, // BG
    0x16181D, // SURFACE
    0x23272F, // ELEVATED
    0x2F343E, // BORDER
    0x1C1F26, // DIVIDER
    0xF4F6FA, // TEXT
    0xA6B0C2, // TEXT2
    0x707A8C, // TEXT3
    0x444C59, // DISABLED
    0x8A94A6, // DIM
    0x18BCF2, // ACCENT
    0x7A828E, // NEUTRAL
    0x2FD97C, // SUCCESS
    0xF5A524, // WARNING
    0xFF4D6A, // ERROR
};

// Night ladder, 5-bit red. Every level is PINNED, not derived from luminance:
// left to derive, ERROR and DIM land on the same value, and a card picks
// between them on the very same string. DESIGN.md §3.3.
//
//   BG 0 < SURFACE 2 < DIVIDER 3 < ELEVATED 4 < BORDER 6 < DIM 7 < SUCCESS 10
//        < DISABLED 12 < NEUTRAL 13 < TEXT3 16 < ACCENT 18 < TEXT2 20
//        < WARNING 22 < TEXT 25 < ERROR 31
//
// Fifteen tokens, all distinct. Dropping the HA build's two colour-temperature
// tokens freed level 14, which is why NEUTRAL->TEXT3 now clears 3 steps where
// it used to clear 1.
static const uint8_t NIGHT_R5[T_COUNT] = {
    0,  // BG
    2,  // SURFACE
    4,  // ELEVATED
    6,  // BORDER
    3,  // DIVIDER
    25, // TEXT
    20, // TEXT2
    16, // TEXT3
    12, // DISABLED
    7,  // DIM
    18, // ACCENT
    13, // NEUTRAL
    10, // SUCCESS
    22, // WARNING
    31, // ERROR
};

uint16_t rgb888to565(uint32_t rgb) {
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

static void apply() {
    for (uint8_t i = 0; i < T_COUNT; i++) {
        TH[i] = s_night ? (uint16_t)(NIGHT_R5[i] << 11) : rgb888to565(DAY[i]);
    }
}

void themeInit() { s_night = false; apply(); }

void themeSetNight(bool on) {
    s_night = on;
    apply();
    // A palette swap changes no value a dirty-region compare looks at, so the
    // caller must fillScreen() and invalidate. screenSetNight() does both.
}

bool themeIsNight() { return s_night; }

uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t) {
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = ar + ((br - ar) * t) / 255;
    int g = ag + ((bg - ag) * t) / 255;
    int bl = ab + ((bb - ab) * t) / 255;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

uint16_t themeMap(uint16_t c565) {
    if (!s_night) return c565;
    // Luminance -> red only. Green carries the most weight and already has the
    // extra bit, so it does the work here.
    int r = (c565 >> 11) & 0x1F, g = (c565 >> 5) & 0x3F, b = c565 & 0x1F;
    int y = (r * 77 + (g >> 1) * 151 + b * 28) >> 8;   // 0..31
    if (y > 31) y = 31;
    return (uint16_t)(y << 11);
}
