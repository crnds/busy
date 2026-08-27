#include "Apps.h"
#include "../canvas/Canvas.h"
#include "../settings/Settings.h"
#include "../ui/theme.h"
#include "../../include/config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>

// The originals ship as proprietary zipped LVGL assets. Rather than port a
// decoder, a theme here is a label, three colours and a NAMED PROCEDURAL
// EFFECT -- so a theme is a few hundred bytes of JSON and recolours for free,
// exactly as the icons do.
static const char *THEME_DIRS[] = { "busy", "meeting", "dnd", "coding", "lunch" };
static const uint8_t THEME_N = sizeof(THEME_DIRS) / sizeof(THEME_DIRS[0]);

static StatusTheme s_cur;
// Set whenever the theme itself changes; cleared by the first render after.
static bool     s_themeDirty = true;
static uint32_t s_lastBucket = 0xFFFFFFFFu;

uint8_t themeCount() { return THEME_N; }
const char *themeNameAt(uint8_t i) { return THEME_DIRS[i % THEME_N]; }
const StatusTheme &themeCurrent() { return s_cur; }

static uint8_t parseEffect(const char *s) {
    if (!s) return 0;
    if (!strcmp(s, "blink")) return 1;
    if (!strcmp(s, "sweep")) return 2;
    if (!strcmp(s, "pulse")) return 3;
    if (!strcmp(s, "flame") || !strcmp(s, "fire") || !strcmp(s, "bars") || !strcmp(s, "anim")) return 4;
    return 0;
}

static uint16_t jsonColour(JsonVariantConst v, uint16_t dflt) {
    if (!v.is<const char *>()) return dflt;
    const char *s = v.as<const char *>();
    if (*s == '#') s++;
    char *end = nullptr;
    uint32_t rgb = strtoul(s, &end, 16);
    if (end == s) return dflt;
    return rgb888to565(rgb);
}

bool themeLoad(const char *name) {
    char path[64];
    snprintf(path, sizeof(path), "/themes/%s/theme.json", name);
    fs::File f = LittleFS.open(path, "r");
    if (!f) {
        // Fail soft: keep the label so the panel still says something true.
        strncpy(s_cur.name, name, sizeof(s_cur.name) - 1);
        strncpy(s_cur.label, name, sizeof(s_cur.label) - 1);
        s_cur.fg = 0xFFFF; s_cur.bg = 0xF800; s_cur.accent = 0xFFE0;
        s_cur.effect = 0; s_cur.periodMs = 1000; s_cur.loaded = false;
        s_themeDirty = true;
        return false;
    }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, f);
    f.close();
    if (e) { s_cur.loaded = false; s_themeDirty = true; return false; }

    memset(&s_cur, 0, sizeof(s_cur));
    strncpy(s_cur.name, name, sizeof(s_cur.name) - 1);
    strncpy(s_cur.label, doc["label"] | name, sizeof(s_cur.label) - 1);
    s_cur.fg       = jsonColour(doc["fg"], 0xFFFF);
    s_cur.bg       = jsonColour(doc["bg"], 0);
    s_cur.accent   = jsonColour(doc["accent"], 0xF800);
    s_cur.effect   = parseEffect(doc["effect"] | "none");
    s_cur.periodMs = (uint16_t)(doc["period_ms"] | 1200);
    if (!s_cur.periodMs) s_cur.periodMs = 1200;
    s_cur.loaded   = true;
    s_themeDirty   = true;
    return true;
}

void themeAppBegin() {
    if (!themeLoad(CFG.theme)) themeLoad(THEME_DIRS[0]);
}

void themeSelect(uint8_t i) {
    if (i >= THEME_N) return;
    themeLoad(THEME_DIRS[i]);
    strncpy(CFG.theme, THEME_DIRS[i], sizeof(CFG.theme) - 1);
    settingsSave();
    vd::invalidate();
}

void themeNext(int dir) {
    uint8_t i = 0;
    for (uint8_t k = 0; k < THEME_N; k++)
        if (!strcmp(THEME_DIRS[k], s_cur.name)) { i = k; break; }
    themeSelect((uint8_t)((i + THEME_N + dir) % THEME_N));
}

void themeRender(uint32_t now) {
    if (canvasOwns()) return;
    const StatusTheme &t = s_cur;

    // A still theme redraws only when it changes; an animated one redraws at
    // ANIM_TICK_MS at most. Without this gate the panel was cleared and
    // re-blitted on every pass of loop().
    uint32_t bucket = t.effect ? (now / ANIM_TICK_MS) : 0;
    if (!s_themeDirty && bucket == s_lastBucket) return;
    s_themeDirty = false;
    s_lastBucket = bucket;

    uint32_t period = t.periodMs ? t.periodMs : 1;
    uint32_t phase  = now % period;
    bool     onBeat = phase < (period / 2);

    // The panel is full colour now, so a theme finally shows its OWN colours --
    // on the old one-bit back panel every theme rendered identically in white,
    // and the effect had to carry the whole difference.
    uint16_t bg = t.bg, fg = t.fg, accent = t.accent;
    if (t.effect == 1 && !onBeat) { bg = t.accent; fg = t.bg; }       // blink

    vd::clear(bg);
    vd::drawRect(2, 2, VD_W - 4, VD_H - 4, accent);
    vd::text(VD_W / 2, 36, t.label, fg, VF_SMALL, 3, VA_MC);

    // The effect is still a SHAPE as well as a colour. Night mode collapses
    // the palette to one hue, so a theme that differed only in colour would
    // be indistinguishable from every other theme after dark.
    if (t.effect == 2) {                                              // sweep
        int x = (int)((int32_t)(VD_W + 24) * (int32_t)phase / (int32_t)period) - 24;
        vd::fillRect(x, VD_H - 12, 24, 6, accent);
    }
    if (t.effect == 3) {                                              // pulse
        int w = (int)((int32_t)(VD_W - 16) * (int32_t)phase / (int32_t)period);
        if (!onBeat) w = (VD_W - 16) - w;
        vd::fillRect(8, VD_H - 12, w, 6, accent);
    }
    if (t.effect == 4) {                                              // 5 fps looping realistic fire/flame icon (left & right)
        static const uint32_t FLAME_D[5][24] = {
            { 0x00600, 0x00900, 0x00900, 0x01080, 0x21080, 0x51080, 0x52046, 0x24049, 0x08029, 0x10029, 0x20019, 0x20004, 0x40008, 0x40008, 0x80008, 0x80008, 0x80008, 0x80008, 0x40008, 0x20010, 0x10020, 0x0C0C0, 0x03F00, 0x00000 },
            { 0x01800, 0x02400, 0x04200, 0x04200, 0x08200, 0x08218, 0x28124, 0x580A4, 0x54034, 0x24008, 0x08004, 0x10002, 0x20001, 0x20001, 0x40001, 0x40001, 0x80002, 0x80002, 0x40002, 0x3000C, 0x08010, 0x06060, 0x01F80, 0x00000 },
            { 0x00300, 0x00480, 0x00840, 0x00840, 0x18840, 0x24820, 0x25020, 0x15016, 0x0A00D, 0x04003, 0x08001, 0x10001, 0x20001, 0x20001, 0x40001, 0x40001, 0x80001, 0x80001, 0x40002, 0x20004, 0x18018, 0x06060, 0x01F80, 0x00000 },
            { 0x00C00, 0x01230, 0x02148, 0x02148, 0x04150, 0x04120, 0x08080, 0x28040, 0x58026, 0x58019, 0x28005, 0x10001, 0x20001, 0x40001, 0x40001, 0x80001, 0x80001, 0x80001, 0x40002, 0x20004, 0x18018, 0x06060, 0x01F80, 0x00000 },
            { 0x00660, 0x00990, 0x010D0, 0x01050, 0x02040, 0x32040, 0x4A020, 0x4C016, 0x2C00D, 0x14003, 0x08001, 0x10001, 0x20001, 0x40001, 0x40001, 0x80001, 0x80001, 0x80001, 0x40002, 0x20004, 0x18018, 0x06060, 0x01F80, 0x00000 }
        };
        static const uint32_t FLAME_O[5][24] = {
            { 0x00000, 0x00600, 0x00600, 0x00D00, 0x00D00, 0x20900, 0x21980, 0x03086, 0x06046, 0x0C0C6, 0x18066, 0x18078, 0x30030, 0x30030, 0x60030, 0x60030, 0x70070, 0x780F0, 0x3C1F0, 0x1FFE0, 0x0FFC0, 0x03F00, 0x00000, 0x00000 },
            { 0x00000, 0x01800, 0x03400, 0x03400, 0x06400, 0x04400, 0x04218, 0x26118, 0x22048, 0x02070, 0x06018, 0x0C00C, 0x18006, 0x18006, 0x30006, 0x38006, 0x7C01C, 0x7E03C, 0x3F07C, 0x0FFF0, 0x07FE0, 0x01F80, 0x00000, 0x00000 },
            { 0x00000, 0x00300, 0x00680, 0x00680, 0x00480, 0x18440, 0x18C40, 0x08820, 0x01812, 0x0300C, 0x06006, 0x0C006, 0x18006, 0x18006, 0x38006, 0x3C00E, 0x7E01E, 0x7F03E, 0x3FFFC, 0x1FFF8, 0x07FE0, 0x01F80, 0x00000, 0x00000 },
            { 0x00000, 0x00C00, 0x01A30, 0x01A30, 0x03220, 0x02200, 0x06100, 0x04080, 0x240C0, 0x24026, 0x0401A, 0x0C00E, 0x18006, 0x30002, 0x30002, 0x70006, 0x7C00E, 0x7F03E, 0x3FFFC, 0x1FFF8, 0x07FE0, 0x01F80, 0x00000, 0x00000 },
            { 0x00000, 0x00660, 0x00D20, 0x00C80, 0x01880, 0x01880, 0x31040, 0x33020, 0x12012, 0x0200C, 0x06006, 0x0C006, 0x18006, 0x30002, 0x38006, 0x78006, 0x7E01E, 0x7F03E, 0x3FFFC, 0x1FFF8, 0x07FE0, 0x01F80, 0x00000, 0x00000 }
        };
        static const uint32_t FLAME_Y[5][24] = {
            { 0x00000, 0x00000, 0x00000, 0x00200, 0x00200, 0x00600, 0x00600, 0x00F00, 0x01F80, 0x03300, 0x06180, 0x06180, 0x0E1C0, 0x0F3C0, 0x1FFC0, 0x1FFC0, 0x0FF80, 0x07F00, 0x03E00, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00800, 0x00800, 0x01800, 0x03800, 0x03C00, 0x01E00, 0x01F80, 0x01980, 0x01860, 0x03870, 0x07878, 0x07CF8, 0x0FFF8, 0x07FF8, 0x03FE0, 0x01FC0, 0x00F80, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00100, 0x00100, 0x00300, 0x00380, 0x00380, 0x007C0, 0x00660, 0x00C30, 0x01818, 0x03818, 0x07C38, 0x07FF8, 0x07FF8, 0x03FF0, 0x01FE0, 0x00FC0, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00400, 0x00400, 0x00C00, 0x01C00, 0x01E00, 0x03F00, 0x03300, 0x030C0, 0x030E0, 0x03030, 0x07878, 0x0FCFC, 0x0FFFC, 0x0FFF8, 0x03FF0, 0x00FC0, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00200, 0x00300, 0x00700, 0x00700, 0x00F80, 0x00CC0, 0x01860, 0x01870, 0x01818, 0x03C38, 0x07E78, 0x0FFFC, 0x07FF8, 0x07FF8, 0x01FE0, 0x00FC0, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 }
        };
        static const uint32_t FLAME_W[5][24] = {
            { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00C00, 0x01E00, 0x01E00, 0x01E00, 0x00C00, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00600, 0x00780, 0x00780, 0x00780, 0x00300, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00180, 0x003C0, 0x007E0, 0x007E0, 0x003C0, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00C00, 0x00F00, 0x00F00, 0x00FC0, 0x00780, 0x00300, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 },
            { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00300, 0x00780, 0x00780, 0x007E0, 0x003C0, 0x00180, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000 }
        };

        uint8_t frame = (now / 200) % 5;
        uint16_t cD = rgb888to565(0x6A0000);  // Deep ember outline
        uint16_t cO = rgb888to565(0xFF7A00);  // Bright fiery orange
        uint16_t cY = rgb888to565(0xFFE000);  // Glowing yellow
        uint16_t cW = 0xFFFF;                 // White hot core

        for (int r = 0; r < 24; r++) {
            uint32_t dBits = FLAME_D[frame][r];
            uint32_t oBits = FLAME_O[frame][r];
            uint32_t yBits = FLAME_Y[frame][r];
            uint32_t wBits = FLAME_W[frame][r];
            int y = 24 + r;
            for (int c = 0; c < 20; c++) {
                int shiftL = 19 - c;
                if ((wBits >> shiftL) & 1)      vd::pixel(18 + c, y, cW);
                else if ((yBits >> shiftL) & 1) vd::pixel(18 + c, y, cY);
                else if ((oBits >> shiftL) & 1) vd::pixel(18 + c, y, cO);
                else if ((dBits >> shiftL) & 1) vd::pixel(18 + c, y, cD);

                int shiftR = c;
                if ((wBits >> shiftR) & 1)      vd::pixel(122 + c, y, cW);
                else if ((yBits >> shiftR) & 1) vd::pixel(122 + c, y, cY);
                else if ((oBits >> shiftR) & 1) vd::pixel(122 + c, y, cO);
                else if ((dBits >> shiftR) & 1) vd::pixel(122 + c, y, cD);
            }
        }
    }
}
