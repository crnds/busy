#pragma once

#include "Types.h"

struct ThemeInfo {
  const char* id;
  const char* label;
  const char* backSub;
  uint16_t color;
  uint16_t bg;
  const char* anim;  // procedural name
};

namespace ThemeApp {
void begin();
void draw();
void handleKey(Key k);
void setById(const char* id);
const ThemeInfo* current();
const ThemeInfo* list(int& n);
int index();
}
