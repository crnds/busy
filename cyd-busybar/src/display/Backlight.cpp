#include "display/Backlight.h"
#include "Pins.h"
#include "settings/Settings.h"

BacklightCtrl Backlight;

void BacklightCtrl::begin() {
  pinMode(PIN_TFT_BL, OUTPUT);
  pinMode(PIN_LDR, INPUT);
  ledcSetup(BL_LEDC_CH, BL_LEDC_FREQ, BL_LEDC_BITS);
  ledcAttachPin(PIN_TFT_BL, BL_LEDC_CH);
  mode_ = Settings.d.brightnessMode;
  manual_ = Settings.d.brightness;
  apply();
}

uint8_t BacklightCtrl::percent() const {
  return (uint8_t)((int)duty_ * 100 / 255);
}

void BacklightCtrl::setMode(BrightnessMode mode) {
  mode_ = mode;
  Settings.d.brightnessMode = mode;
  apply();
}

void BacklightCtrl::setManual(uint8_t percent) {
  manual_ = percent > 100 ? 100 : percent;
  Settings.d.brightness = manual_;
  if (mode_ == BrightnessMode::Manual) apply();
}

void BacklightCtrl::apply() {
  if (mode_ == BrightnessMode::Manual) {
    duty_ = (uint8_t)((int)manual_ * 255 / 100);
    if (manual_ > 0 && duty_ < 12) duty_ = 12;
  } else {
    ldrRaw_ = analogRead(PIN_LDR);
    float t = (float)(ldrRaw_ - LDR_BRIGHT_RAW) / (float)(LDR_DARK_RAW - LDR_BRIGHT_RAW);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float bright = 1.0f - t;          // 1 = lots of light
    float gamma = powf(bright, 0.55f);
    duty_ = (uint8_t)(18 + gamma * 237);
  }
  ledcWrite(BL_LEDC_CH, duty_);
}

void BacklightCtrl::poll() {
  uint32_t now = millis();
  if (now - lastMs_ < 500) return;
  lastMs_ = now;
  if (mode_ == BrightnessMode::Auto) apply();
}
