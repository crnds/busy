#include "Apps.h"
#include "../canvas/Canvas.h"
#include "../net/Net.h"
#include "../ui/theme.h"
#include "../../include/config.h"
#include <stdio.h>
#include <string.h>

// The AP's details do not change while it is up, so this redraws on a state
// change rather than on a clock. Nothing here animates: a notice that blinks
// competes with the canvas content the panel exists to show.
static bool s_dirty = true;
static bool s_lastUp = false;

void setupInvalidate() { s_dirty = true; }

void setupRender(uint32_t now) {
    (void)now;
    if (canvasOwns()) return;

    bool up = apActive();
    if (up != s_lastUp) { s_lastUp = up; s_dirty = true; }
    if (!s_dirty) return;
    s_dirty = false;

    char ip[24];
    snprintf(ip, sizeof(ip), "%s", apIP().toString().c_str());

    // Amber, not the accent: this is a state to act on, not a selection.
    const uint16_t warn = rgb888to565(0xF5A524);

    vd::clear(0);
    vd::drawRect(0, 0, VD_W, VD_H, warn);
    vd::text(VD_W / 2, 14, "WI-FI SETUP", warn,                 VF_SMALL, 1, VA_MC);
    // The SSID is 17 characters and 101px at 1x, so there is no room to make
    // it larger -- and it is the string you actually have to read.
    vd::text(VD_W / 2, 34, apSsid(),      rgb888to565(0xF4F6FA), VF_SMALL, 1, VA_MC);
    vd::text(VD_W / 2, 56, ip,            rgb888to565(0x18BCF2), VF_SMALL, 2, VA_MC);
}
