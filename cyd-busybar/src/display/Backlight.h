#pragma once

#include "Types.h"

class BacklightCtrl {
 public:
  void begin();
  void poll();
  void setMode(BrightnessMode mode);
  void setManual(uint8_t percent);  // 0-100
  void apply();
  uint8_t duty() const { return duty_; }
  uint8_t percent() const;
  int ldrRaw() const { return ldrRaw_; }
  BrightnessMode mode() const { return mode_; }

 private:
  BrightnessMode mode_ = BrightnessMode::Auto;
  uint8_t manual_ = 80;
  uint8_t duty_ = 200;
  int ldrRaw_ = 0;
  uint32_t lastMs_ = 0;
};

extern BacklightCtrl Backlight;
