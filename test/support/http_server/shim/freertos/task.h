#pragma once
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out, BaseType_t core, uint32_t caps);
void vTaskDeleteWithCaps(TaskHandle_t t);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out, BaseType_t core);
void vTaskDelete(TaskHandle_t t);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskDelay(TickType_t t);
#ifdef __cplusplus
}
#endif
