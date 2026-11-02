#pragma once
#include "esp_err.h"
#include <stdint.h>
static inline esp_err_t esp_efuse_mac_get_default(uint8_t *m) { for (int i = 0; i < 6; ++i) m[i] = 0x10 + i; return ESP_OK; }
