# AGENTS.md — working in this repo

Firmware for the CYD 2.8" ESP32 (BUSY Bar clone). The buildable project is
`cyd-busybar/`; the root holds the plan (`brief.md`) and the source design
system (`DESIGN.md` — the visual-system source, not this firmware's spec).

## Read first

- `cyd-busybar/CLAUDE.md` — engineering memory: module map and hardware
  traps (SPI bus split, backlight-before-mount, LDR polarity, RGB order,
  touch calibration inset). Read it before touching hardware-adjacent code.
- `cyd-busybar/DESIGN.md` — how the root design system is applied and where
  it deliberately deviates.
- `cyd-busybar/README.md` — device doc and API reference.

## Build & verify

```sh
cd cyd-busybar
pio run                 # the only env is `cyd`
pio run -t upload       # flash
pio run -t uploadfs     # data partition (themes, config.default.json)
pio device monitor      # 115200
```

UI work without hardware — the browser simulator mirrors `config.h`:

```sh
python3 simulator/gen_sim_fonts.py   # regenerate sim fonts if Font 2/GLCD change
python3 simulator/gen_vfont.py       # regenerate the panel's 3x5/5x7 vfont
python3 simulator/build_sim.py       # build simulator.html
node simulator/sim_check.js          # headless sweep; non-zero exit on failure
python3 simulator/font_metrics.py --check
```

There is no C++ test suite. Verification is: it compiles, `sim_check.js`
passes, and (when hardware is available) the serial log + screen behaviour.
Measured at last check: RAM 100,040 B (30.5%), flash 1,000,201 B (31.8%) —
keep it comfortably below the `huge_app.csv` limits.

## Load-bearing conventions

- **`include/config.h` is the ONLY place a coordinate may live.** Nothing in
  `screen.cpp` (or elsewhere) computes a position from a literal, and
  `simulator.html` mirrors this file. Add the constant to `config.h` first,
  then use it in both places.
- **Dirty-region rendering is structural.** Invalidate on *value* change, not
  colour change — a palette swap (night mode) needs an explicit
  `fillScreen` + full invalidate, because comparing colours would miss it.
  `vd::present()` composites the 160×80 RGB565 framebuffer at 2× into
  320×160 full-bleed.
- **Fonts:** only GLCD and Font 2 are loaded (see `platformio.ini`); no
  GFX font headers outside `gfx.cpp`. The panel's second font system is the
  3×5 `VF_TINY` + 5×7 `VF_SMALL` in `vfont.inc`.
- **Wi-Fi credentials live in NVS** (Preferences, namespace `"wifi"`), NOT in
  `/config.json` — that is why they survive `-t upload`/`-t uploadfs`.
- **AsyncTCP handlers run off the loop.** Connection-breaking actions
  (reboot, AP start/stop, disconnect) must be deferred via the `PendingKind`
  mechanism + `API_DEFER_MS` and executed in `apiTick()` — never inline in a
  handler. Route registration order matters (prefix matching).
- **One-tap, reversible actions; no dialogs.** The UI never blocks for
  confirmation. The setup AP self-closes after 15 min (the fallback portal
  with no stored credentials never times out — closing it would strand the
  device).
- **TFT_eSPI is configured entirely via `build_flags`**
  (`USER_SETUP_LOADED`) so a library reinstall can't change the pinout.
  `TFT_RGB_ORDER=1` is required for this panel batch — flip that one line if
  a different board shows red/blue swapped.
- **Backlight off before LittleFS mount** (`blBegin()` first in `setup()`),
  or the panel flashes at 100% for the length of the mount.

## Docs to keep honest

- `cyd-busybar/README.md` — user-facing; update when tabs, themes, or API
  paths change.
- `cyd-busybar/DESIGN.md` §10 mentions a `calib` PlatformIO env that does not
  exist — do not propagate that reference.
- `simulator.html` must track `config.h` layout changes.
