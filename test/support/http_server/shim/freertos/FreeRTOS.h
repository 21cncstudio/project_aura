#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef struct { void *h; } StaticSemaphore_t;
typedef void (*TaskFunction_t)(void *);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFUL
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) (x)
#define tskIDLE_PRIORITY 0
#define tskNO_AFFINITY 0x7FFFFFFF
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_8BIT 0
#ifdef __cplusplus
}
#endif
