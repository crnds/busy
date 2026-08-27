#include "Apps.h"
#include "../canvas/Canvas.h"
#include "../settings/Settings.h"
#include "../net/Net.h"
#include "../ui/theme.h"
#include "../../include/config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static uint32_t s_lastHalf = 0xFFFFFFFFu;
static int  s_lastSec  = -1;
static bool s_lastHave = false;

static const char *WD[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
static const char *MO[12] = { "JAN","FEB","MAR","APR","MAY","JUN",
                              "JUL","AUG","SEP","OCT","NOV","DEC" };

// The panel shows "--:--" until NTP has answered. Showing 00:00 would be a
// calm-looking lie, and the first principle of this UI is not to tell one.
static void fmtTime(char *out, size_t n, const struct tm &t, bool have, bool colon) {
    if (!have) { snprintf(out, n, "--%c--", colon ? ':' : ' '); return; }
    int h = t.tm_hour;
    if (!CFG.clock24) { h = h % 12; if (!h) h = 12; }
    snprintf(out, n, "%02d%c%02d", h, colon ? ':' : ' ', t.tm_min);
}

void clockRender(uint32_t now) {
    if (canvasOwns()) return;

    struct tm t;
    bool have = timeNow(t);
    if (!have) memset(&t, 0, sizeof(t));

    // A colon that blinks is the cheapest possible proof the board is alive.
    uint32_t half = now / 500;
    bool colon = (half & 1) == 0;

    // Redraw exactly when the drawn content would differ. Keying off seconds
    // alone silently drops every other blink, since the blink is twice the
    // rate of the value it sits in.
    if (half == s_lastHalf && t.tm_sec == s_lastSec && have == s_lastHave) return;
    s_lastHalf = half;
    s_lastSec  = t.tm_sec;
    s_lastHave = have;

    char hhmm[12];
    fmtTime(hhmm, sizeof(hhmm), t, have, colon);

    // 160x80, full colour. "23:45" at 4x is 116px wide and 28 tall, which is
    // what sets the whole layout: the time gets the top half and the two
    // supporting lines share the bottom.
    vd::clear(0);

    uint16_t timeCol = have ? rgb888to565(0xF4F6FA) : rgb888to565(0xF5A524);
    vd::text(VD_W / 2, 26, hhmm, timeCol, VF_SMALL, 4, VA_MC);

    char line[32];
    if (have) snprintf(line, sizeof(line), "%s %02d %s",
                       WD[t.tm_wday % 7], t.tm_mday, MO[t.tm_mon % 12]);
    else      snprintf(line, sizeof(line), "WAITING FOR TIME");
    vd::text(VD_W / 2, 54, line, rgb888to565(0xA6B0C2), VF_SMALL, 1, VA_MC);

    if (have) {
        if (CFG.clock24) snprintf(line, sizeof(line), ":%02d", t.tm_sec);
        else snprintf(line, sizeof(line), ":%02d %s", t.tm_sec, t.tm_hour < 12 ? "AM" : "PM");
        vd::text(VD_W / 2, 68, line, rgb888to565(0x18BCF2), VF_SMALL, 1, VA_MC);
    }
}
