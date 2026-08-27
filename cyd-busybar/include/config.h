// config.h — build-wide constants for cyd-busybar.
//
// The LAYOUT block below is the ONLY place a coordinate is allowed to live.
// Nothing in screen.cpp computes a position from a literal; simulator.html
// mirrors this file and nothing else. See DESIGN.md §9.
#pragma once

#include <stdint.h>

// ── IDENTITY ─────────────────────────────────────────────────────────────
#define FW_NAME            "cyd-busybar"
#define FW_VERSION         "1.0.0"
#define FW_API_COMPAT      "1.0.0"     // BUSY Bar API version we claim
#define DEFAULT_HOSTNAME   "cyd-busybar"
#define AP_SSID            "cyd-busybar-setup"

// ── PANEL ────────────────────────────────────────────────────────────────
#define SCR_W              320
#define SCR_H              240
#define SCR_ROTATION       1           // landscape, USB on the left

// ── VIRTUAL DISPLAY ──────────────────────────────────────────────────────
// ONE panel. The CYD has a single screen, so emulating the BUSY Bar's two and
// splitting the body between them was inventing a constraint the hardware does
// not have -- and it cost both panels their size.
//
// What survives is the BACK panel's geometry (160x80, the richer of the two
// and the one the layouts were designed for) with the FRONT panel's rendering
// (RGB565, full colour). The old back panel was one bit deep, so a status
// theme could not show its own colours; now it can.
//
// 160x80 stays as the LOGICAL resolution because it is the real BUSY Bar
// back-panel size -- draw coordinates from the original API still land where
// the caller meant them.
#define VD_W               160
#define VD_H               80
#define VD_SCALE           2           // 160x80 x2 -> 320x160, a flat integer
#define VD_DRAW_W          (VD_W * VD_SCALE)                  // 320
#define VD_DRAW_H          (VD_H * VD_SCALE)                  // 160

// ── SPACING (DESIGN.md §5) ───────────────────────────────────────────────
// Five values. A sixth means the layout is wrong, not that the scale is.
#define SP_1               4
#define SP_2               8
#define SP_3               12
#define SP_4               16
#define SP_6               24

// ── LAYOUT: HEADER, y 0..31 (DESIGN.md §9.1) ─────────────────────────────
// Three regions tile the bar exactly: 26 + 3*81 + 51 = 320.
#define HDR_H              32
#define HDR_DIV_Y          31

#define CONN_X             0
#define CONN_W             26

#define TAB_X              26
#define TAB_W              81
#define TAB_N              3
#define TAB_UNDERLINE_H    2

#define CLK_X              269
#define CLK_W              51
#define CLK_MARGIN_R       6

// ── LAYOUT: BODY, y 32..239 (208px) ──────────────────────────────────────
#define BODY_Y             HDR_H
#define BODY_H             (SCR_H - HDR_H)

// Cards share one x geometry on every page.
#define CARD_X             8
#define CARD_W             304
#define CARD_X1            (CARD_X + CARD_W - 1)              // 311
#define CARD_IN_X0         16                                 // content left
#define CARD_IN_X1         303                                // content right
#define CARD_IN_W          (CARD_IN_X1 - CARD_IN_X0 + 1)      // 288

// ── DISPLAY PAGE: the panel, then a strip ────────────────────────────────
// 32 + 160 + 48 = 240, tiling the screen exactly.
//
// The raster is FULL-BLEED: at 2x it is exactly the screen's width, so there
// is no room for a card around it and no need for one -- the raster IS the
// content, the way a photograph is, and the screen edge is its bezel.
#define RASTER_X           0
#define RASTER_Y           BODY_Y                             // 32

// The strip below it. With the raster full-bleed there are no side gutters
// left for the theme picker, and nothing else names what is on screen or who
// put it there -- which an ownership border alone could never do.
#define STRIP_Y            (RASTER_Y + VD_DRAW_H)             // 192
#define STRIP_H            (SCR_H - STRIP_Y)                  // 48

// CHIP MODE: Clock tab, Wi-Fi setup, or a remote application holding the
// panel. One indicator, so it takes the full content width -- nothing else
// shares the row with it.
#define STRIP_CHIP_X       CARD_IN_X0                         // 16
#define STRIP_CHIP_W       CARD_IN_W                          // 288

// ── SETTINGS PAGE: four row bands (DESIGN.md §9.2) ───────────────────────
// 32 + 4*52 = 240, tiling exactly. Cards inset 2px top and bottom.
#define ROW_N              4
#define ROW_H              52
#define CARD_DY            2
#define CARD_H             (ROW_H - 2 * CARD_DY)              // 48

#define CARD_L1_CY         8      // line-1 text datum, relative to card top
#define CARD_L1_Y0         1      // line-1 dirty rect, relative to card top
#define CARD_L1_Y1         19     // ..covers descenders, stops above the row
#define CTL_DY             20     // control row, relative to card top
#define CTL_H              26

#define CARD_ICO_R         7      // every icon fits a 15x15 box
#define CARD_ICO_X         (CARD_IN_X0 + CARD_ICO_R)          // 23

// Settings control grid: 5 * 54 + 4 * 4 = 286, x 16..301.
#define SET_CHIP_N         5
#define SET_CHIP_W         54
#define SET_CHIP_GAP       SP_1

// The Status tab's theme picker, on the strip below the panel (not a
// settings row) -- five full-width tabs at the SAME pitch as the row above,
// reused rather than a second chip size invented for it. THEME_TAB_N must
// match THEME_DIRS in ThemeApp.cpp; both are compile-time 5.
#define THEME_TAB_N        5
#define THEME_TAB_X0       CARD_IN_X0
#define THEME_TAB_W        SET_CHIP_W
#define THEME_TAB_GAP      SET_CHIP_GAP

// The Network card sets its OWN chip pitch. "RECONNECT" needs 72px of label
// and does not fit the 54px brightness chip -- and per the design system each
// row's pitch is fixed by that row tiling the card, not shared between rows.
// Three chips -- AP mode, Reconnect, Forget -- at 3 * 92 + 2 * 4 = 284, x
// 16..299. The setup AP's own address used to read out in the leftover space
// here; it is dropped now that a third chip needs the room, because the
// address is a fixed constant (softAP always hands out 192.168.4.1) already
// shown, large, on the panel itself the moment the AP comes up -- this row
// was the redundant copy, not the only one.
#define NET_CHIP_N         3
#define NET_CHIP_W         92

// The night toggle: a square knob in a square track, right-aligned.
#define TOG_W              44
#define TOG_H              24
#define TOG_KNOB           18
#define TOG_X              (CARD_IN_X1 - TOG_W + 1)           // 260

// ── TIMING ───────────────────────────────────────────────────────────────
#define PRESS_FLASH_MS     120     // the tactility flash, before anything else
#define TOUCH_DEBOUNCE_MS  180
#define TOUCH_Z_MIN        400
#define LDR_PERIOD_MS      500
#define CLOCK_TICK_MS      200
#define ANIM_TICK_MS       40      // 25 fps ceiling for canvas animations
#define WIFI_CONNECT_MS    20000
#define SPLASH_PIP_MS      200
#define API_DEFER_MS       400     // flush delay before connection-breaking actions
// The setup AP is a transient mode, not a second network. It closes itself so
// an open AP cannot be left broadcasting because somebody walked away, and it
// does not persist across a reboot for the same reason. The fallback portal
// (no credentials stored) has no timeout -- closing that would strand the
// device with no way in at all.
#define AP_TIMEOUT_MS      (15UL * 60UL * 1000UL)

// ── CANVAS ENGINE ────────────────────────────────────────────────────────
#define CANVAS_MAX_ELEMENTS 100    // the original's cap
#define CANVAS_MAX_APPS      8     // distinct application_name namespaces
#define CANVAS_PRIO_MIN      1
#define CANVAS_PRIO_MAX      100
#define APP_NAME_LEN        24
#define ELEM_ID_LEN         24
#define ELEM_TEXT_LEN       64
#define ELEM_PATH_LEN       48

// ── HARDWARE PINS (ESP32-2432S028R) ──────────────────────────────────────
#define PIN_BL             21      // backlight, LEDC PWM
#define PIN_LDR            34      // ambient light, ADC1_CH6
#define PIN_LED_R          4       // onboard RGB LED, all three active LOW
#define PIN_LED_G          16
#define PIN_LED_B          17
#define PIN_TOUCH_CS       33      // XPT2046 on its own VSPI bus
#define PIN_TOUCH_IRQ      36
#define PIN_TOUCH_SCK      25
#define PIN_TOUCH_MOSI     32
#define PIN_TOUCH_MISO     39

// ── BACKLIGHT ────────────────────────────────────────────────────────────
#define BL_LEDC_CH         7
#define BL_LEDC_FREQ       5000
#define BL_LEDC_BITS       8
#define BL_MIN_DUTY        3       // 1% -- the night-mode floor, never 0
#define BL_MAX_DUTY        255

// Touch calibration: accuracy degrades toward the bezel, so the fit is taken
// from an inset rectangle and y=0..31 is extrapolated. DESIGN.md §1.
#define CAL_INSET          30
