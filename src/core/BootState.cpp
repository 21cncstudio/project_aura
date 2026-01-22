#include "core/BootState.h"
#include <esp_attr.h>

RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR uint32_t safe_boot_stage = 0;
esp_reset_reason_t boot_reset_reason = ESP_RST_UNKNOWN;
bool boot_i2c_recovered = false;
bool boot_touch_detected = false;
