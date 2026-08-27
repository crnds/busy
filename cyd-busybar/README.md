# cyd-busybar

A BUSY Bar clone for the **CYD 2.8"** (ESP32-2432S028R): the original's panel
emulated at its native resolution and presented full-screen, with the same HTTP
draw API so tooling written for the real device mostly works against this one.

```
┌────────────────────────────────────────────┐
│ ▟▙  Clock   Status   Settings        23:45 │  header,  32px
├────────────────────────────────────────────┤
│                                            │
│                 23:45                      │  panel,  160×80 → ×2
│               WED 27 AUG                   │          full-bleed 320×160
│                  :07                       │
│                                            │
├────────────────────────────────────────────┤
│ [                  Clock                  ] │  strip,   48px
└────────────────────────────────────────────┘
```

On the **Status** tab the strip becomes five tabs — `[BUSY][MEETING][DND]
[CODING][LUNCH]` — so a theme is picked directly rather than stepped to.

## What it does

- **One virtual panel.** 160×80 RGB565 — the BUSY Bar's own resolution, so
  draw coordinates from the original API land where the caller meant them —
  presented at a flat 2× across the full width of the screen.
- **A canvas engine.** Text (with width-limited auto-scroll), rectangles,
  images, animations, countdowns and inline bitmaps — namespaced by
  application, arbitrated by priority, expiring on their own.
- **A clock**, NTP-synced against a POSIX timezone string.
- **Status themes** — BUSY, MEETING, DND, CODING, LUNCH — as label, colours
  and a named procedural effect, so a theme is a few hundred bytes of JSON.
- **A REST API** on the original's paths, plus a device-hosted web UI with a
  live panel preview.
- **Auto-brightness** from the onboard LDR, and a red-only night mode at 1%
  backlight.

Out of scope by decision: Pomodoro/busy timer, MQTT, BLE, Matter, the JS app
platform, audio, OTA, and the busy.app cloud account.

## Build and flash

```sh
pio run                 # compile
pio run -t upload       # flash the firmware
pio run -t uploadfs     # flash data/ (web UI, themes, config) to LittleFS
pio device monitor      # 115200 baud
```

Both partitions are needed on a first flash — the web UI and themes live in the
LittleFS image, not in the firmware.

## First boot

With no credentials stored the device brings up an open AP called
**`cyd-busybar-setup`**. Join it, open any page, and the captive portal serves
the web UI; enter your SSID and password and it reboots onto your network.

After that it is at `http://cyd-busybar.local` (and advertises `_busybar._tcp`
for discovery).

**Credentials are stored in NVS**, not in the LittleFS filesystem image, so
they survive `pio run -t upload` and `pio run -t uploadfs` — a reflash of
either does not send the device back to setup. Only `pio run -t erase` (or an
explicit `Forget`, below) clears them.

## Changing Wi-Fi later

Once the device is on a network you no longer have to erase anything to change
it. **Settings → Network → `AP MODE`** raises the setup access point *alongside*
the existing connection, so nothing is disconnected and no credentials are
touched. The panel switches to showing the SSID and address while it is up,
and it closes itself after 15 minutes.

`RECONNECT` re-joins the stored network without a reboot. `FORGET` clears the
stored SSID and password and raises the AP in their place, in one tap — no
confirmation step, the same as everything else on this device, because the
fallback AP it hands you to is the identical route back in a never-configured
device already has. All three actions are on the web UI and at
`POST /api/wifi/ap`, `POST /api/wifi/reconnect` and `POST /api/wifi/forget`,
and the web UI adds a network scan so the SSID can be picked rather than typed.

With no credentials stored the AP is the only way in, so it never times out and
the `AP MODE` and `FORGET` chips are disabled — there is nothing there to
toggle or to forget.

## API

Open by default. Set `api_token` in `/config.json` to require an
`X-API-Token` header on every `/api/*` route except `/api/version`.

| Method | Path | |
|---|---|---|
| `POST` | `/api/display/draw` | Draw elements. `409` if a higher-priority application owns a target panel |
| `DELETE` | `/api/display/draw` | Clear. `?application_name=` and `?display=` narrow it |
| `PUT` | `/api/display/brightness` | `{"value":0-100}` or `{"auto":true}` |
| `GET` | `/api/screen` | 24-bit BMP straight out of the framebuffer |
| `POST` | `/api/input?key=ok\|back\|up\|down` | Inject a key |
| `GET`/`PUT` | `/api/time` | Timezone, 12/24-hour, re-sync |
| `GET`/`PUT` | `/api/themes` | List and select |
| `GET` | `/api/wifi` | Station and AP state |
| `PUT` | `/api/wifi` | Credentials; verifies NVS write and reboots after response |
| `POST` | `/api/wifi/ap` | `{"enabled":true,"timed":true}` — start/stop the setup AP (takes effect after response) |
| `GET`/`POST` | `/api/wifi/scan` | Async scan: `GET` starts and polls, `POST` restarts |
| `POST` | `/api/wifi/reconnect` | Re-join the station without a reboot (takes effect after response) |
| `POST` | `/api/wifi/forget` | Clear stored credentials and raise the AP; no reboot (takes effect after response) |
| `POST` | `/api/assets/upload` | Multipart upload to `/assets/` |
| `GET` | `/api/status`, `/api/version`, `/api/assets` | |

### Drawing

```sh
curl -X POST http://cyd-busybar.local/api/display/draw \
  -H 'Content-Type: application/json' -d '{
    "application_name": "deploy",
    "priority": 70,
    "led_notification_color": "#FF0000",
    "elements": [
      { "id": "l1", "type": "text",
        "x": 80, "y": 24, "align": 4, "scale": 3,
        "text": "BUILD", "color": "#18BCF2" },
      { "id": "l2", "type": "countdown",
        "x": 80, "y": 56, "align": 4, "scale": 2,
        "target": 1735689600, "timeout_ms": 600000 }
    ]
  }'
```

Elements carry `id` (replace in place), `type`, `x`/`y`/`w`/`h`, `align` (0–8,
nine-point), `z`, `color`, and `timeout_ms` or `display_until`. A `display`
field is still **accepted** — 0 and 1 are both valid — because BUSY Bar tooling
sends it, but the board has one screen and both land on the same panel.
Text adds `font` (`small` 5×7 or `tiny` 3×5), `scale`, and `scroll` with
`scroll_rate_ms` / `scroll_start_delay_ms` / `scroll_repeat_delay_ms`.
Animations take a `path` and `frame_ms`; `xpm` takes hex-encoded packed 1bpp
`bits`.

Image and animation assets are raw RGB565 with a short header —
`'B','I' | w | h` for a still, `'B','A' | w | h | frames` for a loop, all
little-endian uint16.

## Layout on the CYD

`320 × 240`, and every band tiles exactly:

```
header   y   0.. 31    26 (Wi-Fi) + 3 × 81 (tabs) + 51 (clock) = 320
panel    y  32..191    160 × 80 at ×2 = 320 × 160, full-bleed
strip    y 192..239    a chip, or five theme tabs on the Status tab
```

The strip names what is on the panel — `Clock`, `Wi-Fi setup`, or the
application that took it over — since a full-bleed raster leaves no side
gutters for that. On the Status tab, with nothing else claiming the panel, it
becomes a full-width row of theme tabs instead, so a theme is picked directly
rather than stepped to one at a time.

The Settings tab replaces the body with four 52px rows. See `DESIGN.md`.

## Verifying without hardware

`simulator.html` draws the panel's real glyph data and re-checks the layout:

```sh
python3 scripts/gen_sim_fonts.py    # TFT_eSPI's faces, for the chrome
python3 scripts/gen_vfont.py        # the two panel faces
python3 scripts/build_sim.py        # assemble simulator.html
node scripts/sim_check.js           # headless sweep
python3 scripts/font_metrics.py --check
```

Open `simulator.html` and drive it with `?app=2&night=1&conn=2&theme=dnd`.
