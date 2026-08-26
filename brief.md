Plan: CYD 2.8" BUSY Bar clone firmware ("cyd-busybar")                                                  │
   │                                                                                                         │
   │ Goal                                                                                                    │
   │                                                                                                         │
   │ An Arduino/PlatformIO firmware for the CYD 2.8" (ESP32-2432S028R, ILI9341 320×240 SPI, XPT2046 touch,   │
   │ onboard LDR GPIO34, RGB LED) that clones the BUSY Bar's display features — no Pomodoro timer, no        │
   │ MQTT/BLE/Matter/JS.                                                                                     │
   │                                                                                                         │
   │ Selected scope (from user):                                                                             │
   │ • Display: Canvas draw API, Clock face, Status themes, Status bar + chrome (boot/transition animations, │
   │   menus)                                                                                                │
   │ • Remote control: HTTP REST API only                                                                    │
   │ • Extras: device-hosted web UI, auto-brightness via LDR                                                 │
   │ • Stack: Arduino / PlatformIO                                                                           │
   │                                                                                                         │
   │ Reference source (already cloned, read-only): /Users/eunitembam3/busy/busybar-firmware                  │
   │ Key API reference: busybar-firmware/applications/services/web_server/openapi/assets.yaml +              │
   │ http_api/api_display.c (draw element schema), applications/services/canvas/ (compositing model),        │
   │ applications/main/clock/, applications/main/busy/ themes.                                               │
   │                                                                                                         │
   │ Architecture                                                                                            │
   │                                                                                                         │
   │ Single-screen emulation of BUSY Bar's dual displays on 320×240 portrait-ish layout:                     │
   │ • Virtual front display: 72×16 24-bit RGB framebuffer, rendered scaled 4× → 288×64 px region (keeps     │
   │   original asset/layout compatibility).                                                                 │
   │ • Virtual back display: 160×80 1-bit (grayscale-simulated) framebuffer, rendered 1.5–2× in a region     │
   │   below.                                                                                                │
   │ • Status bar strip at top: Wi-Fi + time + volume icons (8×8 style, no battery on CYD).                  │
   │ • Screen layout: status bar (16px) / front region (64px) / back region (160px) — fits 320×240 landscape │
   │   rotated or portrait; decide rotation in code, default landscape 320×240.                              │
   │                                                                                                         │
   │ Core modules (PlatformIO project in ~/busy/cyd-busybar/):                                               │
   │                                                                                                         │
   │ 1. Scaffold (platformio.ini, src/main.cpp)                                                              │
   │     • env esp32-2432s028r; libs: TFT_eSPI (User_Setup for CYD: ILI9341, SPI pins, backlight GPIO21      │
   │       PWM), XPT2046_Touchscreen, ArduinoJson, ESPAsyncWebServer (or built-in WebServer — prefer         │
   │       AsyncWebServer for upload handling), LittleFS, NTPClient/configTime, AnimatedGIF or custom        │
   │       raw-frame player.                                                                                 │
   │ 2. Display HAL (src/display/)                                                                           │
   │     • Two virtual framebuffers (uint16_t 565 for front 72×16, 1-bit bitmap for back 160×80), compositor │
   │       that upscales + blits dirty regions to TFT via DMA sprites (TFT_eSprite) to avoid flicker.        │
   │     • Backlight control: LEDC PWM on GPIO21; auto-brightness task reads LDR (GPIO34, note: LDR is       │
   │       inverted/active-low on CYD — verify) every 500ms, maps lux→PWM with gamma curve; manual override  │
   │       via API.                                                                                          │
   │ 3. Canvas engine (src/canvas/) — the centerpiece, mirrors BUSY Bar's model                              │
   │     • Element types: text (font, color, align, width-limited auto-scroll with rate/start/repeat         │
   │       delays), image (from LittleFS assets), animation (looping raw 565 frame packs), countdown         │
   │       (to/from unix timestamp), rectangle, xpm (inline bitmap).                                         │
   │     • Per-element: id, x/y, 9-point align, z-index, timeout/display-until, target display (front/back). │
   │       Cap 100 elements like original.                                                                   │
   │     • Request-level: priority (1–100, 409 conflict semantics), application_name namespacing + selective │
   │       clear, led_notification_color → CYD onboard RGB LED.                                              │
   │     • JSON parsing with ArduinoJson (use PSRAM-backed or sufficiently large doc; ESP32 has enough RAM). │
   │ 4. Apps (src/apps/) — an app owns the screen at a priority; simple app-manager with priority            │
   │    arbitration                                                                                          │
   │     • Clock app (default home): big HH:MM (blinking colons optional), seconds, date line; 12/24h        │
   │       setting; NTP via configTime + POSIX TZ string from settings.                                      │
   │     • Status themes app: theme picker (touch left/right), themes = folder in LittleFS /themes/<name>/   │
   │       with theme.json (label text + looping background animation frames + colors). Port a few originals │
   │       conceptually: ON AIR, MEETING, DND, CODING, LUNCH (redraw simplified frames — originals are       │
   │       proprietary zips, recreate simple ones).                                                          │
   │     • Chrome: boot animation, screen-wipe transition between apps (horizontal mask like original),      │
   │       message/QR screen utility.                                                                        │
   │     • Touch input: soft buttons / swipe regions emulating BUSY Bar's mode-switch positions (Clock /     │
   │       Status / Apps / Settings) + OK/Back.                                                              │
   │ 5. Settings (src/settings/) — persisted to LittleFS JSON (/config.json): Wi-Fi creds, TZ, 12/24h,       │
   │    brightness mode (auto/manual), theme. On-device settings menu (touch) mirroring BUSY Bar's settings  │
   │    list (Wi-Fi, Time, Brightness, Sound→skip, About).                                                   │
   │ 6. HTTP API (src/api/) — subset of BUSY Bar API, same paths so existing BUSY Bar tooling partially      │
   │    works:                                                                                               │
   │     • POST/DELETE /api/display/draw (full canvas schema above)                                          │
   │     • PUT /api/display/brightness                                                                       │
   │     • GET /api/screen?display=0|1 → BMP screenshot of virtual framebuffer                               │
   │     • POST /api/input?key=ok|back|up|down → injects into app manager                                    │
   │     • POST /api/assets/upload → LittleFS (multipart/octet-stream)                                       │
   │     • GET /api/status, /api/version (report compat version), /api/time (GET/PUT, tzlist minimal)        │
   │     • Optional simple token auth (X-API-Token) mirroring original's open/token modes.                   │
   │ 7. Web UI (data/www/ → LittleFS)                                                                        │
   │     • Single-page vanilla HTML/JS (no build step): live screen preview (polls /api/screen ~2fps),       │
   │       draw-element composer (text/image/countdown forms → POST draw), theme picker, brightness slider,  │
   │       clock settings. Served from /.                                                                    │
   │ 8. Connectivity glue: Wi-Fi station with credentials from settings + captive-portal fallback AP for     │
   │    first boot (WiFiManager-style, minimal custom), mDNS (cyd-busybar.local, _busybar._tcp TXT for       │
   │    discovery-compat), SNTP.                                                                             │
   │                                                                                                         │
   │ File layout                                                                                             │
   │                                                                                                         │
   │ ```                                                                                                     │
   │   cyd-busybar/                                                                                          │
   │     platformio.ini                                                                                      │
   │     src/                                                                                                │
   │       main.cpp                                                                                          │
   │       display/{DisplayHAL.h,.cpp, Backlight.cpp}                                                        │
   │       canvas/{Canvas.h,.cpp, Elements.*, Fonts.*}                                                       │
   │       apps/{AppManager.*, ClockApp.*, ThemeApp.*, Chrome.*, SettingsApp.*}                              │
   │       api/{HttpApi.*, routes_*.cpp}                                                                     │
   │       net/{WifiSetup.*, Mdns.*, TimeSync.*}                                                             │
   │       settings/Settings.*                                                                               │
   │     data/www/index.html (+ app.js, style.css)                                                           │
   │     data/themes/{on_air,meeting,dnd,coding,lunch}/theme.json + frames                                   │
   │     data/config.default.json                                                                            │
   │ ```                                                                                                     │
   │                                                                                                         │
   │ Build/verify steps                                                                                      │
   │                                                                                                         │
   │ 1. pio run builds; pio run -t upload + -t uploadfs (LittleFS) to flash.                                 │
   │ 2. Verify on hardware in stages: display test pattern → canvas engine unit-ish tests via curl from Mac  │
   │    (curl -X POST :8000-style against device IP with draw JSON) → clock → themes → web UI →              │
   │    auto-brightness (cover LDR, watch PWM).                                                              │
   │ 3. Cross-check draw JSON semantics against busybar-firmware/.../openapi/assets.yaml so the clone stays  │
   │    API-compatible.                                                                                      │
   │                                                                                                         │
   │ Out of scope (confirmed)                                                                                │
   │                                                                                                         │
   │ Pomodoro/busy timer, MQTT, BLE, Matter, JS app platform, audio playback, OTA update client, busy.app    │
   │ cloud account.                                                                                          │
   │                                                                                                         │
   │ Risks / notes                                                                                           │
   │                                                                                                         │
   │ • Original .anim/font assets are zips/LVGL bitmaps — we recreate simplified equivalents rather than     │
   │   port decoders (keeps it simple; avoids license/asset-format work).                                    │
   │ • CYD LDR polarity varies by board revision — verified on hardware in step above.                       │
   │ • No battery on CYD → battery status icon omitted.