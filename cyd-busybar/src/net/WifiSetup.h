#pragma once

#include "Types.h"
#include <IPAddress.h>

namespace WifiSetup {
void begin();
void poll();
bool connected();
bool inPortal();
const char* apName();
IPAddress ip();
void forget();
void startPortal();
}
