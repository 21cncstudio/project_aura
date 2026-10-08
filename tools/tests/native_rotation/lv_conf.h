// Host feasibility only: retain Aura's drawing settings, replace the ESP heap.
#include "../lvgl9_ui/lv_conf.h"
#undef LV_ASSERT_HANDLER_INCLUDE
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#undef LV_ASSERT_HANDLER
#define LV_ASSERT_HANDLER abort();
