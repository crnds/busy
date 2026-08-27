#include "widgets.h"
#include "theme.h"
#include "gfx.h"
#include "../../include/config.h"
#include <ctype.h>
#include <string.h>

static TFT_eSPI *T = nullptr;
void widgetsBegin(TFT_eSPI *tft) { T = tft; }

CtlColour ctlColour(uint8_t vis) {
    switch (vis) {
        // An inactive control has NO BORDER. Deleting the outlines -- not
        // changing any colour -- is the single largest reduction in visual
        // noise in this design.
        case BV_ACTIVE:     return { C_ACCENT,   C_ACCENT,  C_BG };
        case BV_ACTIVE_OFF: return { C_NEUTRAL,  C_NEUTRAL, C_BG };
        // Deliberately the loudest fill the UI ever draws: it must land within
        // PRESS_FLASH_MS, before anything else has answered.
        case BV_PRESSED:    return { C_TEXT,     C_TEXT,    C_BG };
        // Recedes into the background rather than greying out on top of the
        // card: there is no state to show, so it must not look like it has one.
        case BV_DISABLED:   return { C_BG,       C_BG,      C_DISABLED };
        case BV_ERR:        return { C_BG,       C_ERROR,   C_ERROR };
        case BV_INACTIVE:
        default:            return { C_ELEVATED, C_ELEVATED, C_TEXT2 };
    }
}

void wCard(int x, int y, int w, int h, uint16_t fill, uint16_t edge) {
    if (!T) return;
    if (fill != edge) T->fillRect(x + 1, y + 1, w - 2, h - 2, fill);
    else              T->fillRect(x, y, w, h, fill);
    T->drawRect(x, y, w, h, edge);
}

void wChip(int x, int y, int w, int h, const char *label, uint8_t vis) {
    if (!T) return;
    CtlColour c = ctlColour(vis);
    T->fillRect(x, y, w, h, c.fill);
    if (c.edge != c.fill) T->drawRect(x, y, w, h, c.edge);
    if (!label || !*label) return;

    char up[24];
    size_t i = 0;
    for (; label[i] && i < sizeof(up) - 1; i++) up[i] = (char)toupper((unsigned char)label[i]);
    up[i] = '\0';

    int budget = w - 2 * SP_1;
    FontRole r = textFit(F_BODY, up, budget);
    char out[24];
    textTrunc(out, sizeof(out), r, up, budget);
    drawText(r, out, x + w / 2, y + h / 2, c.fg, MC_DATUM);
}

void wToggle(int x, int y, int w, int h, int knob, bool on, uint16_t behind) {
    if (!T) return;
    // The track is an affordance, not the hit area -- the whole row is tappable.
    T->fillRect(x, y, w, h, on ? C_ACCENT : C_DIVIDER);
    int pad = (h - knob) / 2;
    int kx  = on ? (x + w - knob - pad) : (x + pad);
    // A square block, not a disc: the knob's POSITION says on or off, and a
    // circle sliding in a sharp-cornered slot is the one mark that would still
    // read as rounded on an otherwise square page.
    T->fillRect(kx, y + pad, knob, knob, on ? C_BG : behind);
}

void wTab(int x, int y, int w, int h, const char *label, bool selected, int underlineH) {
    if (!T || !label) return;
    uint16_t fg = selected ? C_ACCENT : C_TEXT3;
    // Sentence case here: tabs have the vertical room, and it reads calmer.
    int cy = y + (h - underlineH) / 2;
    drawText(F_BODY, label, x + w / 2, cy, fg, MC_DATUM);
    if (selected) T->fillRect(x + SP_2, y + h - underlineH - 1, w - 2 * SP_2, underlineH, C_ACCENT);
}

void wPip(int cx, int cy, int r, bool on) {
    if (!T) return;
    if (on) T->fillCircle(cx, cy, r, C_ACCENT);
    else    T->drawCircle(cx, cy, r, C_TEXT3);
}
