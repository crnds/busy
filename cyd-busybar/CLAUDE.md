# CLAUDE.md — cyd-busybar

Engineering memory: the hardware constraints and the traps that cost real
debugging time. `README.md` is for someone using the device; `DESIGN.md` is the
design system. Where they overlap, `include/config.h`, `src/ui/theme.h` and
`src/ui/gfx.h` are the source of truth.

## Shape of the thing

An Arduino/PlatformIO firmware for the **CYD 2.8" (ESP32-2432S028R)**. It
emulates the BUSY Bar's panel as a virtual framebuffer and composites it onto
the ILI9341:

```
src/
  vdisp/     one 160x80 RGB565 framebuffer, dirty-box compositor
  canvas/    the element model behind /api/display/draw
  apps/      clock, status themes, boot/transition chrome, app arbitration
  ui/        theme tokens, typography roles, primitive icons, widgets, screen
  api/       the REST surface and the web UI's static route
  net/       Wi-Fi + portal, mDNS, SNTP
  hw/        backlight PWM, LDR, RGB LED, touch
  settings/  /config.json on LittleFS
```

`include/config.h` is the ONLY place a coordinate lives. `simulator.html`
mirrors it and nothing else.

## Hardware traps

**TFT_eSPI is configured from `build_flags`, never by editing the library.**
`USER_SETUP_LOADED` plus the pin defines live in `platformio.ini`, so a library
reinstall cannot silently change the pinout.

**The two SPI buses are not interchangeable.** The panel runs on **HSPI**
(`USE_HSPI_PORT`), which leaves the global `SPI` (VSPI) object free for the
XPT2046. The pinned `XPT2046_Touchscreen` has only `begin()` — no
`begin(SPIClass&)` overload — so it uses the global object, and that only works
because TFT_eSPI is on the other bus. Moving the panel to VSPI breaks touch
with no compile error.

**The backlight must be driven to 0 before LittleFS mounts.** `blBegin()` sets
the LEDC duty to 0 and `settingsBegin()` runs before `screenBegin()`. Skip that
ordering and the panel flashes at 100% for the length of the mount.

**The LDR polarity varies by board revision.** `settingsDutyFor()` treats a
larger raw ADC reading as a DARKER room. If auto-brightness runs backwards on
your board, that inversion is the one line to flip. The reading is smoothed
with a heavy EMA so a hand passing over the sensor cannot step the backlight.

**Panel colour order varies by board batch.** This unit's panel is RGB-ordered,
so `platformio.ini` sets `-DTFT_RGB_ORDER=1`; with TFT_eSPI's default (BGR bit
set in MADCTL) red renders as blue. If a different board shows red/blue
swapped, that flag is the one line to flip — everything above the driver is
standard RGB565 and never needs to change.

**The onboard RGB LED is active LOW** on all three channels, and is driven
fully on or off only — a half-lit indicator reads as a fault.

**Touch is a 4-point linear fit from an INSET rectangle** (`CAL_INSET` 30), so
`y = 0..31` — the whole header — is *extrapolated*, not interpolated. That is
why the header spends 32px on its targets and why the tab strip is the one
place a target is not generous relative to its neighbours.

## Rendering rules

**Dirty-region rendering is structural, not an optimisation.** Every region
returns early unless a compared VALUE changed. Two consequences that have both
already bitten:

- **Compare the state, not the resolved colour.** The panel-card border
  snapshot once stored a 0/1 flag and compared it against a 16-bit colour, so
  both cards repainted their border every frame. Snapshots hold the *meaning*.
- **A palette swap changes no compared value**, so `screenSetNight()` must
  `fillScreen()` and invalidate explicitly. Same for a page change.

**Every panel app must gate its own redraw.** `clockRender()` keys off the
half-second colon phase (keying off seconds alone silently drops every other
blink, since the blink is twice the rate of the value it sits in);
`themeRender()` keys off an `ANIM_TICK_MS` bucket. Without those gates both
panels are cleared and re-blitted on every pass of `loop()`, which is far more
SPI traffic than the panel or the touch poll can afford.

**`vd::present()` only upscales the dirty box.** The front is a flat 4×; the
back is a fixed 3:2 pattern — every other source column doubled — chosen
because integer 2× would need 320×160 and the body has only 208px once the
32px header is paid for. It is a fixed pattern, not a resample, so one-bit
content stays hard-edged.

## Fonts

**Two font systems, deliberately.** The CYD's chrome uses TFT_eSPI's built-in
faces by NUMBER (`setTextFont(2)`), so no glyph table is pulled into a
translation unit. The virtual panels use their own 3×5 and 5×7 faces from
`src/canvas/vfont.inc` — Font 1 is 6×8 in an 8px cell, which leaves 4px of a
72×16 panel and no room for a second line.

**Never include a GFX font header, and never name a face outside `gfx.cpp`.**

**Font 4 is not loaded.** Nothing in the chrome draws a large number, so the
`F_NUM` role was removed and `LOAD_FONT4` is off — about 6 KB of flash. Adding
it back without a role that uses it just re-adds the weight.

**Ink extents are measured, not read off the headers.** Run
`python3 scripts/font_metrics.py --check`; it decodes the real glyph data and
verifies `gfx.cpp`'s tables. `F_BODY_INK_BOT` is a `constexpr` in `gfx.h`
precisely so `screen.cpp` can `static_assert` the row geometry against it —
change a face and the build fails rather than the descenders landing in the
control row.

## Canvas semantics

- **A draw is all-or-nothing.** The whole request is validated before any
  element is applied; a bad type on element three used to leave one and two
  applied behind a 400.
- **Ownership is global.** While any element is live the canvas owns the panel
  and the local app stands down. Between remote callers, priority arbitrates
  and a loser gets 409.
- **The strip's chip is the ownership channel** — a `BV_ACTIVE` fill carrying
  the owning application's name. The full-bleed raster left no card border to
  use, and naming the owner beats implying one. On the Status tab this same
  strip is a five-way theme picker instead, whenever nothing else is claiming
  the panel or the tab — `drawDisplayPage()`'s `tabsMode` and
  `screenTouch()`'s hit test share the identical condition, which is what
  stops the drawn tabs and the tappable ones drifting apart.
- **`/api/screen` reads the framebuffer from the web-server task** while
  `loop()` may be writing it. A torn frame costs one refresh at 2 fps; a mutex
  would put network latency in the render path.

## Regenerating

```sh
python3 scripts/gen_vfont.py         # panel faces -> vfont.inc + sim_vfont.js
python3 scripts/gen_sim_fonts.py     # TFT_eSPI faces -> sim_fonts.js
python3 scripts/build_sim.py         # -> simulator.html
node scripts/sim_check.js            # headless sweep, exits non-zero on failure
python3 scripts/font_metrics.py --check
```

`gen_sim_fonts.py` reads TFT_eSPI out of `.pio/libdeps/`, so run `pio run`
first on a clean checkout. Font 2 is uncompressed rows and its width table
carries a +1 spacing pixel; Font 4 is run-length encoded and its table does
not — that asymmetry is the thing to check first if glyphs come out as noise.

## Settings and flash wear

`settingsSave()` writes `/config.json` on every brightness chip, clock toggle
and theme step. That is a human-rate write on a wear-levelled filesystem and is
accepted; if a UI is ever added that changes settings faster, debounce it.

## Wi-Fi credentials live in NVS, not `/config.json`

**This is the one setting that has to survive a reflash, so it is the one
setting that lives outside `/config.json`.** `pio run -t uploadfs` replaces
the ENTIRE LittleFS partition with whatever is in `data/` — any `/config.json`
the device wrote at runtime is gone, by design, on every filesystem reflash.
That is fine for theme/clock/brightness (they reset to sane defaults), but it
would mean re-entering Wi-Fi credentials after every `-t uploadfs`, which is
the exact friction this device exists to avoid at first boot.

`src/net/WifiSetup.cpp` owns credential storage via the Arduino `Preferences`
library, in its own NVS namespace (`"wifi"`). NVS is a SEPARATE flash
partition (`nvs`, 20 KB, see `huge_app.csv`) that neither `-t upload` (writes
`app0`) nor `-t uploadfs` (writes the `spiffs`/LittleFS region) touches. Only
`-t erase` or a manual `esptool.py erase_flash` clears it.

- `CFG.ssid`/`CFG.pass` stay in the `Config` struct — every existing call site
  keeps reading them unchanged — but `wifiBegin()` populates them from NVS via
  `credsLoad()`, not from `/config.json`. `Settings.cpp`'s `loadFrom()` and
  `settingsSave()` no longer touch those two keys at all.
- `wifiSetCreds(ssid, pass)` writes NVS, verifies the write via readback, and updates
  `CFG` in one call, returning `bool`. It does **not** reboot directly.
- `wifiForget()` clears NVS, clears `CFG.ssid`/`CFG.pass`, drops the station,
  and re-raises the AP in place (`apStart(false)`) — no reboot. It calls
  `apStart()` **unconditionally**, even if the AP is already up: an AP raised
  manually (AP+STA) has `s_apFallback = false` baked in from when it was
  raised, and that flag is now wrong the instant credentials are gone.
  `apStart()` recomputes it from the current (now empty) `CFG.ssid`, which is
  what stops `apStop()` being willing to close an AP that has just become the
  only route back in.

## AsyncTCP and deferred actions

**Handlers run on the AsyncTCP task, not `loop()`.** `AsyncWebServerRequest::send()`
only stages the response in memory — the actual TCP write happens after the handler
returns. Any connection-breaking work — rebooting (`ESP.restart()`), disconnecting
the station (`WiFi.disconnect()`), or starting/stopping the AP (`apStart()` /
`apStop()`) — must **not** run synchronously inside the handler, because destroying
the connection before the response flushes prevents the client from receiving the 200.

- `HttpApi.cpp` stages arguments into a small deferred channel (`PendingKind`,
  `API_DEFER_MS`), sends the 200/state, and returns immediately.
- `apiTick()` in `loop()` executes the actual action once the deadline passes.
- **Route registration order matters:** AsyncWebServer uses prefix matching in
  registration order, so `/api/wifi/scan` MUST be registered before `/api/wifi` (GET).
  Otherwise, GET `/api/wifi/scan` is shadowed by `routeWifiGet` and returns no networks.
- *Latent concurrency note:* `PUT /api/time`, `PUT /api/themes`, and
  `PUT /api/display/brightness` call `settingsSave()` (LittleFS write) from the
  async task, while `screenTouch()` in `loop()` can touch it concurrently. Avoid adding
  heavy flash writes to async handlers.

## The setup AP

**AP mode is additive, and that is the whole point.** `apStart()` runs
`WIFI_AP_STA` when credentials exist: the station is not disconnected and
nothing stored is touched. That is what makes the AP-mode chip specifically
safe as a single tap with no confirmation step. `FORGET` (see the NVS section
above) sits on the same row and IS a credential-destroying action — that one
is justified on a different basis (it hands you back the fallback AP, not
nothing), not on being non-destructive, so do not extend "additive" reasoning
to it if this section gets edited later.

Two ways it comes up, and they behave differently:

- **Fallback** (no credentials): `apIsFallback()`. Never times out and
  `apStop()` refuses — closing it would leave no route to the device at all.
  The chip renders `BV_DISABLED`, which is honest: it cannot act.
- **On request**: times out after `AP_TIMEOUT_MS` (15 min) and does not survive
  a reboot. An open AP that persists because somebody walked away is the
  failure mode being designed out.

**`applyMode()` picks AP+STA from whether credentials EXIST**, not from whether
the station happens to be connected. Keying off the live connection lets a
reconnect tear the AP down in the middle of somebody using it.

**Scanning is asynchronous throughout.** `WiFi.scanNetworks()` blocks for two
to four seconds, and the only two callers would be `loop()` (which owns the
render budget) or the async web task. `GET /api/wifi/scan` starts a scan on the
first call and reports progress after, so the client polls.

**The AP is announced on the panel**, not in the header glyph — `setupRender()`
outranks the clock and the themes. A 5px badge is not readable across a room
and the SSID is the thing you actually need. It is only invisible while a
canvas application holds the panel, and the strip then names that application
while the Settings card still shows the AP.

## Deliberately not built

No confirmation dialogs and no menus — every on-device action is one tap and
reversible, **including `FORGET`** (Settings → Network): it is not reversible
in the sense of undoing itself, but it hands the device back to the identical
fallback-AP state a never-configured one boots into, which is what qualifies
it under the same rule everything else here follows. See "Wi-Fi credentials
live in NVS" above for the mechanism, and DESIGN.md §12 for why an earlier
version of this document argued the opposite and what changed.
