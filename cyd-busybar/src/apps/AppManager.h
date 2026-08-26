#pragma once

#include "Types.h"

class AppManager {
 public:
  void begin();
  void tick();
  void draw();
  void handleKey(Key k);
  void switchTo(AppId id, bool wipe = true);
  AppId current() const { return current_; }
  int settingsIndex() const { return settingsIndex_; }
  void setSettingsIndex(int i) { settingsIndex_ = i; }
  bool inLeaf() const { return settingsLeaf_; }
  void setLeaf(bool v) { settingsLeaf_ = v; }

 private:
  AppId current_ = AppId::Clock;
  AppId pending_ = AppId::Clock;
  uint32_t wipeStart_ = 0;
  bool wiping_ = false;
  int settingsIndex_ = 0;
  bool settingsLeaf_ = false;
  uint32_t lastDrawMs_ = 0;

  void drawTabBar();
};

extern AppManager Apps;
