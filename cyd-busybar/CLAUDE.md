# cyd-busybar

BUSY Bar display clone for the ESP32-2432S028R (CYD 2.8"). PlatformIO + Arduino.
See `README.md` for flash and HTTP API.

Virtual displays: front 72×16 RGB565 scaled 4×, back 160×80 scaled 2×, 16px
status bar. Canvas engine, clock, status themes, settings, REST API, LittleFS
web UI. No Pomodoro / MQTT / BLE / Matter / JS.

Pin map lives in `src/Pins.h`. TFT_eSPI flags are in `platformio.ini`
(`USER_SETUP_LOADED`, `ILI9341_2_DRIVER`, `TFT_BGR`, HSPI). Touch is bit-banged
on the dedicated XPT2046 pins — never give TFT_eSPI `TOUCH_CS`.

## Simulator lockstep

`simulator.html` is a pixel twin of the firmware screen. Change layout,
colors, 5x7 bits, or app coordinates in **both** `src/` and `simulator.html`.

- Canvas is 320x240 landscape (CSS-scaled 2x, `image-rendering: pixelated`).
- Status 16px / front 72x16 x4 at (16,16) / back 160x80 x2 at (0,80).
- 5x7 bitmap is copied from `src/canvas/font5x7.inc`.
- Tab hits: TFT y>=216, four 80px slots (CLK/STAT/APP/SET). Swipe dx>40 = Up/Down.
- Colors are RGB565 with the same hex as `Types.h` (`COL_ACCENT = 0xFBE0`).
