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
#include "core/Logger.h"

namespace {

using namespace Config;

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

bool BootHelpers::recoverI2CBus(gpio_num_t sda, gpio_num_t scl) {
    gpio_set_direction(sda, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_direction(scl, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(sda, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(scl, GPIO_PULLUP_ONLY);
    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    delayMicroseconds(5);

    if (gpio_get_level(sda) == 1 && gpio_get_level(scl) == 1) {
        return true;
    }

    for (int i = 0; i < 9; ++i) {
        gpio_set_level(scl, 0);
        delayMicroseconds(5);
        gpio_set_level(scl, 1);
        delayMicroseconds(5);
        if (gpio_get_level(sda) == 1) {
            break;
        }
    }

    gpio_set_level(sda, 0);
    delayMicroseconds(5);
    gpio_set_level(scl, 1);
    delayMicroseconds(5);
    gpio_set_level(sda, 1);
    delayMicroseconds(5);

    bool ok = (gpio_get_level(sda) == 1 && gpio_get_level(scl) == 1);
    gpio_set_direction(sda, GPIO_MODE_INPUT);
    gpio_set_direction(scl, GPIO_MODE_INPUT);
    gpio_set_pull_mode(sda, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(scl, GPIO_PULLUP_ONLY);
    return ok;
}

void BootHelpers::logGt911Address() {
    uint8_t id[3] = {};
    bool ok_primary = gt911_read_product_id(GT911_ADDR_PRIMARY, id, sizeof(id));
    if (ok_primary) {
        Logger::log(Logger::Info, "GT911", "probe 0x5D: %02X,%02X,%02X", id[0], id[1], id[2]);
    } else {
        Logger::log(Logger::Info, "GT911", "probe 0x5D: no response");
    }

    bool ok_alt = gt911_read_product_id(GT911_ADDR_ALT, id, sizeof(id));
    if (ok_alt) {
        Logger::log(Logger::Info, "GT911", "probe 0x14: %02X,%02X,%02X", id[0], id[1], id[2]);
    } else {
        Logger::log(Logger::Info, "GT911", "probe 0x14: no response");
    }
    boot_touch_detected = ok_primary || ok_alt;
}
