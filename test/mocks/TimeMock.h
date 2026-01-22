#pragma once

#include <cstdint>
#include <time.h>

void setNowEpoch(time_t epoch);
void advanceEpoch(uint32_t delta_s);
time_t mockNow();
