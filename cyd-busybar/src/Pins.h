#pragma once

// ESP32-2432S028R "Cheap Yellow Display" pin map.
// Touch and SD are NOT on the TFT SPI bus.

#define PIN_TFT_MOSI 13
#define PIN_TFT_MISO 12
#define PIN_TFT_SCLK 14
#define PIN_TFT_CS   15
#define PIN_TFT_DC   2
#define PIN_TFT_RST  -1
#define PIN_TFT_BL   21

#define PIN_TOUCH_SCLK 25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS   33
#define PIN_TOUCH_IRQ  36

#define PIN_LDR   34
#define PIN_LED_R 4     // active LOW
#define PIN_LED_G 16
#define PIN_LED_B 17

#define BL_LEDC_CH    0
#define BL_LEDC_FREQ  5000
#define BL_LEDC_BITS  8

#define LED_R_CH 1
#define LED_G_CH 2
#define LED_B_CH 3

// XPT2046 raw ADC → 320×240 landscape. Tune from serial "touch dbg".
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800
#define TOUCH_Z_MIN 95

// LDR: divider reads LOWER in brighter light (verified on sibling CYD units).
#define LDR_BRIGHT_RAW 300
#define LDR_DARK_RAW   3800
