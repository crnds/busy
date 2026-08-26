#include "net/TimeSync.h"
#include "settings/Settings.h"
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

static bool s_started = false;

void TimeSync::applyTz() {
  setenv("TZ", Settings.d.tzPosix, 1);
  tzset();
}

void TimeSync::begin() {
  applyTz();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  s_started = true;
}

bool TimeSync::ready() {
  time_t t = time(nullptr);
  return t > 1700000000;
}

time_t TimeSync::nowUtc() { return time(nullptr); }

void TimeSync::localTm(struct tm& out) {
  time_t t = time(nullptr);
  localtime_r(&t, &out);
}

void TimeSync::formatIso(char* buf, size_t n) {
  struct tm t;
  localTm(t);
  const TzEntry* e = Settings.findTz(Settings.d.tzName);
  const char* off = e ? e->offset : "+00:00";
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d%s",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec, off);
}

bool TimeSync::setFromIso(const char* iso) {
  if (!iso) return false;
  int Y, M, D, h, m, s;
  if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return false;
  if (Y < 2020 || Y > 2099) return false;
  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  bool utc = (strchr(iso, 'Z') != nullptr);
  time_t epoch;
  if (utc) {
    char saved[48];
    strlcpy(saved, Settings.d.tzPosix, sizeof(saved));
    setenv("TZ", "GMT0", 1);
    tzset();
    epoch = mktime(&t);
    setenv("TZ", saved, 1);
    tzset();
  } else {
    epoch = mktime(&t);
  }
  if (epoch <= 0) return false;
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  return true;
}

void TimeSync::formatClock(char* buf, size_t n, bool hour12, bool seconds, bool blinkColon) {
  struct tm t;
  localTm(t);
  int hour = t.tm_hour;
  const char* colon = blinkColon && ((millis() / 500) % 2) ? " " : ":";
  if (hour12) {
    int h = hour % 12;
    if (h == 0) h = 12;
    if (seconds) snprintf(buf, n, "%d%s%02d%s%02d", h, colon, t.tm_min, colon, t.tm_sec);
    else snprintf(buf, n, "%d%s%02d", h, colon, t.tm_min);
  } else {
    if (seconds) snprintf(buf, n, "%02d%s%02d%s%02d", hour, colon, t.tm_min, colon, t.tm_sec);
    else snprintf(buf, n, "%02d%s%02d", hour, colon, t.tm_min);
  }
}

void TimeSync::formatDate(char* buf, size_t n) {
  static const char* wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  struct tm t;
  localTm(t);
  snprintf(buf, n, "%s %d %s", wday[t.tm_wday], t.tm_mday, mon[t.tm_mon]);
}

int TimeSync::hour24() {
  struct tm t;
  localTm(t);
  return t.tm_hour;
}
