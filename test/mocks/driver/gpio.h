#pragma once

#include <cstdint>

typedef int gpio_num_t;
typedef int esp_err_t;

enum gpio_mode_t {
    GPIO_MODE_INPUT = 1,
    GPIO_MODE_INPUT_OUTPUT_OD = 2,
};

enum gpio_pull_mode_t {
    GPIO_FLOATING = 0,
};

#ifndef ESP_OK
#define ESP_OK 0
#endif
