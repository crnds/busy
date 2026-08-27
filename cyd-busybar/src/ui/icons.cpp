#include "icons.h"

static TFT_eSPI *T = nullptr;
void iconsBegin(TFT_eSPI *tft) { T = tft; }

// Three bars, 3px wide with 2px gaps, bottom-aligned: 13px of the 15px box.
static const int8_t BAR_DX[3] = { -6, -1, 4 };
static const int8_t BAR_H[3]  = {  5,  9, 13 };

void icoWifi(int cx, int cy, uint8_t bars, uint16_t fg) {
    if (!T) return;
    for (int i = 0; i < 3; i++) {
        int x = cx + BAR_DX[i];
        int h = BAR_H[i];
        int y = cy + 6 - h + 1;
        if (bars) T->fillRect(x, y, 3, h, fg);
        else      T->drawRect(x, y, 3, h, fg);
    }
}

void icoBadge(int cx, int cy, uint16_t fg, uint16_t behind) {
    if (!T) return;
    T->fillCircle(cx, cy, 3, behind);   // punch a ring so it reads on the bars
    T->fillCircle(cx, cy, 2, fg);
}

void icoSun(int cx, int cy, uint16_t fg) {
    if (!T) return;
    T->fillCircle(cx, cy, 3, fg);
    // Eight rays on the diagonals and the axes, 2px long at r 5..6.
    static const int8_t D[8][2] = {
        {0,-1},{0,1},{-1,0},{1,0},{-1,-1},{1,-1},{-1,1},{1,1}
    };
    for (int i = 0; i < 8; i++) {
        for (int r = 5; r <= 6; r++) {
            T->drawPixel(cx + D[i][0] * r, cy + D[i][1] * r, fg);
        }
    }
}

void icoMoon(int cx, int cy, uint16_t fg, uint16_t behind) {
    if (!T) return;
    T->fillCircle(cx, cy, 6, fg);
    T->fillCircle(cx + 4, cy - 3, 6, behind);
}

void icoClock(int cx, int cy, uint16_t fg) {
    if (!T) return;
    T->drawCircle(cx, cy, 6, fg);
    T->drawFastVLine(cx, cy - 4, 5, fg);   // hour hand, pointing up
    T->drawFastHLine(cx, cy, 4, fg);       // minute hand, pointing right
}
