// Canvas.h — the BUSY Bar draw model.
//
// A draw request is a namespaced, prioritised set of ELEMENTS on the virtual
// panel. Elements live until they expire, are replaced by the same id, or
// their application is cleared -- so a caller can post once and walk away,
// which is what /api/display/draw is for.
//
// Ownership: while any element is live the canvas owns the panel and the local
// apps (clock, themes) stand down. Between REMOTE callers, priority arbitrates
// and a loser gets 409, matching the original's semantics.
//
// Elements still ACCEPT a `display` field, and 0 and 1 are both valid, because
// BUSY Bar tooling sends it. With one screen it selects nothing -- both land on
// the same panel. Two elements aimed at "different displays" therefore overlap,
// which is the honest consequence of the hardware having one.
#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include "../vdisp/VDisplay.h"
#include "../../include/config.h"

enum ElemType : uint8_t {
    EL_TEXT = 0, EL_RECT, EL_IMAGE, EL_ANIM, EL_COUNTDOWN, EL_XPM
};

struct Element {
    bool     used;
    char     id[ELEM_ID_LEN];
    char     app[APP_NAME_LEN];
    uint8_t  type;
    int16_t  x, y, w, h;
    uint8_t  align;        // VAlign
    int8_t   z;
    uint16_t colour;
    uint32_t expiresAt;    // millis deadline; 0 = never

    char     text[ELEM_TEXT_LEN];
    uint8_t  font;         // VFont
    uint8_t  scale;

    bool     scroll;
    uint16_t scrollRateMs;
    uint32_t scrollStartMs;   // hold before the first step
    uint32_t scrollRepeatMs;  // hold between passes
    int32_t  scrollOff;
    uint32_t scrollNextMs;

    int64_t  targetEpoch;     // countdown
    bool     countUp;

    char     path[ELEM_PATH_LEN];
    uint16_t frames, frameMs, frame;
    uint32_t frameNextMs;

    uint8_t *inlineBits;      // xpm payload, heap-owned
    uint16_t inlineLen;
};

enum DrawResult : uint8_t {
    DR_OK = 0,
    DR_BAD_REQUEST,
    DR_CONFLICT,     // a higher-priority application owns a target panel
    DR_FULL          // CANVAS_MAX_ELEMENTS reached
};

void       canvasBegin();

// Parse and apply one draw request. `err` receives a human-readable reason.
DrawResult canvasDraw(JsonObjectConst req, char *err, size_t errLen);

// Selective clear. app == nullptr clears every namespace.
void       canvasClear(const char *app);

// Advance animations, scrolling and expiry, then repaint the panel if the
// canvas owns it. Returns true if it drew anything.
bool       canvasTick(uint32_t now);

bool       canvasOwns();
uint8_t    canvasPriority();
const char *canvasOwner();
uint16_t   canvasElementCount();

// The last led_notification_color a request carried, as RGB888, or -1.
int32_t    canvasLedColour();
void       canvasClearLed();
