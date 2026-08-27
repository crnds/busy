#include "Apps.h"
#include "../ui/screen.h"
#include "../ui/theme.h"
#include "../../include/config.h"

void chromeSplash(uint32_t now, uint8_t pip) { screenSplash(now, pip); }

void chromeWipe() {
    TFT_eSPI *t = screenTft();
    if (!t) return;
    // A horizontal mask across the body, mirroring the original's transition.
    // 16px columns at ~4ms is about 80ms end to end -- long enough to read as
    // motion, short enough that it never delays the tap it is acknowledging.
    for (int x = 0; x < SCR_W; x += 16) {
        t->fillRect(x, BODY_Y, 16, BODY_H, C_BG);
        delay(4);
    }
}
