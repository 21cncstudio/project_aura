// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "I2CHelper.h"
#include <Arduino.h>
#include "config/AppConfig.h"

namespace I2C {

uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

esp_err_t probe(uint8_t addr) {
    return aura_i2c_probe(Config::SENSOR_I2C_PORT, addr, Config::SENSOR_I2C_TIMEOUT_MS);
}

esp_err_t write_cmd(uint8_t addr, uint16_t cmd, const uint8_t *params, size_t len) {
    if (len > 0 && !params) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t cmd_bytes[2] = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF)
    };
    return aura_i2c_write_pair(Config::SENSOR_I2C_PORT, addr, cmd_bytes, sizeof(cmd_bytes),
                               params, len, Config::SENSOR_I2C_TIMEOUT_MS);
}

esp_err_t read_bytes(uint8_t addr, uint8_t *data, size_t len) {
    return aura_i2c_read(
        Config::SENSOR_I2C_PORT,
        addr,
        data,
        len,
        Config::SENSOR_I2C_TIMEOUT_MS
    );
}

} // namespace I2C
