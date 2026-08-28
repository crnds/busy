# cyd-busybar

A [BUSY Bar](https://busy.app) clone for the **CYD 2.8" ESP32** (ESP32-2432S028R): the BUSY Bar's display features — Canvas draw API, status themes, clock, Wi-Fi setup — running on an ILI9341 320×240 SPI panel with XPT2046 resistive touch, an LDR for auto-brightness, and an RGB status LED.

The firmware speaks the original BUSY Bar's HTTP draw API, so anything that talks to a BUSY Bar (scripts, Home Assistant, whatever) can drive this device unmodified.

## What's in the repo

```
brief.md            The original plan: scope, architecture, out-of-scope list.
DESIGN.md           The design system (tokens, type, spacing, components).
                    The SOURCE for the visual system; its functional
                    constraints (devices, scenes, three-page structure)
                    do not apply to this firmware.
cyd-busybar/        The firmware (PlatformIO project).
  README.md         Device documentation: build/flash, first boot, Wi-Fi
                    flows, full API reference, simulator usage.
  DESIGN.md         How the root design system is applied here, and where
                    it deliberately deviates (one panel, strip modes, …).
  CLAUDE.md         Engineering memory: module map and hardware traps
                    (read this before touching hardware-adjacent code).
  include/config.h  All layout coordinates live here — the only place they
                    are allowed to.
  simulator.html    Browser simulator mirroring config.h, for UI work
                    without hardware.
```

## Quickstart

Prereqs: [PlatformIO Core](https://docs.platformio.org/) (CLI).

```sh
cd cyd-busybar
pio run                 # build (env: cyd)
pio run -t upload       # flash (115200 baud)
pio run -t uploadfs     # flash the data partition (themes, default config)
pio device monitor      # serial log
```

First boot: the device opens an open access point, **`cyd-busybar-setup`**, with a captive portal — join it, and set your Wi-Fi from the panel or the portal. Once connected it is reachable at **`http://cyd-busybar.local`** (mDNS; the hostname is configurable).

The screen has two tabs — **Status** (the 160×80 virtual panel at 2×, full-bleed, with a theme strip below it) and **Settings** (Wi-Fi, brightness, night mode, clock).

For the full API reference, draw examples, and Wi-Fi change flows, see [`cyd-busybar/README.md`](cyd-busybar/README.md).

## Out of scope

By decision: Pomodoro, MQTT, BLE, Matter, the JS app platform, audio, OTA, and the busy.app cloud. See `brief.md` for the full list.

## Agent instructions

See [AGENTS.md](AGENTS.md) for build/verify commands and the conventions that keep this codebase consistent.
