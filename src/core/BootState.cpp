// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/BootState.h"
#include <esp_attr.h>

RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR uint32_t safe_boot_stage = 0;
esp_reset_reason_t boot_reset_reason = ESP_RST_UNKNOWN;
bool boot_i2c_recovered = false;
bool boot_touch_detected = false;
