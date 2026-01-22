#pragma once

#include <driver/gpio.h>
#include <esp_system.h>

namespace BootHelpers {
    bool isCrashReset(esp_reset_reason_t reason);
    bool recoverI2CBus(gpio_num_t sda, gpio_num_t scl);
    void logGt911Address();
}
