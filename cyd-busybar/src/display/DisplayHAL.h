#pragma once

#include "Types.h"
#include <TFT_eSPI.h>

class DisplayHAL {
 public:
  TFT_eSPI tft;

  void begin();
  void tick();

  void clear(DisplayId d, uint16_t color);
  void pixel(DisplayId d, int x, int y, uint16_t color);
  void fillRect(DisplayId d, int x, int y, int w, int h, uint16_t color);
  void hLine(DisplayId d, int x, int y, int w, uint16_t color);
  void vLine(DisplayId d, int x, int y, int h, uint16_t color);
  void rect(DisplayId d, int x, int y, int w, int h, uint16_t color);
  void roundRect(DisplayId d, int x, int y, int w, int h, int r, uint16_t color);
  void fillRoundRect(DisplayId d, int x, int y, int w, int h, int r, uint16_t color);
  void fillGradient(DisplayId d, int x, int y, int w, int h, uint16_t c0, uint16_t c1, bool vertical);
  void blit(DisplayId d, int x, int y, int w, int h, const uint16_t* src, uint8_t opacity = 100);

  uint16_t* frontBuf() { return front_; }
  uint16_t* backBuf() { return back_; }
  uint16_t getPixel(DisplayId d, int x, int y) const;

  void markDirty() { dirty_ = true; }
  bool dirty() const { return dirty_; }
  void composite();

  void setRgb(uint8_t r, uint8_t g, uint8_t b);
  void blinkRgb(uint16_t color565, uint32_t ms = 3000);
  void rgbOff();

  void setWipe(int16_t x);  // -1 off; else mask columns x..x+12 on TFT
  void setStatus(const char* left, const char* mid, bool wifiOn, int rssi);

  int width(DisplayId d) const { return d == DisplayId::Front ? FRONT_W : BACK_W; }
  int height(DisplayId d) const { return d == DisplayId::Front ? FRONT_H : BACK_H; }

 private:
  uint16_t front_[FRONT_W * FRONT_H];
  uint16_t back_[BACK_W * BACK_H];
  bool dirty_ = true;
  int16_t wipeX_ = -1;
  uint8_t rgbR_ = 0, rgbG_ = 0, rgbB_ = 0;
  uint32_t blinkUntil_ = 0;
  uint32_t blinkLast_ = 0;
  bool blinkOn_ = false;
  char statusLeft_[24] = "";
  char statusMid_[16] = "";
  bool wifiOn_ = false;
  int rssi_ = 0;
  uint32_t lastComposite_ = 0;

  void drawStatusBar();
  void scaleBlit(const uint16_t* src, int sw, int sh, int dx, int dy, int scale);
};

extern DisplayHAL Display;
