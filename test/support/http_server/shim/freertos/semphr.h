#pragma once
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t init);
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *b);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
void vSemaphoreDelete(SemaphoreHandle_t s);
#ifdef __cplusplus
}
#endif
