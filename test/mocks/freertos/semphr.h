#pragma once

#include "freertos/FreeRTOS.h"

using SemaphoreHandle_t = void *;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    static int mutex;
    return &mutex;
}

inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
