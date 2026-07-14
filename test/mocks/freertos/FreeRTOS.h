#pragma once

#include <cstdint>

using TickType_t = uint32_t;

#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif

inline void vTaskDelay(TickType_t) {}
