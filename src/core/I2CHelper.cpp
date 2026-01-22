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

esp_err_t write_cmd(uint8_t addr, uint16_t cmd, const uint8_t *params, size_t len) {
    if (len > 0 && !params) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t cmd_bytes[2] = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF)
    };
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(handle, cmd_bytes, sizeof(cmd_bytes), true);
    if (params && len > 0) {
        i2c_master_write(handle, params, len, true);
    }
    i2c_master_stop(handle);
    esp_err_t err = i2c_master_cmd_begin(
        Config::I2C_PORT,
        handle,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    i2c_cmd_link_delete(handle);
    return err;
}

esp_err_t read_bytes(uint8_t addr, uint8_t *data, size_t len) {
    return i2c_master_read_from_device(
        Config::I2C_PORT,
        addr,
        data,
        len,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
}

} // namespace I2C
