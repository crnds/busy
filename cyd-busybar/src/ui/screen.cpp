#include "screen.h"
#include "theme.h"
#include "gfx.h"
#include "icons.h"
#include "widgets.h"
#include "../../include/config.h"
#include "../apps/Apps.h"
#include "../canvas/Canvas.h"
#include "../net/Net.h"
#include "../apps/Apps.h"
#include "../settings/Settings.h"
#include "../vdisp/VDisplay.h"
#include <string.h>
#include <stdio.h>

// The body tiles exactly, on both pages. Checked here so a change to any one
// constant in config.h fails the build rather than leaving a band that nothing
// ever clears.
static_assert(HDR_H + VD_DRAW_H + STRIP_H == SCR_H,        "display page must tile the screen");
static_assert(VD_DRAW_W == SCR_W,                          "the raster is full-bleed by design");
static_assert(STRIP_CHIP_X + STRIP_CHIP_W - 1 == CARD_IN_X1, "the chip must fill the strip's content width");
static_assert(THEME_TAB_N * THEME_TAB_W + (THEME_TAB_N - 1) * THEME_TAB_GAP <= CARD_IN_W + 2,
              "theme tab row must fit the strip");
static_assert(HDR_H + ROW_N * ROW_H == SCR_H,              "settings rows must tile the screen");
static_assert(CONN_W + TAB_N * TAB_W + CLK_W == SCR_W,     "header regions must tile the bar");
static_assert(SET_CHIP_N * SET_CHIP_W + (SET_CHIP_N - 1) * SET_CHIP_GAP <= CARD_IN_W + 2,
              "settings chip row must fit the card");
// A card's line-1 descenders must stop before the control row begins, and its
// dirty rect must cover them -- clear only the cap box and a renamed label
// leaves the old descenders on the card forever. The extent comes from gfx.h,
// not from a literal, so re-running font_metrics.py fails the build if a face
// change invalidates the row height.
static_assert(CARD_L1_CY + F_BODY_INK_BOT < CTL_DY,  "line-1 descenders reach into the control row");
static_assert(CARD_L1_Y1 >= CARD_L1_CY + F_BODY_INK_BOT, "line-1 dirty rect misses its descenders");
static_assert(CARD_L1_CY + F_BODY_INK_TOP >= CARD_L1_Y0, "line-1 ink starts above its dirty rect");

static TFT_eSPI *T = nullptr;

// ── PRESS FLASH ──────────────────────────────────────────────────────────
// Keyed by KIND as well as index, or a tap on one page would invert the
// same-numbered control on another. It expires by TIME, so every snapshot
// carries a per-item vis byte -- a page that skipped it would leave the
// tapped control inverted forever.
enum PressKind : uint8_t { PK_NONE = 0, PK_TAB, PK_SET, PK_THEME };
static PressKind s_pk = PK_NONE;
static uint8_t   s_pi = 0, s_pj = 0;
static uint32_t  s_pressUntil = 0;

static bool pressed(PressKind k, uint8_t i, uint8_t j = 0) {
    return s_pk == k && s_pi == i && s_pj == j && s_pressUntil != 0;
}
static void flash(PressKind k, uint8_t i, uint8_t j, uint32_t now) {
    s_pk = k; s_pi = i; s_pj = j; s_pressUntil = now + PRESS_FLASH_MS;
}

// ── SNAPSHOTS ────────────────────────────────────────────────────────────
struct HdrSnap  { uint8_t conn, tab; int8_t hh, mm; bool valid; };
struct DispSnap { uint8_t mode, vis, activeTheme; uint16_t srcHash; bool valid; };
struct SetSnap  { uint8_t vis[ROW_N][SET_CHIP_N]; uint8_t conn, bl, ap; bool night, clock24; bool valid; };

static HdrSnap  s_hdr;
static DispSnap s_disp;
static SetSnap  s_set;
static bool     s_full = true;

void screenInvalidate() {
    s_full = true;
    s_hdr.valid = s_disp.valid = s_set.valid = false;
}

void screenBegin(TFT_eSPI *tft) {
    T = tft;
    gfxBegin(tft);
    iconsBegin(tft);
    widgetsBegin(tft);
    screenInvalidate();
}

void screenSetNight(bool on) {
    themeSetNight(on);
    if (T) T->fillScreen(C_BG);
    vd::invalidate();
    screenInvalidate();
}

// ── HEADER ───────────────────────────────────────────────────────────────
static const char *TAB_LABEL[TAB_N] = { "Status", "Settings" };

// 0 = everything reachable, 1 = up but the time is unknown, 2 = no network.
static uint8_t connState() {
    if (wifiState() != NET_ONLINE) return 2;
    return timeSynced() ? 0 : 1;
}

static void drawConn(uint8_t st) {
    T->fillRect(CONN_X, 0, CONN_W, HDR_DIV_Y, C_BG);
    int cx = CONN_X + CONN_W / 2, cy = HDR_DIV_Y / 2;
    if (st == 2) {
        icoWifi(cx, cy, 0, C_ERROR);           // hollow AND red: never colour alone
    } else {
        icoWifi(cx, cy, 3, C_TEXT3);
        if (st == 1) icoBadge(cx + 7, cy - 5, C_WARNING, C_BG);
    }
}

static void drawHeader(uint32_t now) {
    uint8_t conn = connState();
    uint8_t tab  = (uint8_t)appActive();

    struct tm t;
    bool have = timeNow(t);
    int8_t hh = have ? (int8_t)t.tm_hour : -1;
    int8_t mm = have ? (int8_t)t.tm_min  : -1;

    bool first = !s_hdr.valid;
    if (first || s_hdr.conn != conn) { drawConn(conn); s_hdr.conn = conn; }

    if (first || s_hdr.tab != tab || s_pressUntil) {
        for (uint8_t i = 0; i < TAB_N; i++) {
            int x = TAB_X + i * TAB_W;
            T->fillRect(x, 0, TAB_W, HDR_DIV_Y, C_BG);
            bool sel = (i == tab) || pressed(PK_TAB, i);
            wTab(x, 0, TAB_W, HDR_DIV_Y, TAB_LABEL[i], sel, TAB_UNDERLINE_H);
        }
        s_hdr.tab = tab;
    }

    // Minute-rate by design. Do NOT put a per-second value here: it would
    // repaint the bar several times a second for no information.
    if (first || s_hdr.hh != hh || s_hdr.mm != mm) {
        T->fillRect(CLK_X, 0, CLK_W, HDR_DIV_Y, C_BG);
        char buf[8];
        if (!have) snprintf(buf, sizeof(buf), "--:--");
        else {
            int h = hh;
            if (!CFG.clock24) { h = h % 12; if (!h) h = 12; }
            snprintf(buf, sizeof(buf), "%02d:%02d", h, mm);
        }
        drawText(F_TITLE, buf, CLK_X + CLK_W - CLK_MARGIN_R, HDR_DIV_Y / 2,
                 have ? C_TEXT2 : C_TEXT3, MR_DATUM);
        s_hdr.hh = hh; s_hdr.mm = mm;
    }

    if (first) T->drawFastHLine(0, HDR_DIV_Y, SCR_W, C_DIVIDER);
    s_hdr.valid = true;
    (void)now;
}

// ── DISPLAY PAGE ─────────────────────────────────────────────────────────
// The panel, full-bleed, and a strip under it.
//
// At 2x the raster is exactly the screen's width, so there is no card around
// it -- the raster IS the content and the screen edge is its bezel. That costs
// the side gutters a theme control would sit in, and it costs the card border
// that used to signal ownership, which is what the strip is for: it NAMES
// what is on the panel and who put it there, which a border never could.
//
// The strip has two mutually exclusive modes:
//   CHIP  Clock tab, Wi-Fi setup, or a remote application holding the panel --
//         one indicator, so it takes the full content width.
//   TABS  the Status tab, with nothing else claiming the panel: a full-width
//         row of theme tabs, one per theme, so a theme is PICKED directly
//         rather than stepped to. Picking beats stepping once there are only
//         five: five taps to guarantee landing beat up to four to step there,
//         and every option is visible without cycling through the others.

static const char *chipLabel(bool owns, bool ap, uint8_t &vis) {
    if (owns) {
        // Solid accent fill with dark text -- the vis table's "selected"
        // treatment, so a remote takeover changes shape and colour, not colour
        // alone, and the owning application is named rather than implied.
        vis = BV_ACTIVE;
        return canvasOwner();
    }
    vis = BV_INACTIVE;
    return ap ? "Wi-Fi setup" : "Status";
}

static uint16_t hashStr(const char *s) {
    uint16_t h = 0;
    for (; s && *s; s++) h = (uint16_t)(h * 31 + (uint8_t)*s);
    return h;
}

static void stripBg() {
    T->fillRect(0, STRIP_Y + 1, SCR_W, STRIP_H - 1, C_BG);
    T->drawFastHLine(0, STRIP_Y, SCR_W, C_DIVIDER);
}

static void drawStripChip(const char *label, uint8_t vis) {
    int cy = STRIP_Y + STRIP_H / 2;
    stripBg();
    wChip(STRIP_CHIP_X, cy - CTL_H / 2, STRIP_CHIP_W, CTL_H, label, vis);
}

// Folder name, underscore swapped for space -- wChip uppercases it, so
// "busy" reads as "BUSY" with no second copy of the label to keep in sync.
// If a theme.json's own label ever diverges from its folder name this tab
// shows the folder's, not the JSON's; that is the accepted cost of not
// re-parsing all five theme.json files every render this page is up.
static void tabLabel(char *out, size_t n, uint8_t i) {
    const char *src = themeNameAt(i);
    size_t k = 0;
    for (; src[k] && k < n - 1; k++) out[k] = (src[k] == '_') ? ' ' : src[k];
    out[k] = '\0';
}

static uint8_t activeThemeIndex() {
    for (uint8_t i = 0; i < THEME_TAB_N; i++)
        if (!strcmp(themeNameAt(i), themeCurrent().name)) return i;
    return 0;
}

static void drawStripTabs(uint8_t active) {
    stripBg();
    int y = STRIP_Y + STRIP_H / 2 - CTL_H / 2;
    for (uint8_t i = 0; i < THEME_TAB_N; i++) {
        int x = THEME_TAB_X0 + i * (THEME_TAB_W + THEME_TAB_GAP);
        uint8_t vis = pressed(PK_THEME, i) ? BV_PRESSED
                    : (i == active)        ? BV_ACTIVE : BV_INACTIVE;
        char label[16];
        tabLabel(label, sizeof(label), i);
        wChip(x, y, THEME_TAB_W, CTL_H, label, vis);
    }
}

static void drawDisplayPage() {
    bool owns = canvasOwns();
    bool ap   = apActive();
    // Tabs replace the chip only when nothing else is claiming the panel or
    // the tab -- a remote takeover or the setup AP still get the chip, on
    // whichever tab the user happens to be on, because the panel content
    // they are viewing has already changed to match.
    bool tabsMode = !owns && !ap && appActive() == APP_STATUS;
    uint8_t mode  = tabsMode ? 1 : 0;

    bool first = !s_disp.valid;
    if (first) {
        T->fillRect(0, BODY_Y, SCR_W, BODY_H, C_BG);
        vd::invalidate();
    }

    if (tabsMode) {
        uint8_t active = activeThemeIndex();
        if (first || s_disp.mode != mode || s_disp.activeTheme != active || s_pressUntil) {
            drawStripTabs(active);
            s_disp.mode = mode; s_disp.activeTheme = active;
        }
    } else {
        uint8_t vis;
        const char *label = chipLabel(owns, ap, vis);
        uint16_t   h      = hashStr(label);
        if (first || s_disp.mode != mode || s_disp.vis != vis || s_disp.srcHash != h) {
            drawStripChip(label, vis);
            s_disp.mode = mode; s_disp.vis = vis; s_disp.srcHash = h;
        }
    }

    vd::present(T, RASTER_X, RASTER_Y);
    s_disp.valid = true;
}

// ── SETTINGS PAGE ────────────────────────────────────────────────────────
// Four cards on the 4 x 52 row grid, each carrying an icon so the four are
// distinguishable without reading.

static int cardY(int row)  { return BODY_Y + row * ROW_H + CARD_DY; }

// The ONLY function that knows where a control is. The renderer and the hit
// test both go through it, which is what stops the drawn rect and the
// tappable rect drifting apart.
static bool btnRect(int row, int slot, int &x, int &y, int &w, int &h) {
    int cy = cardY(row);
    switch (row) {
        case 0:  if (slot >= SET_CHIP_N) return false; break;   // brightness, 5
        case 1:  if (slot >= 3)          return false; break;   // time, 3
        case 2:  if (slot != 0)          return false;          // night, a toggle
                 x = TOG_X; y = cy + (CARD_H - TOG_H) / 2; w = TOG_W; h = TOG_H;
                 return true;
        case 3:  if (slot >= NET_CHIP_N)  return false;          // network, 2 wide chips
                 x = CARD_IN_X0 + slot * (NET_CHIP_W + SET_CHIP_GAP);
                 y = cy + CTL_DY; w = NET_CHIP_W; h = CTL_H;
                 return true;
        default: return false;
    }
    x = CARD_IN_X0 + slot * (SET_CHIP_W + SET_CHIP_GAP);
    y = cy + CTL_DY;
    w = SET_CHIP_W;
    h = CTL_H;
    return true;
}

static const char *SET_TITLE[ROW_N] = { "Brightness", "Time", "Night mode", "Network" };
static const char *ROW0_CHIP[SET_CHIP_N] = { "Auto", "25", "50", "75", "100" };
static const char *ROW1_CHIP[3] = { "12h", "24h", "Sync" };
static const char *ROW3_CHIP[NET_CHIP_N] = { "AP mode", "Reconnect", "Forget" };

static uint8_t row0Vis(int slot) {
    return (CFG.blMode == slot) ? BV_ACTIVE : BV_INACTIVE;
}
static uint8_t row1Vis(int slot) {
    // Sync is momentary -- no BV_ACTIVE case on purpose, the same reason a
    // stepper button has none. Offline it is DISABLED rather than ERR: there
    // is nothing to sync against, which is an absence of state, not a failed
    // command. The unsynced condition is already carried by the header badge
    // and by the clock reading "--:--".
    if (slot == 2) return (wifiState() == NET_ONLINE) ? BV_INACTIVE : BV_DISABLED;
    return ((slot == 1) == CFG.clock24) ? BV_ACTIVE : BV_INACTIVE;
}

static uint8_t row3Vis(int slot) {
    if (slot == 0) {
        // With no credentials stored the AP is the ONLY way in, so it cannot
        // be turned off. DISABLED is the honest state for a control that
        // cannot act -- the identity line above still names the AP, so the
        // fact that it is up is not lost.
        if (apIsFallback()) return BV_DISABLED;
        return apActive() ? BV_ACTIVE : BV_INACTIVE;
    }
    // Reconnect and Forget are both momentary -- no BV_ACTIVE case, the same
    // reason Sync has none -- and both need something stored to act on: with
    // nothing there, there is nothing to reconnect to and nothing to forget.
    return CFG.ssid[0] ? BV_INACTIVE : BV_DISABLED;
}

static void drawSetIdentity(int row) {
    int cy = cardY(row);
    T->fillRect(CARD_IN_X0, cy + CARD_L1_Y0, CARD_IN_W, CARD_L1_Y1 - CARD_L1_Y0 + 1, C_SURFACE);

    int icx = CARD_ICO_X, icy = cy + CARD_L1_CY;
    switch (row) {
        case 0: icoSun(icx, icy, C_TEXT2); break;
        case 1: icoClock(icx, icy, C_TEXT2); break;
        case 2: icoMoon(icx, icy, C_TEXT2, C_SURFACE); break;
        // The Network card's icon IS the live connectivity state, which is
        // what earns it a glyph rather than a fourth decorative one.
        default: {
            uint8_t st = connState();
            icoWifi(icx, icy, st == 2 ? 0 : 3, st == 2 ? C_ERROR : C_TEXT2);
            if (st == 1) icoBadge(icx + 7, icy - 5, C_WARNING, C_SURFACE);
            break;
        }
    }

    int tx = CARD_ICO_X + CARD_ICO_R + SP_2;
    drawText(F_TITLE, SET_TITLE[row], tx, icy, C_TEXT, ML_DATUM);

    // The right-hand value: what the selection MEANS, not which chip is lit.
    char val[40] = "";
    switch (row) {
        case 0: snprintf(val, sizeof(val), "%s", settingsBrightnessLabel()); break;
        case 1: snprintf(val, sizeof(val), "%s", CFG.tz); break;
        case 2: snprintf(val, sizeof(val), "%s", CFG.night ? "On" : "Off"); break;
        default: {
            // What you would need in order to reach the device right now: the
            // AP's name while it is the only route in, the station address
            // otherwise.
            if (apIsFallback())               snprintf(val, sizeof(val), "%s", AP_SSID);
            else if (wifiState() == NET_ONLINE)
                snprintf(val, sizeof(val), "%s", wifiIP().toString().c_str());
            else                              snprintf(val, sizeof(val), "Offline");
            break;
        }
    }
    if (val[0]) {
        int budget = CARD_IN_X1 - (tx + textW(F_TITLE, SET_TITLE[row]) + SP_2);
        char out[40];
        textTrunc(out, sizeof(out), F_BODY, val, budget);
        drawText(F_BODY, out, CARD_IN_X1, icy, C_TEXT2, MR_DATUM);
    }
}

static void drawSettingsPage(uint32_t now) {
    bool first = !s_set.valid;
    if (first) {
        T->fillRect(0, BODY_Y, SCR_W, BODY_H, C_BG);
        for (int r = 0; r < ROW_N; r++)
            wCard(CARD_X, cardY(r), CARD_W, CARD_H, C_SURFACE, C_BORDER);
    }

    // Every value drawn on an identity line has to be in this compare, or the
    // line goes stale while the control below it updates. blMode is on the
    // brightness card's line as a WORD, not only as a lit chip.
    uint8_t conn = connState();
    uint8_t apState = (uint8_t)(apActive() ? (apIsFallback() ? 2 : 1) : 0);
    if (first || s_set.conn != conn || s_set.bl != CFG.blMode || s_set.ap != apState ||
        s_set.night != CFG.night || s_set.clock24 != CFG.clock24) {
        for (int r = 0; r < ROW_N; r++) drawSetIdentity(r);
        s_set.conn = conn; s_set.bl = CFG.blMode; s_set.ap = apState;
        s_set.night = CFG.night; s_set.clock24 = CFG.clock24;
    }

    for (int r = 0; r < ROW_N; r++) {
        for (int slot = 0; slot < SET_CHIP_N; slot++) {
            int x, y, w, h;
            if (!btnRect(r, slot, x, y, w, h)) continue;

            uint8_t vis;
            if (r == 0)      vis = row0Vis(slot);
            else if (r == 1) vis = row1Vis(slot);
            else if (r == 3) vis = row3Vis(slot);
            else             vis = CFG.night ? BV_ACTIVE : BV_INACTIVE;
            if (r != 2 && pressed(PK_SET, (uint8_t)r, (uint8_t)slot)) vis = BV_PRESSED;

            if (!first && s_set.vis[r][slot] == vis) continue;
            s_set.vis[r][slot] = vis;

            if (r == 2) {
                // No press flash on a toggle: the flip IS the feedback.
                T->fillRect(x, y, w, h, C_SURFACE);
                wToggle(x, y, w, h, TOG_KNOB, CFG.night, C_SURFACE);
            } else {
                const char *label = (r == 0) ? ROW0_CHIP[slot]
                                  : (r == 1) ? ROW1_CHIP[slot]
                                             : ROW3_CHIP[slot];
                wChip(x, y, w, h, label, vis);
            }
        }
    }

    // The Network row's control space is three chips wide with nothing left
    // over -- Forget's chip took the room a fourth, address-readout slot used
    // to have. The AP's address (always 192.168.4.1, a fixed constant) is
    // still shown, large, on the panel itself the moment the AP comes up; this
    // row does not need its own copy of a value that never changes.

    s_set.ap = apState;
    s_set.valid = true;
    (void)now;
}

// ── SPLASH ───────────────────────────────────────────────────────────────
void screenSplash(uint32_t now, uint8_t pip) {
    if (!T) return;
    static bool drawn = false;
    if (!drawn) {
        T->fillScreen(C_BG);
        // The mark is the product: the panel at its real 2:1 aspect ratio,
        // drawn from primitives so night mode recolours it too.
        int cx = SCR_W / 2, cy = SCR_H / 2 - SP_2;
        T->drawRect(cx - 80, cy - 40, 160, 80, C_TEXT3);
        T->fillRect(cx - 68, cy - 12, 136, 24, C_ACCENT);
        drawn = true;
    }
    // Three pips, cycling. Wordless on purpose -- both boot states (joining a
    // network, and waiting in the portal) look identical, so the screen never
    // implies the wrong one.
    int cy = SCR_H / 2 + SP_6 + SP_4;
    for (int i = 0; i < 3; i++)
        wPip(SCR_W / 2 + (i - 1) * SP_4, cy, 4, i == (pip % 3));
    (void)now;
}

// ── TICK ─────────────────────────────────────────────────────────────────
void screenTick(uint32_t now) {
    if (!T) return;

    if (s_pressUntil && (int32_t)(now - s_pressUntil) >= 0) {
        s_pressUntil = 0; s_pk = PK_NONE;
        s_hdr.valid = false;              // repaint whatever was inverted
        s_disp.activeTheme = 0xFF;        // force one more tab-row redraw
        s_set.valid = false;
    }

    if (s_full) {
        T->fillScreen(C_BG);
        s_full = false;
        s_hdr.valid = s_disp.valid = s_set.valid = false;
    }

    drawHeader(now);
    if (appActive() == APP_SETTINGS) { s_disp.valid = false; drawSettingsPage(now); }
    else                             { s_set.valid  = false; drawDisplayPage(); }
}

// ── TOUCH ────────────────────────────────────────────────────────────────
bool screenTouch(int x, int y, uint32_t now) {
    // Header: the tab strip. 81 x 32 each, and the full band counts.
    if (y < HDR_H) {
        if (x < TAB_X || x >= TAB_X + TAB_N * TAB_W) return false;
        uint8_t i = (uint8_t)((x - TAB_X) / TAB_W);
        flash(PK_TAB, i, 0, now);
        appSetActive((AppId)i);
        return true;
    }

    if (appActive() == APP_SETTINGS) {
        int row = (y - BODY_Y) / ROW_H;
        if (row < 0 || row >= ROW_N) return false;

        // A settings toggle's track is an affordance, not the hit area: the
        // whole row counts, at any x.
        if (row == 2) {
            CFG.night = !CFG.night;
            settingsSave();
            screenSetNight(CFG.night);
            return true;
        }

        for (int slot = 0; slot < SET_CHIP_N; slot++) {
            int bx, by, bw, bh;
            if (!btnRect(row, slot, bx, by, bw, bh)) continue;
            // Generous by design: the full 52px row band counts vertically, so
            // the gaps above and below fold into the nearest control.
            if (x < bx || x >= bx + bw) continue;
            flash(PK_SET, (uint8_t)row, (uint8_t)slot, now);
            if (row == 0) {
                CFG.blMode = (uint8_t)slot;
                settingsSave();
            } else if (row == 1) {
                if (slot == 2) timeResync();
                else { CFG.clock24 = (slot == 1); settingsSave(); s_hdr.hh = -2; }
            } else if (row == 3) {
                if (slot == 0) {
                    // Additive: does not disconnect, does not touch stored
                    // credentials. Reversible by tapping again, which is what
                    // lets it be a single tap with no confirmation step.
                    if (apActive()) apStop(); else apStart(true);
                    setupInvalidate();
                    vd::invalidate();
                } else if (slot == 1) {
                    wifiReconnect();
                } else {
                    // Forget. Genuinely one-way in the sense that whatever was
                    // stored is gone, but not destructive: the fallback AP it
                    // hands the device to is the identical route back in that
                    // exists on a device that has never been configured, so
                    // there is nothing here a confirmation step would protect.
                    wifiForget();
                    setupInvalidate();
                    vd::invalidate();
                }
            }
            s_set.valid = false;
            return true;
        }
        return false;
    }

    // Display page. The raster is a DEAD TARGET, deliberately: it shows a
    // caller's content, there is nothing there to act on, and accepting a tap
    // would make a phantom touch look like it did something.
    if (y < STRIP_Y) return false;
    // Same gate drawDisplayPage() uses for tabsMode -- the tabs are tappable
    // exactly when they are drawn, and only then.
    if (appActive() != APP_STATUS || canvasOwns() || apActive()) return false;

    // The full strip height counts, so the padding above and below each tab
    // folds into the control.
    for (uint8_t i = 0; i < THEME_TAB_N; i++) {
        int tx = THEME_TAB_X0 + i * (THEME_TAB_W + THEME_TAB_GAP);
        if (x < tx || x >= tx + THEME_TAB_W) continue;
        flash(PK_THEME, i, 0, now);
        themeSelect(i);
        return true;
    }
    return false;
}

TFT_eSPI *screenTft() { return T; }
