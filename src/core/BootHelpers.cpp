// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/BootHelpers.h"

#include <Arduino.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/Gt911ProbePolicy.h"
#include "core/Logger.h"
#include "esp_panel_board_custom_conf.h"

namespace {

using namespace Config;

constexpr auto GT911_PROBE_PLAN = Gt911ProbePolicy::configuredAddressOnly(
    static_cast<uint8_t>(ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS));
static_assert((SENSOR_I2C_SEPARATE && SENSOR_I2C_PORT != I2C_PORT) ||
                  GT911_PROBE_PLAN.address != SFA3X_ADDR,
              "Configured GT911 address must not collide with SFA3X on the same bus");

bool gt911_read_product_id(uint8_t addr, uint8_t *out, size_t len) {
    uint8_t reg[2] = {
        static_cast<uint8_t>(GT911_REG_PRODUCT_ID >> 8),
        static_cast<uint8_t>(GT911_REG_PRODUCT_ID & 0xFF)
    };
    esp_err_t err = i2c_master_write_read_device(
        I2C_PORT,
        addr,
        reg,
        sizeof(reg),
        out,
        len,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

} // namespace

bool BootHelpers::isCrashReset(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

I2cBusRecovery::Result BootHelpers::recoverI2CBus(gpio_num_t sda, gpio_num_t scl) {
    return I2cBusRecovery::recover(sda, scl);
}

const char *BootHelpers::resetReasonText(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
#ifdef ESP_RST_BROWNOUT
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
#endif
#ifdef ESP_RST_SDIO
        case ESP_RST_SDIO:      return "SDIO";
#endif
        default:                return "UNMAPPED";
    }
}

bool BootHelpers::readGt911ConfiguredProductId(uint8_t out[3]) {
    if (out == nullptr) {
        return false;
    }
    return gt911_read_product_id(GT911_PROBE_PLAN.address, out, 3U);
}

bool BootHelpers::isExpectedGt911ProductId(const uint8_t id[3]) {
    return id != nullptr && id[0] == static_cast<uint8_t>('9') &&
           id[1] == static_cast<uint8_t>('1') &&
           id[2] == static_cast<uint8_t>('1');
}

void BootHelpers::logGt911Address() {
    uint8_t id[3] = {};
    const bool ok = readGt911ConfiguredProductId(id);
    if (ok) {
        Logger::log(Logger::Info,
                    "GT911",
                    "probe 0x%02X: %02X,%02X,%02X",
                    GT911_PROBE_PLAN.address,
                    id[0],
                    id[1],
                    id[2]);
    } else {
        Logger::log(Logger::Info,
                    "GT911",
                    "probe 0x%02X: no response",
                    GT911_PROBE_PLAN.address);
    }
    boot_touch_detected = ok;
}
