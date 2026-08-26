# cyd-busybar

Arduino / PlatformIO firmware that clones the BUSY Bar **display** on a Cheap
Yellow Display (ESP32-2432S028R, 2.8" ILI9341 320×240, XPT2046 touch, LDR,
onboard RGB LED).

No Pomodoro timer, MQTT, BLE, Matter, JS apps, audio, or cloud account.

## Layout

Landscape 320×240:

| Band | Pixels | Contents |
|------|--------|----------|
| Status bar | 16 tall | Wi-Fi, clock, IP (no battery) |
| Front | 288×64 (72×16 × 4) | BUSY Bar front LED clone |
| Back | 320×160 (160×80 × 2) | BUSY Bar back clone + tabs |

Tabs along the bottom of the back display: **CLK / STAT / APP / SET**.
Swipe left/right on Status to change themes (BUSY, MEETING, DND, CODING, LUNCH).

## Simulator

Open `simulator.html` in a browser (no board required). It is a pixel twin of
the 320x240 firmware UI: same 5x7 font, layout, tabs, themes, and draw overlay.
Click the screen like the resistive panel, or use keys 1-4 / arrows / Enter / Esc.

## Build / flash

Needs [PlatformIO](https://platformio.org/). This board's USB-serial is flaky
above 115200.

```bash
cd cyd-busybar
pio run
pio run -t upload          # firmware
pio run -t uploadfs        # LittleFS: web UI + theme metadata
```

First boot with empty Wi-Fi: join AP `CYD-BusyBar-XXXX`, open `192.168.4.1`.
After that: `http://cyd-busybar.local/` or the IP shown on the status bar.

## HTTP API (BUSY Bar subset)

Same paths as `busybar-firmware` so existing tooling partially works.
`api_semver` reports `27.5.0`. Optional `X-API-Token` if configured.

```bash
# text on the front 72×16
curl -X POST http://DEVICE/api/display/draw \
  -H 'Content-Type: application/json' \
  -d '{
    "application_name": "my_app",
    "priority": 50,
    "led_notification_color": "#FF0000FF",
    "elements": [{
      "id": "0",
      "type": "text",
      "text": "Hello, World! Long text",
      "x": 2, "y": 4,
      "font": "bold",
      "color": "#FFFFFFFF",
      "width": 72,
      "scroll_rate": 800,
      "display": "front"
    }]
  }'

curl -X DELETE 'http://DEVICE/api/display/draw?application_name=my_app'
curl 'http://DEVICE/api/screen?display=0' -o front.bmp
curl -X POST 'http://DEVICE/api/display/brightness?value=auto'
curl -X POST 'http://DEVICE/api/input?key=busy'
curl http://DEVICE/api/status
curl http://DEVICE/api/version
```

Draw element types: `text`, `image` (`.bmp` 24-bit or `.rgb565`), `animation`
(raw `A565` frame pack), `countdown`, `rectangle`, `xpmbitmap`.

Upload: `POST /api/assets/upload?application_name=my_app&file=icon.bmp` with
`Content-Type: application/octet-stream`.

## Hardware notes

- TFT: ILI9341 on HSPI (MOSI 13, SCLK 14, CS 15, DC 2). `TFT_BGR` for the
  common CYD panel that swaps red/blue.
- Touch: XPT2046 on its **own** pins (CLK 25, MOSI 32, MISO 39, CS 33) bit-banged.
  Do not put `TOUCH_CS` into TFT_eSPI.
- Backlight PWM on GPIO 21. Auto-brightness reads the LDR on GPIO 34 (inverted:
  brighter room → lower ADC). Cover the LDR and the PWM should drop.
- RGB LED on GPIO 4/16/17, active low. Driven by `led_notification_color`.

## Out of scope

Pomodoro / busy timer, MQTT, BLE, Matter, JS app host, audio, OTA, busy.app cloud.
