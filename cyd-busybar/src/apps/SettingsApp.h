#pragma once

#include "Types.h"

namespace SettingsApp {
void draw();
void handleKey(Key k);
int rowCount();
const char* rowLabel(int i);
}
