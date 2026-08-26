#pragma once

#include <Arduino.h>

// Virtual displays (BUSY Bar geometry). Front is 4× onto the TFT, back 2×.
static const int FRONT_W = 72;
static const int FRONT_H = 16;
static const int BACK_W  = 160;
static const int BACK_H  = 80;

static const int TFT_W = 320;
static const int TFT_H = 240;
static const int STATUS_H = 16;
static const int FRONT_SCALE = 4;
static const int BACK_SCALE  = 2;
static const int FRONT_DX = 16;   // (320 - 72*4) / 2
static const int FRONT_DY = 16;   // below status bar
static const int BACK_DX  = 0;
static const int BACK_DY  = 80;   // 16 + 64

static const int CANVAS_MAX_ELEMENTS = 100;
static const int CANVAS_MAX_PRIORITY = 100;
static const int SYSTEM_APP_PRIORITY = 10;
static const int DRAW_DEFAULT_PRIORITY = 50;

static const uint16_t COL_BG      = 0x0000;
static const uint16_t COL_STATUS  = 0x0841;
static const uint16_t COL_TEXT    = 0xFFFF;
static const uint16_t COL_MUTED   = 0x7BEF;
static const uint16_t COL_ACCENT  = 0xFBE0;  // #F4620E ≈ rgb565
static const uint16_t COL_GREEN   = 0x07E0;
static const uint16_t COL_RED     = 0xF800;
static const uint16_t COL_BLUE    = 0x3D7F;
static const uint16_t COL_YELLOW  = 0xFFE0;

enum class DisplayId : uint8_t { Front = 0, Back = 1 };

enum class Align : uint8_t {
  TopLeft = 0, TopMid, TopRight,
  MidLeft, Center, MidRight,
  BottomLeft, BottomMid, BottomRight
};

enum class FontId : uint8_t {
  Tiny, Small, Normal, Condensed, Bold, Large, ExtraLarge, Global
};

enum class AppId : uint8_t { Clock, Theme, Apps, Settings };

enum class Key : uint8_t {
  None, Up, Down, Ok, Back, Start, Busy, Custom, Off, Apps, Settings
};

enum class BrightnessMode : uint8_t { Auto, Manual };

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3));
}

inline void rgb565split(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (c >> 8) & 0xF8; r |= r >> 5;
  g = (c >> 3) & 0xFC; g |= g >> 6;
  b = (c << 3) & 0xF8; b |= b >> 5;
}

// #RRGGBBAA or #RRGGBB. Returns false on mismatch.
bool parseHexColor(const char* s, uint16_t& out565, uint8_t* alpha = nullptr);

inline Align parseAlign(const char* s) {
  if (!s) return Align::TopLeft;
  if (!strcmp(s, "top_mid")) return Align::TopMid;
  if (!strcmp(s, "top_right")) return Align::TopRight;
  if (!strcmp(s, "mid_left")) return Align::MidLeft;
  if (!strcmp(s, "center")) return Align::Center;
  if (!strcmp(s, "mid_right")) return Align::MidRight;
  if (!strcmp(s, "bottom_left")) return Align::BottomLeft;
  if (!strcmp(s, "bottom_mid")) return Align::BottomMid;
  if (!strcmp(s, "bottom_right")) return Align::BottomRight;
  return Align::TopLeft;
}

inline FontId parseFont(const char* s) {
  if (!s) return FontId::Normal;
  if (!strcmp(s, "tiny")) return FontId::Tiny;
  if (!strcmp(s, "small")) return FontId::Small;
  if (!strcmp(s, "condensed")) return FontId::Condensed;
  if (!strcmp(s, "bold")) return FontId::Bold;
  if (!strcmp(s, "large")) return FontId::Large;
  if (!strcmp(s, "extra_large")) return FontId::ExtraLarge;
  if (!strcmp(s, "global")) return FontId::Global;
  return FontId::Normal;
}

inline const char* fontName(FontId id) {
  switch (id) {
    case FontId::Tiny: return "tiny";
    case FontId::Small: return "small";
    case FontId::Condensed: return "condensed";
    case FontId::Bold: return "bold";
    case FontId::Large: return "large";
    case FontId::ExtraLarge: return "extra_large";
    case FontId::Global: return "global";
    default: return "normal";
  }
}

void applyAlign(int& x, int& y, int w, int h, Align a);

inline Key parseKey(const char* s) {
  if (!s) return Key::None;
  if (!strcmp(s, "up")) return Key::Up;
  if (!strcmp(s, "down")) return Key::Down;
  if (!strcmp(s, "ok")) return Key::Ok;
  if (!strcmp(s, "back")) return Key::Back;
  if (!strcmp(s, "start")) return Key::Start;
  if (!strcmp(s, "busy")) return Key::Busy;
  if (!strcmp(s, "custom")) return Key::Custom;
  if (!strcmp(s, "off")) return Key::Off;
  if (!strcmp(s, "apps")) return Key::Apps;
  if (!strcmp(s, "settings")) return Key::Settings;
  return Key::None;
}

inline int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
