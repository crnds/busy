// Apps.h — an app owns the virtual panel until the canvas takes it.
//
// Arbitration is deliberately simple: while a live canvas element exists the
// canvas owns the panel and the local app stands down. The tab strip still
// shows which app is underneath, so a remote takeover is never mistaken for a
// navigation change -- the strip under the panel names the owner instead.
#pragma once

#include <stdint.h>
#include "../vdisp/VDisplay.h"

enum AppId : uint8_t { APP_STATUS = 0, APP_SETTINGS, APP_COUNT };

void  appBegin();
void  appTick(uint32_t now);
AppId appActive();
void  appSetActive(AppId id);

// ── ClockApp.cpp ─────────────────────────────────────────────────────────
void clockRender(uint32_t now);

// ── ThemeApp.cpp ─────────────────────────────────────────────────────────
struct StatusTheme {
    char     name[24];
    char     label[24];
    uint16_t fg, bg, accent;
    uint8_t  effect;    // 0 none, 1 blink, 2 sweep, 3 pulse
    uint16_t periodMs;
    bool     loaded;
};

void  themeAppBegin();
void  themeRender(uint32_t now);
bool  themeLoad(const char *name);
const StatusTheme &themeCurrent();
uint8_t themeCount();
const char *themeNameAt(uint8_t i);
void  themeNext(int dir);
// Jump directly to theme index i (the Status tab's full-width tab row), rather
// than stepping relative to the current one.
void  themeSelect(uint8_t i);

// ── SetupApp.cpp ─────────────────────────────────────────────────────────
// Shown on the panel whenever the setup AP is up. It outranks the clock and
// the themes because it is the only place the SSID and address are legible
// from across the room -- an open AP that nothing announces is exactly the
// thing that gets left running.
void setupRender(uint32_t now);
void setupInvalidate();

// ── Chrome.cpp ───────────────────────────────────────────────────────────
// The boot splash: a wordless mark plus three cycling pips. A still logo for
// twenty seconds is indistinguishable from a hung board, and "is it working?"
// is the one question a boot screen has to answer.
void chromeSplash(uint32_t now, uint8_t pip);
// A horizontal wipe between apps, mirroring the original's transition.
void chromeWipe();
