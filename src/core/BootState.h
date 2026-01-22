#pragma once

#include <esp_system.h>
#include <stdint.h>

extern uint32_t boot_count;
extern uint32_t safe_boot_stage;
extern esp_reset_reason_t boot_reset_reason;
extern bool boot_i2c_recovered;
extern bool boot_touch_detected;
