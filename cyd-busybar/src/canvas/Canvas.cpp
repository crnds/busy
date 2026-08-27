#include "Canvas.h"
#include "../ui/theme.h"
#include <LittleFS.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ── STATE ────────────────────────────────────────────────────────────────

static Element  s_el[CANVAS_MAX_ELEMENTS];
static uint16_t s_count = 0;

struct Owner { char app[APP_NAME_LEN]; uint8_t prio; };
static Owner    s_owner;
static int32_t  s_led = -1;

// ── HELPERS ──────────────────────────────────────────────────────────────

static void copyStr(char *dst, size_t n, const char *src) {
    if (!n) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

static uint32_t parseHex(const char *s, bool *ok) {
    if (ok) *ok = false;
    if (!s || !*s) return 0;
    if (*s == '#') s++;
    char *end = nullptr;
    uint32_t v = (uint32_t)strtoul(s, &end, 16);
    if (end == s) return 0;
    if (ok) *ok = true;
    return v;
}

static uint16_t parseColour(JsonVariantConst v, uint16_t dflt) {
    if (v.isNull()) return dflt;
    uint32_t rgb;
    if (v.is<const char *>()) {
        bool ok = false;
        rgb = parseHex(v.as<const char *>(), &ok);
        if (!ok) return dflt;
    } else if (v.is<uint32_t>()) {
        rgb = v.as<uint32_t>();
    } else {
        return dflt;
    }
    return rgb888to565(rgb);
}

static uint8_t parseFont(JsonVariantConst v) {
    const char *s = v.is<const char *>() ? v.as<const char *>() : nullptr;
    if (s && (!strcmp(s, "tiny") || !strcmp(s, "TINY"))) return VF_TINY;
    return VF_SMALL;
}

static uint8_t parseType(const char *s) {
    if (!s) return 0xFF;
    if (!strcmp(s, "text"))      return EL_TEXT;
    if (!strcmp(s, "rectangle")) return EL_RECT;
    if (!strcmp(s, "rect"))      return EL_RECT;
    if (!strcmp(s, "image"))     return EL_IMAGE;
    if (!strcmp(s, "animation")) return EL_ANIM;
    if (!strcmp(s, "countdown")) return EL_COUNTDOWN;
    if (!strcmp(s, "xpm"))       return EL_XPM;
    return 0xFF;
}

static void freeElement(Element &e) {
    if (e.inlineBits) { free(e.inlineBits); e.inlineBits = nullptr; }
    e.inlineLen = 0;
    e.used = false;
}

static Element *findById(const char *app, const char *id) {
    if (!id || !*id) return nullptr;
    for (auto &e : s_el)
        if (e.used && !strcmp(e.id, id) && !strcmp(e.app, app)) return &e;
    return nullptr;
}

static Element *allocElement() {
    for (auto &e : s_el) if (!e.used) return &e;
    return nullptr;
}

// ── PUBLIC ───────────────────────────────────────────────────────────────

void canvasBegin() {
    memset(s_el, 0, sizeof(s_el));
    memset(&s_owner, 0, sizeof(s_owner));
    s_count = 0;
    s_led = -1;
}

uint16_t canvasElementCount() { return s_count; }
bool     canvasOwns()         { return s_owner.app[0] != '\0'; }
uint8_t  canvasPriority()     { return s_owner.prio; }
const char *canvasOwner()     { return s_owner.app; }
int32_t  canvasLedColour() { return s_led; }
void     canvasClearLed()  { s_led = -1; }

void canvasClear(const char *app) {
    for (auto &e : s_el) {
        if (!e.used) continue;
        if (app && strcmp(e.app, app) != 0) continue;
        freeElement(e);
        s_count--;
    }
    bool still = false;
    for (auto &e : s_el) if (e.used) { still = true; break; }
    if (!still) { s_owner.app[0] = '\0'; s_owner.prio = 0; }
    vd::invalidate();
    if (!app) s_led = -1;
}

DrawResult canvasDraw(JsonObjectConst req, char *err, size_t errLen) {
    auto fail = [&](DrawResult r, const char *m) {
        if (err && errLen) copyStr(err, errLen, m);
        return r;
    };

    JsonArrayConst arr = req["elements"].as<JsonArrayConst>();
    if (arr.isNull()) return fail(DR_BAD_REQUEST, "elements array is required");

    char app[APP_NAME_LEN];
    copyStr(app, sizeof(app), req["application_name"] | "default");

    int prio = req["priority"] | 50;
    if (prio < CANVAS_PRIO_MIN || prio > CANVAS_PRIO_MAX)
        return fail(DR_BAD_REQUEST, "priority must be 1..100");

    // Validate the whole request first: a draw is all-or-nothing, or a bad
    // element halfway down leaves the panel in a state the caller never asked
    // for and cannot infer from the 400.
    for (JsonObjectConst je : arr) {
        // `display` is still accepted, and both values are still valid, so
        // BUSY Bar tooling keeps working. With one screen it selects nothing.
        int d = je["display"] | 0;
        if (d < 0 || d > 1) return fail(DR_BAD_REQUEST, "display must be 0 or 1");
        if (parseType(je["type"] | (const char *)nullptr) == 0xFF)
            return fail(DR_BAD_REQUEST, "unknown element type");
    }
    if (canvasOwns() && strcmp(s_owner.app, app) != 0 && prio < s_owner.prio)
        return fail(DR_CONFLICT, "a higher-priority application owns the display");

    if (arr.size() > CANVAS_MAX_ELEMENTS)
        return fail(DR_BAD_REQUEST, "too many elements");

    uint32_t now = millis();

    for (JsonObjectConst je : arr) {
        uint8_t type = parseType(je["type"] | (const char *)nullptr);   // pre-validated

        const char *id = je["id"] | "";
        Element *e = findById(app, id);
        bool reused = (e != nullptr);
        if (!e) {
            e = allocElement();
            if (!e) return fail(DR_FULL, "element limit reached");
        }
        if (reused && e->inlineBits) free(e->inlineBits);
        memset(e, 0, sizeof(*e));
        if (!reused) s_count++;

        e->used    = true;
        e->type    = type;
        copyStr(e->id,  ELEM_ID_LEN,  id);
        copyStr(e->app, APP_NAME_LEN, app);

        e->x     = je["x"] | 0;
        e->y     = je["y"] | 0;
        e->w     = je["w"] | (int)vd::width();
        e->h     = je["h"] | (int)vd::height();
        e->align = (uint8_t)(je["align"] | (int)VA_TL);
        if (e->align > VA_BR) e->align = VA_TL;
        e->z     = (int8_t)(je["z"] | 0);
        e->colour = parseColour(je["color"], 0xFFFF);

        uint32_t timeout = je["timeout_ms"] | 0u;
        e->expiresAt = timeout ? now + timeout : 0;
        if (je["display_until"].is<uint32_t>()) {
            // Absolute unix seconds, as the original accepts.
            time_t nowEpoch = time(nullptr);
            int64_t until = je["display_until"].as<uint32_t>();
            e->expiresAt = (until > nowEpoch) ? now + (uint32_t)((until - nowEpoch) * 1000) : now;
        }

        switch (type) {
            case EL_TEXT:
                copyStr(e->text, ELEM_TEXT_LEN, je["text"] | "");
                e->font  = parseFont(je["font"]);
                e->scale = (uint8_t)(je["scale"] | 1);
                if (!e->scale) e->scale = 1;
                e->scroll        = je["scroll"] | false;
                e->scrollRateMs  = (uint16_t)(je["scroll_rate_ms"] | 60);
                e->scrollStartMs = je["scroll_start_delay_ms"] | 1000u;
                e->scrollRepeatMs= je["scroll_repeat_delay_ms"] | 1000u;
                e->scrollNextMs  = now + e->scrollStartMs;
                break;

            case EL_COUNTDOWN:
                e->targetEpoch = (int64_t)(je["target"] | 0u);
                e->countUp     = je["count_up"] | false;
                e->font        = parseFont(je["font"]);
                e->scale       = (uint8_t)(je["scale"] | 1);
                if (!e->scale) e->scale = 1;
                break;

            case EL_IMAGE:
                copyStr(e->path, ELEM_PATH_LEN, je["path"] | "");
                break;

            case EL_ANIM:
                copyStr(e->path, ELEM_PATH_LEN, je["path"] | "");
                e->frameMs     = (uint16_t)(je["frame_ms"] | 80);
                e->frameNextMs = now + e->frameMs;
                break;

            case EL_XPM: {
                // Packed 1bpp rows, MSB first, hex-encoded.
                const char *hex = je["bits"] | "";
                size_t hl = strlen(hex);
                if (hl >= 2 && hl % 2 == 0 && hl / 2 <= 4096) {
                    e->inlineLen  = (uint16_t)(hl / 2);
                    e->inlineBits = (uint8_t *)malloc(e->inlineLen);
                    if (e->inlineBits) {
                        for (size_t i = 0; i < e->inlineLen; i++) {
                            char b[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
                            e->inlineBits[i] = (uint8_t)strtoul(b, nullptr, 16);
                        }
                    } else {
                        e->inlineLen = 0;
                    }
                }
                break;
            }
            default: break;
        }
    }

    copyStr(s_owner.app, APP_NAME_LEN, app);
    s_owner.prio = (uint8_t)prio;
    vd::invalidate();

    if (req["led_notification_color"].is<const char *>()) {
        bool ok = false;
        uint32_t rgb = parseHex(req["led_notification_color"], &ok);
        if (ok) s_led = (int32_t)rgb;
    }

    return DR_OK;
}

// ── RENDER ───────────────────────────────────────────────────────────────

static void drawCountdown(Element &e) {
    time_t nowEpoch = time(nullptr);
    int64_t diff = e.countUp ? (nowEpoch - e.targetEpoch) : (e.targetEpoch - nowEpoch);
    if (diff < 0) diff = 0;
    int h = (int)(diff / 3600), m = (int)((diff % 3600) / 60), s = (int)(diff % 60);
    char buf[16];
    if (h > 0) snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    else       snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    vd::text(e.x, e.y, buf, e.colour, (VFont)e.font, e.scale, (VAlign)e.align);
}

// Asset files carry a 6-byte header: 'B','I' | w | h  (little-endian uint16).
// Animations use 'B','A' | w | h | frames.
static void drawAsset(Element &e, bool anim) {
    if (!e.path[0]) return;
    fs::File f = LittleFS.open(e.path, "r");
    if (!f) return;
    uint8_t hdr[10];
    size_t need = anim ? 8u : 6u;
    if (f.read(hdr, need) != need) { f.close(); return; }
    if (hdr[0] != 'B' || hdr[1] != (anim ? 'A' : 'I')) { f.close(); return; }
    int w = hdr[2] | (hdr[3] << 8);
    int h = hdr[4] | (hdr[5] << 8);
    if (w <= 0 || h <= 0 || w > 320 || h > 240) { f.close(); return; }
    uint16_t frames = anim ? (uint16_t)(hdr[6] | (hdr[7] << 8)) : 1;
    if (!frames) frames = 1;
    if (anim) { e.frames = frames; if (e.frame >= frames) e.frame = 0; }

    size_t frameBytes = (size_t)w * h * 2;
    f.seek(need + (size_t)e.frame * frameBytes);

    // Stream row by row: a 320x240 frame would be 150 KB, far past what we can
    // hold, and the panels are at most 72x16 / 160x80 anyway.
    static uint16_t row[VD_W];
    int rw = w > (int)(sizeof(row) / 2) ? (int)(sizeof(row) / 2) : w;
    for (int r = 0; r < h; r++) {
        if (f.read((uint8_t *)row, (size_t)rw * 2) != (size_t)rw * 2) break;
        if (w > rw) f.seek(f.position() + (size_t)(w - rw) * 2);
        // The panel is full colour now, so an asset's pixels go straight in --
        // the old luminance threshold existed only for the 1-bit back panel.
        for (int c = 0; c < rw; c++) vd::pixel(e.x + c, e.y + r, row[c]);
    }
    f.close();
}

static void drawElement(Element &e, uint32_t now) {
    switch (e.type) {
        case EL_RECT:
            vd::fillRect(e.x, e.y, e.w, e.h, e.colour);
            break;

        case EL_TEXT: {
            if (!e.scroll) {
                vd::text(e.x, e.y, e.text, e.colour, (VFont)e.font, e.scale, (VAlign)e.align);
                break;
            }
            int tw = vd::textW((VFont)e.font, e.text, e.scale);
            if (tw <= e.w) {
                vd::text(e.x, e.y, e.text, e.colour, (VFont)e.font, e.scale, (VAlign)e.align);
                break;
            }
            // Width-limited scroll: the text box is a window, so draw twice
            // with a gap and let the offset wrap.
            int gap  = vd::glyphW((VFont)e.font) * 4 * e.scale;
            int span = tw + gap;
            int off  = (int)(e.scrollOff % span);
            vd::text(e.x - off,        e.y, e.text, e.colour, (VFont)e.font, e.scale, VA_TL);
            vd::text(e.x - off + span, e.y, e.text, e.colour, (VFont)e.font, e.scale, VA_TL);
            break;
        }

        case EL_COUNTDOWN: drawCountdown(e);        break;
        case EL_IMAGE:     drawAsset(e, false);     break;
        case EL_ANIM:      drawAsset(e, true);      break;

        case EL_XPM:
            if (e.inlineBits && e.w > 0 && e.h > 0) {
                size_t need = (size_t)((e.w + 7) / 8) * e.h;
                if (need <= e.inlineLen)
                    vd::blit1(e.x, e.y, e.w, e.h, e.inlineBits, e.colour);
            }
            break;
        default: break;
    }
    (void)now;
}

bool canvasTick(uint32_t now) {
    bool expired = false;

    for (auto &e : s_el) {
        if (!e.used) continue;
        if (e.expiresAt && (int32_t)(now - e.expiresAt) >= 0) {
            freeElement(e);
            s_count--;
            expired = true;
            continue;
        }
        if (e.type == EL_TEXT && e.scroll && (int32_t)(now - e.scrollNextMs) >= 0) {
            e.scrollOff += 1;
            e.scrollNextMs = now + (e.scrollRateMs ? e.scrollRateMs : 60);
            vd::invalidate();
        }
        if (e.type == EL_ANIM && (int32_t)(now - e.frameNextMs) >= 0) {
            if (e.frames) e.frame = (uint16_t)((e.frame + 1) % e.frames);
            e.frameNextMs = now + (e.frameMs ? e.frameMs : 80);
            vd::invalidate();
        }
        if (e.type == EL_COUNTDOWN) {
            // Once a second, not once a loop pass: the readout has no field
            // finer than seconds, so anything faster is pure SPI traffic.
            uint32_t sec = now / 1000;
            if (sec != e.frameNextMs) { e.frameNextMs = sec; vd::invalidate(); }
        }
    }

    if (expired) {
        bool still = false;
        for (auto &e : s_el) if (e.used) { still = true; break; }
        if (!still) { s_owner.app[0] = '\0'; s_owner.prio = 0; }
        vd::invalidate();
    }

    if (!canvasOwns() || !vd::dirty()) return false;
    vd::clear(0);

    // z-order: a stable insertion sort over indices, since the element array is
    // a free list and its order is arbitrary.
    int idx[CANVAS_MAX_ELEMENTS], n = 0;
    for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++)
        if (s_el[i].used) idx[n++] = i;
    for (int i = 1; i < n; i++) {
        int k = idx[i], j = i - 1;
        while (j >= 0 && s_el[idx[j]].z > s_el[k].z) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = k;
    }
    for (int i = 0; i < n; i++) drawElement(s_el[idx[i]], now);
    return true;
}
