#include "canvas/Canvas.h"
#include "canvas/Fonts.h"
#include "display/DisplayHAL.h"
#include "net/TimeSync.h"
#include <LittleFS.h>
#include <FS.h>
#include <stdlib.h>

using fs::File;

CanvasEngine Canvas;

bool CanvasEngine::parseFill(const char* s, FillMode& out) {
  if (!s || !strcmp(s, "none")) { out = FillMode::None; return true; }
  if (!strcmp(s, "solid")) { out = FillMode::Solid; return true; }
  if (!strcmp(s, "gradient_h")) { out = FillMode::GradH; return true; }
  if (!strcmp(s, "gradient_v")) { out = FillMode::GradV; return true; }
  return false;
}

void CanvasEngine::begin() {
  memset(elems_, 0, sizeof(elems_));
  count_ = 0;
  priority_ = 0;
  app_[0] = 0;
  active_ = false;
}

void CanvasEngine::compact() {
  int w = 0;
  for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
    if (!elems_[i].used) continue;
    if (w != i) elems_[w] = elems_[i];
    w++;
  }
  if (w < CANVAS_MAX_ELEMENTS) {
    for (int i = w; i < CANVAS_MAX_ELEMENTS; i++) elems_[i].used = false;
  }
  count_ = w;
  if (count_ == 0) {
    active_ = false;
    priority_ = 0;
    app_[0] = 0;
  }
}

void CanvasEngine::expire() {
  uint32_t nowMs = millis();
  time_t now = TimeSync::nowUtc();
  bool any = false;
  for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
    CanvasElem& e = elems_[i];
    if (!e.used) continue;
    if (e.timeoutSec > 0 && (nowMs - e.createdMs) >= e.timeoutSec * 1000UL) {
      if (e.xpm) { free(e.xpm); e.xpm = nullptr; }
      e.used = false;
      any = true;
    } else if (e.displayUntil > 0 && now >= (time_t)e.displayUntil) {
      if (e.xpm) { free(e.xpm); e.xpm = nullptr; }
      e.used = false;
      any = true;
    }
  }
  if (any) compact();
}

CanvasResult CanvasEngine::show(const char* app, int priority, CanvasElem* incoming, int n,
                               uint16_t ledColor, bool blinkLed) {
  if (!app || !*app || n <= 0) return CanvasResult::BadParameters;
  if (priority < 1 || priority > CANVAS_MAX_PRIORITY) return CanvasResult::BadParameters;
  if (active_ && priority < priority_) return CanvasResult::LowPriority;
  if (n > CANVAS_MAX_ELEMENTS) return CanvasResult::TooManyElements;

  // Same or higher priority replaces the canvas contents for this request.
  // Elements from other apps are dropped (BUSY Bar canvas is exclusive).
  for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
    if (elems_[i].used && elems_[i].xpm) free(elems_[i].xpm);
  }
  memset(elems_, 0, sizeof(elems_));
  for (int i = 0; i < n; i++) {
    elems_[i] = incoming[i];
    elems_[i].used = true;
    elems_[i].createdMs = millis();
    elems_[i].scrollOff = 0;
    elems_[i].scrollLastMs = millis();
    elems_[i].animFrame = 0;
    elems_[i].animLastMs = millis();
    elems_[i].animLoaded = false;
    strlcpy(elems_[i].app, app, sizeof(elems_[i].app));
  }
  count_ = n;
  priority_ = priority;
  strlcpy(app_, app, sizeof(app_));
  active_ = true;
  if (blinkLed) Display.blinkRgb(ledColor, 3500);
  Display.markDirty();
  return CanvasResult::Ok;
}

CanvasResult CanvasEngine::clearApp(const char* app, const char** ids, int nIds) {
  if (nIds > 0 && ids) {
    for (int k = 0; k < nIds; k++) {
      bool found = false;
      for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
        if (elems_[i].used && !strcmp(elems_[i].id, ids[k])) {
          if (app && *app && strcmp(elems_[i].app, app) != 0)
            return CanvasResult::WrongAppId;
          found = true;
        }
      }
      if (!found) return CanvasResult::NonexistentElementId;
    }
    for (int k = 0; k < nIds; k++) {
      for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
        if (elems_[i].used && !strcmp(elems_[i].id, ids[k])) {
          if (elems_[i].xpm) free(elems_[i].xpm);
          elems_[i].used = false;
        }
      }
    }
  } else if (app && *app) {
    for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
      if (elems_[i].used && !strcmp(elems_[i].app, app)) {
        if (elems_[i].xpm) free(elems_[i].xpm);
        elems_[i].used = false;
      }
    }
  } else {
    for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
      if (elems_[i].used && elems_[i].xpm) free(elems_[i].xpm);
      elems_[i].used = false;
    }
  }
  compact();
  Display.markDirty();
  return CanvasResult::Ok;
}

static int cmpZ(const void* a, const void* b) {
  const CanvasElem* ea = *(const CanvasElem* const*)a;
  const CanvasElem* eb = *(const CanvasElem* const*)b;
  if (ea->z < eb->z) return -1;
  if (ea->z > eb->z) return 1;
  return 0;
}

void CanvasEngine::tick() {
  if (!active_) return;
  expire();
  if (!active_) return;

  uint32_t now = millis();
  for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
    CanvasElem& e = elems_[i];
    if (!e.used) continue;
    if (e.type == ElemType::Text && e.boxW > 0 && e.scrollRate > 0) {
      int tw = Fonts::textWidth(e.text, e.font);
      if (tw > e.boxW) {
        uint32_t elapsed = now - e.createdMs;
        if (elapsed < e.scrollStartMs) {
          e.scrollOff = 0;
        } else {
          // px/min → px
          uint32_t moving = elapsed - e.scrollStartMs;
          int cycle = tw + 8;
          uint32_t period = (e.scrollRate > 0) ? (cycle * 60000UL / e.scrollRate) : 1000;
          period += e.scrollRepeatMs;
          if (period == 0) period = 1;
          uint32_t phase = moving % period;
          uint32_t moveMs = period - e.scrollRepeatMs;
          if (phase < moveMs) {
            e.scrollOff = (int16_t)(phase * cycle / moveMs);
          } else {
            e.scrollOff = 0;
          }
        }
        e.scrolling = true;
        Display.markDirty();
      }
    }
    if (e.type == ElemType::Anim) {
      if (!e.animLoaded) loadAnimHeader(e);
      uint16_t delay = e.animDelay ? e.animDelay : 100;
      if (now - e.animLastMs >= delay) {
        e.animLastMs = now;
        e.animFrame++;
        if (e.animFrames && e.animFrame >= e.animFrames) {
          e.animFrame = e.loop ? 0 : (e.animFrames - 1);
        }
        Display.markDirty();
      }
    }
    if (e.type == ElemType::Countdown) Display.markDirty();
  }
}

void CanvasEngine::draw() {
  if (!active_) return;
  Display.clear(DisplayId::Front, COL_BG);
  Display.clear(DisplayId::Back, COL_BG);

  CanvasElem* order[CANVAS_MAX_ELEMENTS];
  int n = 0;
  for (int i = 0; i < CANVAS_MAX_ELEMENTS; i++) {
    if (elems_[i].used) order[n++] = &elems_[i];
  }
  qsort(order, n, sizeof(order[0]), cmpZ);
  for (int i = 0; i < n; i++) drawElem(*order[i]);
}

static void formatCountdown(char* buf, size_t n, const CanvasElem& e) {
  time_t now = TimeSync::nowUtc();
  int64_t diff;
  if (e.cdDir == 0) diff = e.timestamp - (int64_t)now;  // time_left
  else diff = (int64_t)now - e.timestamp;
  if (diff < 0) diff = 0;
  int hours = (int)(diff / 3600);
  int mins = (int)((diff % 3600) / 60);
  int secs = (int)(diff % 60);
  bool showH = (e.cdHours == 1) || (hours != 0);
  if (showH) snprintf(buf, n, "%d:%02d:%02d", hours, mins, secs);
  else snprintf(buf, n, "%02d:%02d", mins, secs);
}

void CanvasEngine::drawElem(CanvasElem& e) {
  switch (e.type) {
    case ElemType::Text: {
      int tw = Fonts::textWidth(e.text, e.font);
      int th = Fonts::textHeight(e.font);
      int box = e.boxW ? e.boxW : tw;
      int x = e.x, y = e.y;
      applyAlign(x, y, box, th, e.align);
      if (e.boxW && tw > e.boxW) {
        Fonts::drawTextClip(e.display, x, y, e.boxW, th, x - e.scrollOff, e.text, e.color, e.font);
      } else {
        Fonts::drawText(e.display, x, y, e.text, e.color, e.font);
      }
      break;
    }
    case ElemType::Countdown: {
      char buf[24];
      formatCountdown(buf, sizeof(buf), e);
      int tw = Fonts::textWidth(buf, FontId::Bold);
      int th = Fonts::textHeight(FontId::Bold);
      int x = e.x, y = e.y;
      applyAlign(x, y, tw, th, e.align);
      Fonts::drawText(e.display, x, y, buf, e.color, FontId::Bold);
      break;
    }
    case ElemType::Rect: {
      int x = e.x, y = e.y;
      applyAlign(x, y, e.rw, e.rh, e.align);
      if (e.fill == FillMode::Solid) {
        Display.fillRoundRect(e.display, x, y, e.rw, e.rh, e.radius, e.fillC[0]);
      } else if (e.fill == FillMode::GradH) {
        Display.fillGradient(e.display, x, y, e.rw, e.rh, e.fillC[0], e.fillC[1], false);
      } else if (e.fill == FillMode::GradV) {
        Display.fillGradient(e.display, x, y, e.rw, e.rh, e.fillC[0], e.fillC[1], true);
      }
      if (e.borderW > 0) {
        Display.roundRect(e.display, x, y, e.rw, e.rh, e.radius, e.borderC);
      }
      break;
    }
    case ElemType::Image:
      drawImageFile(e);
      break;
    case ElemType::Anim:
      drawAnim(e);
      break;
    case ElemType::Xpm:
      drawXpm(e);
      break;
  }
}

static bool readAll(File& f, uint8_t* buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    int r = f.read(buf + got, n - got);
    if (r <= 0) return false;
    got += r;
  }
  return true;
}

static uint16_t u16le(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool loadBmp24(File& f, uint16_t*& out, int& w, int& h) {
  uint8_t hdr[54];
  if (!readAll(f, hdr, 54)) return false;
  if (hdr[0] != 'B' || hdr[1] != 'M') return false;
  uint32_t off = u32le(hdr + 10);
  int32_t width = (int32_t)u32le(hdr + 18);
  int32_t height = (int32_t)u32le(hdr + 22);
  uint16_t bpp = u16le(hdr + 28);
  uint32_t comp = u32le(hdr + 30);
  if (bpp != 24 || comp != 0 || width <= 0 || height == 0) return false;
  bool bottomUp = height > 0;
  h = bottomUp ? height : -height;
  w = width;
  if (w > 320 || h > 240) return false;
  size_t pix = (size_t)w * h;
  out = (uint16_t*)malloc(pix * 2);
  if (!out) return false;
  f.seek(off);
  int rowBytes = ((w * 3 + 3) & ~3);
  uint8_t* row = (uint8_t*)malloc(rowBytes);
  if (!row) { free(out); out = nullptr; return false; }
  for (int y = 0; y < h; y++) {
    if (!readAll(f, row, rowBytes)) { free(row); free(out); out = nullptr; return false; }
    int dy = bottomUp ? (h - 1 - y) : y;
    for (int x = 0; x < w; x++) {
      uint8_t b = row[x * 3 + 0], g = row[x * 3 + 1], r = row[x * 3 + 2];
      out[dy * w + x] = rgb565(r, g, b);
    }
  }
  free(row);
  return true;
}

static bool loadRgb565(File& f, uint16_t*& out, int& w, int& h) {
  uint8_t hdr[4];
  if (!readAll(f, hdr, 4)) return false;
  w = u16le(hdr);
  h = u16le(hdr + 2);
  if (w <= 0 || h <= 0 || w > 320 || h > 240) return false;
  size_t n = (size_t)w * h * 2;
  out = (uint16_t*)malloc(n);
  if (!out) return false;
  if (!readAll(f, (uint8_t*)out, n)) { free(out); out = nullptr; return false; }
  return true;
}

void CanvasEngine::drawImageFile(CanvasElem& e) {
  if (!e.path[0]) return;
  File f = LittleFS.open(e.path, "r");
  if (!f) return;
  uint16_t* img = nullptr;
  int w = 0, h = 0;
  bool ok = false;
  const char* ext = strrchr(e.path, '.');
  if (ext && (!strcasecmp(ext, ".bmp"))) ok = loadBmp24(f, img, w, h);
  else ok = loadRgb565(f, img, w, h);
  f.close();
  if (!ok || !img) return;
  int x = e.x, y = e.y;
  applyAlign(x, y, w, h, e.align);
  Display.blit(e.display, x, y, w, h, img, e.opacity ? e.opacity : 100);
  free(img);
}

bool CanvasEngine::loadAnimHeader(CanvasElem& e) {
  e.animLoaded = true;
  if (!e.path[0]) return false;
  File f = LittleFS.open(e.path, "r");
  if (!f) return false;
  uint8_t hdr[12];
  bool ok = readAll(f, hdr, 12);
  f.close();
  if (!ok) return false;
  if (memcmp(hdr, "A565", 4) != 0) return false;
  e.animW = u16le(hdr + 4);
  e.animH = u16le(hdr + 6);
  e.animFrames = u16le(hdr + 8);
  e.animDelay = u16le(hdr + 10);
  if (!e.animFrames) e.animFrames = 1;
  return true;
}

void CanvasEngine::drawAnim(CanvasElem& e) {
  if (!e.animLoaded) loadAnimHeader(e);
  if (!e.path[0] || !e.animW || !e.animH) return;
  File f = LittleFS.open(e.path, "r");
  if (!f) return;
  uint16_t frame = e.animFrame;
  if (e.animFrames && frame >= e.animFrames) frame = e.animFrames - 1;
  size_t frameBytes = (size_t)e.animW * e.animH * 2;
  size_t off = 12 + frame * frameBytes;
  if (!f.seek(off)) { f.close(); return; }
  uint16_t* img = (uint16_t*)malloc(frameBytes);
  if (!img) { f.close(); return; }
  bool ok = readAll(f, (uint8_t*)img, frameBytes);
  f.close();
  if (ok) {
    int x = e.x, y = e.y;
    applyAlign(x, y, e.animW, e.animH, e.align);
    Display.blit(e.display, x, y, e.animW, e.animH, img, e.opacity ? e.opacity : 100);
  }
  free(img);
}

static uint16_t namedColor(const char* s) {
  if (!strcasecmp(s, "none") || !strcasecmp(s, "black")) return 0;
  if (!strcasecmp(s, "white")) return 0xFFFF;
  if (!strcasecmp(s, "red")) return 0xF800;
  if (!strcasecmp(s, "green")) return 0x07E0;
  if (!strcasecmp(s, "blue")) return 0x001F;
  if (!strcasecmp(s, "yellow")) return 0xFFE0;
  if (!strcasecmp(s, "cyan")) return 0x07FF;
  if (!strcasecmp(s, "magenta")) return 0xF81F;
  if (!strcasecmp(s, "gray") || !strcasecmp(s, "grey")) return 0x7BEF;
  uint16_t c;
  if (s[0] == '#' && parseHexColor(s, c)) return c;
  // #RGB short
  if (s[0] == '#' && strlen(s) == 4) {
    auto hx = [](char ch) {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
      if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
      return 0;
    };
    int r = hx(s[1]) * 17, g = hx(s[2]) * 17, b = hx(s[3]) * 17;
    return rgb565(r, g, b);
  }
  return 0xFFFF;
}

void CanvasEngine::drawXpm(CanvasElem& e) {
  if (!e.xpm) return;
  size_t n = strlen(e.xpm) + 1;
  char* copy = (char*)malloc(n);
  if (!copy) return;
  memcpy(copy, e.xpm, n);
  char* src = copy;
  // skip signature
  char* line = src;
  auto nextLine = [&]() -> char* {
    char* s = line;
    if (!s || !*s) return nullptr;
    char* nl = strpbrk(s, "\r\n");
    if (nl) {
      *nl = 0;
      line = nl + 1;
      if (*line == '\n') line++;
    } else {
      line = s + strlen(s);
    }
    return s;
  };
  char* sig = nextLine();
  if (!sig) { free(copy); return; }
  char* hdr = nextLine();
  if (!hdr) { free(copy); return; }
  int w = 0, h = 0, ncolors = 0, cpp = 0;
  if (sscanf(hdr, "%d %d %d %d", &w, &h, &ncolors, &cpp) != 4) { free(copy); return; }
  if (w <= 0 || h <= 0 || ncolors <= 0 || ncolors > 32 || cpp <= 0 || cpp > 4) { free(copy); return; }
  if (w > Display.width(e.display) || h > Display.height(e.display)) { free(copy); return; }

  struct Map { char key[5]; uint16_t color; bool none; };
  Map map[32];
  memset(map, 0, sizeof(map));
  for (int i = 0; i < ncolors; i++) {
    char* cl = nextLine();
    if (!cl) { free(copy); return; }
    // "<chars> <visual> <value>"
    if ((int)strlen(cl) < cpp + 3) { free(copy); return; }
    memcpy(map[i].key, cl, cpp);
    map[i].key[cpp] = 0;
    char* rest = cl + cpp;
    while (*rest == ' ') rest++;
    // skip visual token
    while (*rest && *rest != ' ') rest++;
    while (*rest == ' ') rest++;
    map[i].none = (!strcasecmp(rest, "none"));
    map[i].color = namedColor(rest);
  }
  int x0 = e.x, y0 = e.y;
  applyAlign(x0, y0, w, h, e.align);
  for (int y = 0; y < h; y++) {
    char* row = nextLine();
    if (!row) break;
    int len = (int)strlen(row);
    for (int x = 0; x < w; x++) {
      if ((x + 1) * cpp > len) break;
      char key[5] = {0};
      memcpy(key, row + x * cpp, cpp);
      for (int c = 0; c < ncolors; c++) {
        if (!memcmp(map[c].key, key, cpp)) {
          if (!map[c].none) Display.pixel(e.display, x0 + x, y0 + y, map[c].color);
          break;
        }
      }
    }
  }
  free(copy);
}
