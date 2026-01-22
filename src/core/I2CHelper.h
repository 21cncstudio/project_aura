#pragma once
#include <driver/i2c.h>
#include <stddef.h>

namespace I2C {
    uint8_t crc8(const uint8_t *data, size_t len);
    esp_err_t write_cmd(uint8_t addr, uint16_t cmd, const uint8_t *params, size_t len);
    esp_err_t read_bytes(uint8_t addr, uint8_t *data, size_t len);
}
