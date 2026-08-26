#include "Types.h"

bool parseHexColor(const char* s, uint16_t& out565, uint8_t* alpha) {
  if (!s || s[0] != '#') return false;
  size_t n = strlen(s + 1);
  if (n != 6 && n != 8) return false;
  auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  auto byteAt = [&](int i) -> int {
    int hi = hex(s[1 + i * 2]);
    int lo = hex(s[2 + i * 2]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
  };
  int r = byteAt(0), g = byteAt(1), b = byteAt(2);
  if (r < 0 || g < 0 || b < 0) return false;
  int a = 255;
  if (n == 8) {
    a = byteAt(3);
    if (a < 0) return false;
  }
  out565 = rgb565((uint8_t)r, (uint8_t)g, (uint8_t)b);
  if (alpha) *alpha = (uint8_t)a;
  return true;
}

void applyAlign(int& x, int& y, int w, int h, Align a) {
  switch (a) {
    case Align::TopMid:     x -= w / 2; break;
    case Align::TopRight:   x -= w; break;
    case Align::MidLeft:    y -= h / 2; break;
    case Align::Center:     x -= w / 2; y -= h / 2; break;
    case Align::MidRight:   x -= w; y -= h / 2; break;
    case Align::BottomLeft: y -= h; break;
    case Align::BottomMid:  x -= w / 2; y -= h; break;
    case Align::BottomRight:x -= w; y -= h; break;
    default: break;
  }
}
