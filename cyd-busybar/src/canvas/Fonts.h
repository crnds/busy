#pragma once

#include "Types.h"

namespace Fonts {
int glyphW(FontId id);
int glyphH(FontId id);
int textWidth(const char* s, FontId id);
int textHeight(FontId id);

void drawText(DisplayId d, int x, int y, const char* s, uint16_t color, FontId id);
void drawTextClip(DisplayId d, int clipX, int clipY, int clipW, int clipH,
                  int originX, const char* s, uint16_t color, FontId id);

void drawIconWifi(int tftX, int tftY, int bars, uint16_t color);
void drawIconSun(int tftX, int tftY, uint16_t color);
}  // namespace Fonts
