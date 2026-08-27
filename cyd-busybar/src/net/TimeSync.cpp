#include <Arduino.h>
#include "Net.h"
#include "../settings/Settings.h"
#include "../../include/config.h"
#include <time.h>

static bool     s_synced = false;
static uint32_t s_next   = 0;

void timeBegin() {
    // POSIX TZ from settings, so DST is the C library's problem and not ours.
    configTzTime(CFG.tz, "pool.ntp.org", "time.nist.gov");
    s_synced = false;
    s_next   = 0;
}

void timeResync() { timeBegin(); }

void timeTick(uint32_t now) {
    if (s_synced || (int32_t)(now - s_next) < 0) return;
    s_next = now + 2000;
    struct tm t;
    if (getLocalTime(&t, 0) && t.tm_year > (2020 - 1900)) {
        s_synced = true;
        Serial.printf("[time] synced: %04d-%02d-%02d %02d:%02d:%02d %s\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec, CFG.tz);
    }
}

bool timeSynced() { return s_synced; }

bool timeNow(struct tm &out) {
    if (!s_synced) return false;
    return getLocalTime(&out, 0);
}
