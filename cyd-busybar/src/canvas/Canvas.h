#pragma once

#include "Types.h"

enum class ElemType : uint8_t {
  Text, Image, Anim, Countdown, Rect, Xpm
};

enum class FillMode : uint8_t { None, Solid, GradH, GradV };

struct CanvasElem {
  bool used;
  char id[24];
  char app[24];
  ElemType type;
  DisplayId display;
  Align align;
  int16_t x, y;
  int32_t z;
  uint32_t timeoutSec;
  int64_t displayUntil;     // 0 = none
  uint32_t createdMs;
  uint16_t color;
  uint8_t opacity;
  // text
  char text[128];
  FontId font;
  uint16_t boxW;            // 0 = no clip
  uint16_t scrollRate;      // px per minute
  uint16_t scrollStartMs;
  uint16_t scrollRepeatMs;
  int16_t scrollOff;
  uint32_t scrollLastMs;
  bool scrolling;
  // image / anim path
  char path[80];
  bool loop;
  uint16_t animW, animH, animFrames, animDelay;
  uint16_t animFrame;
  uint32_t animLastMs;
  bool animLoaded;
  // countdown
  int64_t timestamp;
  uint8_t cdDir;            // 0 time_left, 1 time_since
  uint8_t cdHours;          // 0 when_non_zero, 1 always
  // rect
  uint16_t rw, rh, radius, borderW;
  FillMode fill;
  uint16_t fillC[2];
  uint16_t borderC;
  // xpm
  char* xpm;
};

enum class CanvasResult : uint8_t {
  Ok = 0,
  BadParameters,
  LowPriority,
  EmptyScreen,
  TooManyElements,
  NonexistentElementId,
  WrongAppId
};

class CanvasEngine {
 public:
  void begin();
  void tick();
  void draw();  // into virtual FBs (caller already cleared or we clear)

  CanvasResult show(const char* app, int priority, CanvasElem* incoming, int n,
                    uint16_t ledColor, bool blinkLed);
  CanvasResult clearApp(const char* app, const char** ids, int nIds);

  bool ownsScreen() const { return active_ && count_ > 0; }
  int priority() const { return priority_; }
  const char* appId() const { return app_; }
  int count() const { return count_; }

  // Used by HTTP parser
  static bool parseFill(const char* s, FillMode& out);

 private:
  CanvasElem elems_[CANVAS_MAX_ELEMENTS];
  int count_ = 0;
  int priority_ = 0;
  char app_[24] = "";
  bool active_ = false;

  void compact();
  void expire();
  void drawElem(CanvasElem& e);
  void drawImageFile(CanvasElem& e);
  void drawAnim(CanvasElem& e);
  void drawXpm(CanvasElem& e);
  bool loadAnimHeader(CanvasElem& e);
};

extern CanvasEngine Canvas;
