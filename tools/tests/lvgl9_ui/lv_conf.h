// Use the firmware drawing/font configuration, replacing only the ESP heap.
#include "../../../include/lv_conf_idf9.h"
#undef LV_USE_STDLIB_MALLOC
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
