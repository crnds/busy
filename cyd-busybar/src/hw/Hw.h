// Hw.h — backlight, ambient light, the onboard RGB LED, and touch.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

// ── BACKLIGHT ────────────────────────────────────────────────────────────
void    blBegin();
void    blSet(uint8_t duty);      // 0..255, LEDC PWM on PIN_BL
uint8_t blGet();

// ── AMBIENT LIGHT ────────────────────────────────────────────────────────
void     ldrBegin();
uint16_t ldrRaw();                // last smoothed reading, 0..4095
void     ldrTick(uint32_t now);   // samples every LDR_PERIOD_MS

// ── ONBOARD RGB LED ──────────────────────────────────────────────────────
// All three channels are active LOW on this board.
void ledBegin();
void ledSet(int32_t rgb888);      // -1 turns it off

// ── TOUCH ────────────────────────────────────────────────────────────────
struct Touch { int16_t x, y; bool down; };

void  touchBegin();
// Returns true once per press, on release-free debounce, with panel coords.
bool  touchPoll(Touch &out, uint32_t now);
