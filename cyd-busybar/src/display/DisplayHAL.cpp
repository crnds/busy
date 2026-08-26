#include "display/DisplayHAL.h"
#include "Pins.h"
#include "canvas/Fonts.h"

DisplayHAL Display;

void DisplayHAL::begin() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);

  ledcSetup(LED_R_CH, 5000, 8);
  ledcSetup(LED_G_CH, 5000, 8);
  ledcSetup(LED_B_CH, 5000, 8);
  ledcAttachPin(PIN_LED_R, LED_R_CH);
  ledcAttachPin(PIN_LED_G, LED_G_CH);
  ledcAttachPin(PIN_LED_B, LED_B_CH);
  rgbOff();

  tft.init();
  tft.setRotation(1);  // landscape 320×240, USB on the right
  tft.fillScreen(COL_BG);
  clear(DisplayId::Front, COL_BG);
  clear(DisplayId::Back, COL_BG);
  dirty_ = true;
}

void DisplayHAL::clear(DisplayId d, uint16_t color) {
  uint16_t* p = (d == DisplayId::Front) ? front_ : back_;
  int n = (d == DisplayId::Front) ? FRONT_W * FRONT_H : BACK_W * BACK_H;
  for (int i = 0; i < n; i++) p[i] = color;
  dirty_ = true;
}

void DisplayHAL::pixel(DisplayId d, int x, int y, uint16_t color) {
  int w = width(d), h = height(d);
  if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
  uint16_t* p = (d == DisplayId::Front) ? front_ : back_;
  p[y * w + x] = color;
  dirty_ = true;
}

uint16_t DisplayHAL::getPixel(DisplayId d, int x, int y) const {
  int w = width(d), h = height(d);
  if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return 0;
  const uint16_t* p = (d == DisplayId::Front) ? front_ : back_;
  return p[y * w + x];
}

void DisplayHAL::fillRect(DisplayId d, int x, int y, int w, int h, uint16_t color) {
  int W = width(d), H = height(d);
  int x1 = clampi(x, 0, W), y1 = clampi(y, 0, H);
  int x2 = clampi(x + w, 0, W), y2 = clampi(y + h, 0, H);
  uint16_t* p = (d == DisplayId::Front) ? front_ : back_;
  for (int yy = y1; yy < y2; yy++) {
    uint16_t* row = p + yy * W + x1;
    for (int xx = x1; xx < x2; xx++) *row++ = color;
  }
  dirty_ = true;
}

void DisplayHAL::hLine(DisplayId d, int x, int y, int w, uint16_t color) {
  fillRect(d, x, y, w, 1, color);
}
void DisplayHAL::vLine(DisplayId d, int x, int y, int h, uint16_t color) {
  fillRect(d, x, y, 1, h, color);
}
void DisplayHAL::rect(DisplayId d, int x, int y, int w, int h, uint16_t color) {
  hLine(d, x, y, w, color);
  hLine(d, x, y + h - 1, w, color);
  vLine(d, x, y, h, color);
  vLine(d, x + w - 1, y, h, color);
}

void DisplayHAL::roundRect(DisplayId d, int x, int y, int w, int h, int r, uint16_t color) {
  if (r <= 0) { rect(d, x, y, w, h, color); return; }
  hLine(d, x + r, y, w - 2 * r, color);
  hLine(d, x + r, y + h - 1, w - 2 * r, color);
  vLine(d, x, y + r, h - 2 * r, color);
  vLine(d, x + w - 1, y + r, h - 2 * r, color);
  // cheap corner dots
  pixel(d, x + r - 1, y + 1, color);
  pixel(d, x + w - r, y + 1, color);
  pixel(d, x + r - 1, y + h - 2, color);
  pixel(d, x + w - r, y + h - 2, color);
}

void DisplayHAL::fillRoundRect(DisplayId d, int x, int y, int w, int h, int r, uint16_t color) {
  if (r <= 0) { fillRect(d, x, y, w, h, color); return; }
  fillRect(d, x + r, y, w - 2 * r, h, color);
  fillRect(d, x, y + r, r, h - 2 * r, color);
  fillRect(d, x + w - r, y + r, r, h - 2 * r, color);
}

static uint16_t lerp565(uint16_t a, uint16_t b, int t, int maxv) {
  if (maxv <= 0) return a;
  uint8_t ar, ag, ab, br, bg, bb;
  rgb565split(a, ar, ag, ab);
  rgb565split(b, br, bg, bb);
  uint8_t r = ar + (int)(br - ar) * t / maxv;
  uint8_t g = ag + (int)(bg - ag) * t / maxv;
  uint8_t bl = ab + (int)(bb - ab) * t / maxv;
  return rgb565(r, g, bl);
}

void DisplayHAL::fillGradient(DisplayId d, int x, int y, int w, int h, uint16_t c0, uint16_t c1,
                             bool vertical) {
  int W = width(d), H = height(d);
  int x1 = clampi(x, 0, W), y1 = clampi(y, 0, H);
  int x2 = clampi(x + w, 0, W), y2 = clampi(y + h, 0, H);
  uint16_t* p = (d == DisplayId::Front) ? front_ : back_;
  int span = vertical ? (h - 1) : (w - 1);
  if (span < 1) span = 1;
  for (int yy = y1; yy < y2; yy++) {
    for (int xx = x1; xx < x2; xx++) {
      int t = vertical ? (yy - y) : (xx - x);
      p[yy * W + xx] = lerp565(c0, c1, t, span);
    }
  }
  dirty_ = true;
}

void DisplayHAL::blit(DisplayId d, int x, int y, int w, int h, const uint16_t* src, uint8_t opacity) {
  int W = width(d), H = height(d);
  for (int yy = 0; yy < h; yy++) {
    int dy = y + yy;
    if ((unsigned)dy >= (unsigned)H) continue;
    for (int xx = 0; xx < w; xx++) {
      int dx = x + xx;
      if ((unsigned)dx >= (unsigned)W) continue;
      uint16_t c = src[yy * w + xx];
      if (opacity >= 100) {
        pixel(d, dx, dy, c);
      } else if (opacity > 0) {
        uint8_t r, g, b, r2, g2, b2;
        rgb565split(getPixel(d, dx, dy), r, g, b);
        rgb565split(c, r2, g2, b2);
        uint8_t nr = r + (int)(r2 - r) * opacity / 100;
        uint8_t ng = g + (int)(g2 - g) * opacity / 100;
        uint8_t nb = b + (int)(b2 - b) * opacity / 100;
        pixel(d, dx, dy, rgb565(nr, ng, nb));
      }
    }
  }
}

void DisplayHAL::setRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgbR_ = r; rgbG_ = g; rgbB_ = b;
  // active LOW
  ledcWrite(LED_R_CH, 255 - r);
  ledcWrite(LED_G_CH, 255 - g);
  ledcWrite(LED_B_CH, 255 - b);
}

void DisplayHAL::rgbOff() {
  blinkUntil_ = 0;
  setRgb(0, 0, 0);
}

void DisplayHAL::blinkRgb(uint16_t color565, uint32_t ms) {
  uint8_t r, g, b;
  rgb565split(color565, r, g, b);
  rgbR_ = r; rgbG_ = g; rgbB_ = b;
  blinkUntil_ = millis() + ms;
  blinkLast_ = 0;
  blinkOn_ = false;
}

void DisplayHAL::setWipe(int16_t x) { wipeX_ = x; dirty_ = true; }

void DisplayHAL::setStatus(const char* left, const char* mid, bool wifiOn, int rssi) {
  if (left) strlcpy(statusLeft_, left, sizeof(statusLeft_));
  if (mid) strlcpy(statusMid_, mid, sizeof(statusMid_));
  wifiOn_ = wifiOn;
  rssi_ = rssi;
  dirty_ = true;
}

void DisplayHAL::drawStatusBar() {
  tft.fillRect(0, 0, TFT_W, STATUS_H, COL_STATUS);
  int bars = 0;
  if (wifiOn_) {
    if (rssi_ > -55) bars = 3;
    else if (rssi_ > -70) bars = 2;
    else bars = 1;
  }
  Fonts::drawIconWifi(4, 4, bars, wifiOn_ ? COL_GREEN : COL_MUTED);
  tft.setTextColor(COL_TEXT, COL_STATUS);
  tft.setTextSize(1);
  tft.setCursor(20, 4);
  tft.print(statusMid_);
  int16_t tw = tft.textWidth(statusLeft_);
  tft.setCursor(TFT_W - 4 - tw, 4);
  tft.setTextColor(COL_MUTED, COL_STATUS);
  tft.print(statusLeft_);
}

void DisplayHAL::scaleBlit(const uint16_t* src, int sw, int sh, int dx, int dy, int scale) {
  // One destination row at a time. Margins outside [dx, dx+sw*scale) stay black.
  uint16_t line[TFT_W];
  for (int sy = 0; sy < sh; sy++) {
    for (int i = 0; i < TFT_W; i++) line[i] = 0;
    for (int sx = 0; sx < sw; sx++) {
      uint16_t c = src[sy * sw + sx];
      int px = dx + sx * scale;
      for (int k = 0; k < scale; k++) {
        int xx = px + k;
        if ((unsigned)xx < (unsigned)TFT_W) line[xx] = c;
      }
    }
    for (int k = 0; k < scale; k++) {
      tft.pushImage(0, dy + sy * scale + k, TFT_W, 1, line);
    }
  }
}

void DisplayHAL::composite() {
  drawStatusBar();
  scaleBlit(front_, FRONT_W, FRONT_H, FRONT_DX, FRONT_DY, FRONT_SCALE);
  scaleBlit(back_, BACK_W, BACK_H, BACK_DX, BACK_DY, BACK_SCALE);
  if (wipeX_ >= 0) {
    tft.fillRect(wipeX_, 0, 14, TFT_H, COL_ACCENT);
  }
  dirty_ = false;
  lastComposite_ = millis();
}

void DisplayHAL::tick() {
  uint32_t now = millis();
  if (blinkUntil_ && now < blinkUntil_) {
    if (now - blinkLast_ > 280) {
      blinkLast_ = now;
      blinkOn_ = !blinkOn_;
      if (blinkOn_) setRgb(rgbR_, rgbG_, rgbB_);
      else setRgb(0, 0, 0);
    }
  } else if (blinkUntil_ && now >= blinkUntil_) {
    rgbOff();
  }
  if (dirty_ || (now - lastComposite_ > 250)) {
    composite();
  }
}
