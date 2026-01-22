// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <driver/i2c.h>
#include <stddef.h>

namespace I2C {
    uint8_t crc8(const uint8_t *data, size_t len);
    esp_err_t write_cmd(uint8_t addr, uint16_t cmd, const uint8_t *params, size_t len);
    esp_err_t read_bytes(uint8_t addr, uint8_t *data, size_t len);
}
