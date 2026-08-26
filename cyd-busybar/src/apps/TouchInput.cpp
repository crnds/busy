#include "apps/TouchInput.h"
#include "apps/AppManager.h"
#include "Pins.h"
#include "display/DisplayHAL.h"

static uint32_t lastTapMs = 0;
static bool down = false;
static int16_t downX = 0, downY = 0;
static int16_t lastX = 0, lastY = 0;

static void xptWrite(uint8_t cmd) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(PIN_TOUCH_MOSI, (cmd >> i) & 1);
    digitalWrite(PIN_TOUCH_SCLK, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_TOUCH_SCLK, HIGH);
    delayMicroseconds(5);
  }
  digitalWrite(PIN_TOUCH_MOSI, LOW);
  digitalWrite(PIN_TOUCH_SCLK, LOW);
}

static uint16_t xptRead(uint8_t cmd) {
  xptWrite(cmd);
  uint16_t result = 0;
  for (int i = 15; i >= 0; i--) {
    digitalWrite(PIN_TOUCH_SCLK, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_TOUCH_SCLK, LOW);
    delayMicroseconds(5);
    result |= (uint16_t)digitalRead(PIN_TOUCH_MISO) << i;
  }
  return result >> 4;
}

static uint16_t xptZ() {
  digitalWrite(PIN_TOUCH_CS, LOW);
  uint16_t z1 = xptRead(0xB1);
  uint16_t z = z1 + 4095;
  uint16_t z2 = xptRead(0xC1);
  z -= z2;
  xptRead(0xD0);
  digitalWrite(PIN_TOUCH_CS, HIGH);
  return z > 4095 ? 0 : z;
}

static bool readTouch(int16_t& x, int16_t& y) {
  if (xptZ() < TOUCH_Z_MIN) return false;
  digitalWrite(PIN_TOUCH_CS, LOW);
  uint16_t rx = xptRead(0xD1);
  uint16_t ry = xptRead(0x91);
  xptRead(0xD0);
  digitalWrite(PIN_TOUCH_CS, HIGH);
  x = map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, TFT_W - 1);
  y = map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, TFT_H - 1);
  x = constrain(x, 0, TFT_W - 1);
  y = constrain(y, 0, TFT_H - 1);
  return true;
}

void TouchInput::begin() {
  pinMode(PIN_TOUCH_CS, OUTPUT);
  pinMode(PIN_TOUCH_SCLK, OUTPUT);
  pinMode(PIN_TOUCH_MOSI, OUTPUT);
  pinMode(PIN_TOUCH_MISO, INPUT);
  pinMode(PIN_TOUCH_IRQ, INPUT);
  digitalWrite(PIN_TOUCH_CS, HIGH);
}

static Key hitToKey(int16_t x, int16_t y) {
  // Tab bar occupies TFT y 80+68*2 = 216 .. 240  (back y 68-80 * 2)
  if (y >= 216) {
    int slot = x / 80;  // 320/4
    if (slot <= 0) return Key::Start;
    if (slot == 1) return Key::Busy;
    if (slot == 2) return Key::Apps;
    return Key::Settings;
  }
  if (y < STATUS_H) return Key::None;
  // front region: tap = ok, swipe handled separately
  if (y < BACK_DY) return Key::Ok;
  // back content
  if (x < 40) return Key::Back;
  if (x > TFT_W - 40) return Key::Ok;
  return Key::None;
}

void TouchInput::poll() {
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if (now - lastPoll < 40) return;
  lastPoll = now;

  int16_t x, y;
  bool t = readTouch(x, y);
  if (t && !down) {
    down = true;
    downX = lastX = x;
    downY = lastY = y;
  } else if (t && down) {
    lastX = x;
    lastY = y;
  } else if (!t && down) {
    down = false;
    int dx = lastX - downX;
    int dy = lastY - downY;
    if (now - lastTapMs < 280) return;
    lastTapMs = now;
    if (abs(dx) > 40 && abs(dx) > abs(dy) * 2) {
      Apps.handleKey(dx > 0 ? Key::Up : Key::Down);
      return;
    }
    Key k = hitToKey(downX, downY);
    if (k != Key::None) Apps.handleKey(k);
  }
}
