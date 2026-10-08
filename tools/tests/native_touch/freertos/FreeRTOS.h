#pragma once
#include <stdint.h>
typedef struct { int owner; } portMUX_TYPE;
#define portMUX_FREE_VAL 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define pdMS_TO_TICKS(ms) (ms)
