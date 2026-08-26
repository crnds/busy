#pragma once

#include "Types.h"

namespace TouchInput {
void begin();
void poll();  // emits keys into AppManager
}
