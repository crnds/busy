#pragma once

#include "Types.h"

namespace TimeSync {
void begin();
void applyTz();
bool ready();
time_t nowUtc();
void localTm(struct tm& out);
void formatIso(char* buf, size_t n);
bool setFromIso(const char* iso);
void formatClock(char* buf, size_t n, bool hour12, bool seconds, bool blinkColon);
void formatDate(char* buf, size_t n);
int hour24();
}  // namespace TimeSync
