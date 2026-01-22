// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/Watchdog.h"

#include <esp_task_wdt.h>

#include "core/Logger.h"

namespace {

bool g_enabled = false;

} // namespace

bool Watchdog::setup(uint32_t timeout_ms) {
    g_enabled = false;
    esp_task_wdt_config_t wdt_config = {};
    wdt_config.timeout_ms = timeout_ms;
    wdt_config.idle_core_mask = 0;
    wdt_config.trigger_panic = true;
    esp_err_t wdt_err = ESP_OK;
    esp_err_t status = esp_task_wdt_status(nullptr);

    if (status == ESP_ERR_INVALID_STATE) {
        wdt_err = esp_task_wdt_init(&wdt_config);
        if (wdt_err != ESP_OK) {
            LOGW("Main", "Task WDT init failed: %d", wdt_err);
            return false;
        }
    } else {
        if (status == ESP_ERR_NOT_FOUND) {
            // Subscribe before reconfigure to avoid esp_task_wdt_reset warnings.
            esp_err_t add_err = esp_task_wdt_add(nullptr);
            if (add_err != ESP_OK && add_err != ESP_ERR_INVALID_STATE) {
                LOGW("Main", "Task WDT add failed: %d", add_err);
            }
        } else if (status != ESP_OK) {
            LOGW("Main", "Task WDT status failed: %d", status);
        }

        wdt_err = esp_task_wdt_reconfigure(&wdt_config);
        if (wdt_err != ESP_OK) {
            LOGW("Main", "Task WDT reconfigure failed: %d", wdt_err);
            return false;
        }
    }

    if (esp_task_wdt_status(nullptr) != ESP_OK) {
        esp_err_t add_err = esp_task_wdt_add(nullptr);
        if (add_err != ESP_OK && add_err != ESP_ERR_INVALID_STATE) {
            LOGW("Main", "Task WDT add failed: %d", add_err);
        }
    }
    if (esp_task_wdt_status(nullptr) == ESP_OK) {
        g_enabled = true;
        LOGI("Main", "Task WDT enabled (%u ms)", timeout_ms);
    } else {
        LOGW("Main", "Task WDT not active for current task");
    }
    return g_enabled;
}

void Watchdog::kick() {
    if (!g_enabled) {
        return;
    }
    esp_task_wdt_reset();
}
