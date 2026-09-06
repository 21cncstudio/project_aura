// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Bmp3xxProbe.h"

#include <driver/i2c.h>

#include "config/AppConfig.h"

namespace {

constexpr uint8_t kBmp3xxErrReservedMask = 0xF8;
constexpr uint8_t kBmp3xxPwrCtrlReservedMask = 0xCC;
constexpr uint8_t kBmp3xxOsrReservedMask = 0xC0;
constexpr uint8_t kBmp3xxOdrReservedMask = 0xE0;

bool read_register(uint8_t addr, uint8_t reg, uint8_t &value) {
    const esp_err_t err = i2c_master_write_read_device(
        Config::SENSOR_I2C_PORT,
        addr,
        &reg,
        1,
        &value,
        1,
        pdMS_TO_TICKS(Config::SENSOR_I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool has_no_reserved_bits(uint8_t addr, uint8_t reg, uint8_t reserved_mask) {
    uint8_t value = 0;
    return read_register(addr, reg, value) && (value & reserved_mask) == 0;
}

} // namespace

namespace Bmp3xxProbe {

bool detect(uint8_t addr, Variant &variant) {
    variant = Variant::Unknown;

    uint8_t chip_id = 0;
    if (!read_register(addr, Config::BMP3XX_REG_CHIP_ID, chip_id)) {
        return false;
    }

    switch (chip_id) {
        case Config::BMP3XX_CHIP_ID_BMP388:
            variant = Variant::BMP388;
            break;
        case Config::BMP3XX_CHIP_ID_BMP390:
            variant = Variant::BMP390;
            break;
        default:
            return false;
    }

    // Shared-address protection against DPS310 and other devices:
    // accept only if several BMP3xx control/status registers fit valid bitmasks.
    const bool accepted =
        has_no_reserved_bits(addr, Config::BMP3XX_REG_ERR, kBmp3xxErrReservedMask) &&
        has_no_reserved_bits(addr, Config::BMP3XX_REG_PWR_CTRL, kBmp3xxPwrCtrlReservedMask) &&
        has_no_reserved_bits(addr, Config::BMP3XX_REG_OSR, kBmp3xxOsrReservedMask) &&
        has_no_reserved_bits(addr, Config::BMP3XX_REG_ODR, kBmp3xxOdrReservedMask);
    if (!accepted) {
        variant = Variant::Unknown;
    }
    return accepted;
}

} // namespace Bmp3xxProbe
