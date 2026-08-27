#include "Apps.h"
#include "../canvas/Canvas.h"
#include "../settings/Settings.h"
#include "../ui/screen.h"
#include "../net/Net.h"

static AppId s_active = APP_STATUS;

void appBegin() {
    themeAppBegin();
    s_active = APP_STATUS;
}

AppId appActive() { return s_active; }

void appSetActive(AppId id) {
    if (id >= APP_COUNT || id == s_active) return;
    bool wasDisplay  = (s_active != APP_SETTINGS);
    bool nowDisplay  = (id != APP_SETTINGS);
    s_active = id;
    // A wipe only makes sense between the two panel apps. Entering settings
    // replaces the whole body, which screenInvalidate() already repaints.
    if (wasDisplay && nowDisplay) chromeWipe();
    vd::invalidate();
    screenInvalidate();
}

void appTick(uint32_t now) {
    // The canvas moves first: it decides which panels it holds this frame.
    canvasTick(now);

    // If the canvas holds the panel there is nothing left for a local app to
    // draw on. Settings draws on the CYD's own chrome, so it leaves the panel
    // to whichever panel app was last active -- the status theme, by default.
    if (canvasOwns()) return;

    // The setup AP outranks the status app. It is the only place the SSID and
    // address are readable from across the room, and an open AP that nothing
    // announces is the one that gets left running.
    if (apActive()) { setupRender(now); return; }

    AppId panelApp = (s_active == APP_SETTINGS) ? APP_STATUS : s_active;
    if (panelApp == APP_STATUS) themeRender(now);
}
