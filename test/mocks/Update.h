#pragma once

#include <stddef.h>
#include <stdint.h>

#define U_FLASH 0
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
public:
    bool begin(size_t size = UPDATE_SIZE_UNKNOWN,
               int command = U_FLASH,
               int led_pin = -1,
               uint8_t led_on = 0,
               const char *label = nullptr);
    size_t write(uint8_t *data, size_t length);
    bool end(bool even_if_remaining = false);
    void abort();
    bool isRunning();
    const char *errorString();
};

extern UpdateClass Update;
