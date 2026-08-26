# cyd-busybar

BUSY Bar display clone for the ESP32-2432S028R (CYD 2.8"). PlatformIO + Arduino.
See `README.md` for flash and HTTP API.

Virtual displays: front 72×16 RGB565 scaled 4×, back 160×80 scaled 2×, 16px
status bar. Canvas engine, clock, status themes, settings, REST API, LittleFS
web UI. No Pomodoro / MQTT / BLE / Matter / JS.

Pin map lives in `src/Pins.h`. TFT_eSPI flags are in `platformio.ini`
(`USER_SETUP_LOADED`, `ILI9341_2_DRIVER`, `TFT_BGR`, HSPI). Touch is bit-banged
on the dedicated XPT2046 pins — never give TFT_eSPI `TOUCH_CS`.
