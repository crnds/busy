#include "Hw.h"
#include "../../include/config.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ── BACKLIGHT ────────────────────────────────────────────────────────────
static uint8_t s_duty = 0;

void blBegin() {
    ledcSetup(BL_LEDC_CH, BL_LEDC_FREQ, BL_LEDC_BITS);
    ledcAttachPin(PIN_BL, BL_LEDC_CH);
    blSet(0);   // stay dark until settings have loaded, or the panel flashes
                // 100% for the length of the mount
}

void blSet(uint8_t duty) {
    s_duty = duty;
    ledcWrite(BL_LEDC_CH, duty);
}

uint8_t blGet() { return s_duty; }

// ── AMBIENT LIGHT ────────────────────────────────────────────────────────
static uint16_t s_ldr = 2048;
static uint32_t s_ldrNext = 0;

void ldrBegin() {
    analogSetPinAttenuation(PIN_LDR, ADC_11db);
    s_ldr = (uint16_t)analogRead(PIN_LDR);
}

uint16_t ldrRaw() { return s_ldr; }

void ldrTick(uint32_t now) {
    if ((int32_t)(now - s_ldrNext) < 0) return;
    s_ldrNext = now + LDR_PERIOD_MS;
    // Heavy EMA: a hand passing over the sensor must not step the backlight.
    uint16_t r = (uint16_t)analogRead(PIN_LDR);
    s_ldr = (uint16_t)((s_ldr * 7 + r) / 8);
}

// ── ONBOARD RGB LED ──────────────────────────────────────────────────────
void ledBegin() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    ledSet(-1);
}

void ledSet(int32_t rgb888) {
    bool off = (rgb888 < 0);
    uint8_t r = off ? 0 : (uint8_t)((rgb888 >> 16) & 0xFF);
    uint8_t g = off ? 0 : (uint8_t)((rgb888 >> 8) & 0xFF);
    uint8_t b = off ? 0 : (uint8_t)(rgb888 & 0xFF);
    // Active LOW, and only ever fully on or off: the three channels share no
    // LEDC timer with the backlight and a half-lit indicator reads as a fault.
    digitalWrite(PIN_LED_R, r > 127 ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g > 127 ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b > 127 ? LOW : HIGH);
}

// ── TOUCH ────────────────────────────────────────────────────────────────
static XPT2046_Touchscreen s_ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
static uint32_t          s_lastTouch = 0;
static bool              s_wasDown   = false;

// A 4-point linear fit taken from an inset rectangle. Accuracy degrades toward
// the bezel, so y = 0..31 (the header) is EXTRAPOLATED, not interpolated --
// which is why the header spends 32px on its targets. DESIGN.md §1.
static const int16_t RAW_X0 = 340, RAW_X1 = 3900;
static const int16_t RAW_Y0 = 200, RAW_Y1 = 3750;

void touchBegin() {
    // TFT_eSPI holds HSPI (USE_HSPI_PORT), so the global VSPI object is free
    // for the digitiser -- which is what this library's begin() uses.
    SPI.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    s_ts.begin();
    s_ts.setRotation(SCR_ROTATION);
}

bool touchPoll(Touch &out, uint32_t now) {
    out.down = false;
    TS_Point p = s_ts.getPoint();

    if (p.z < TOUCH_Z_MIN) { s_wasDown = false; return false; }
    if (s_wasDown) return false;                       // one event per press
    if ((int32_t)(now - s_lastTouch) < TOUCH_DEBOUNCE_MS) return false;

    s_wasDown   = true;
    s_lastTouch = now;

    long x = (long)(p.x - RAW_X0) * SCR_W / (RAW_X1 - RAW_X0);
    long y = (long)(p.y - RAW_Y0) * SCR_H / (RAW_Y1 - RAW_Y0);
    if (x < 0) x = 0; if (x >= SCR_W) x = SCR_W - 1;
    if (y < 0) y = 0; if (y >= SCR_H) y = SCR_H - 1;

    out.x = (int16_t)x;
    out.y = (int16_t)y;
    out.down = true;
    return true;
}
