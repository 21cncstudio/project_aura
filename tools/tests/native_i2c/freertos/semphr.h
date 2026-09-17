#pragma once
#include "FreeRTOS.h"
struct StaticSemaphore_t { bool held=false; };
using SemaphoreHandle_t=StaticSemaphore_t*;
extern bool fail_lock;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {return storage;}
inline int xSemaphoreTake(SemaphoreHandle_t mutex,TickType_t) {if(fail_lock||mutex->held)return 0;mutex->held=true;return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t mutex) {mutex->held=false;}
