// HttpApi.h — a subset of the BUSY Bar REST API on the original's paths, so
// tooling written against the real device partially works against this one.
#pragma once
#include <stdint.h>

void apiBegin();
void apiTick(uint32_t now);
