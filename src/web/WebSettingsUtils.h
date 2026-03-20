// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stddef.h>

#include "config/AppConfig.h"

namespace WebSettingsUtils {

struct SettingsSnapshot {
    bool available = false;
    bool night_mode = false;
    bool night_mode_locked = false;
    bool backlight_on = false;
    bool units_c = true;
    float temp_offset = 0.0f;
    float hum_offset = 0.0f;
    String display_name;
};

struct SettingsUpdate {
    bool has_night_mode = false;
    bool night_mode = false;
    bool has_backlight = false;
    bool backlight_on = false;
    bool has_units_c = false;
    bool units_c = true;
    bool has_temp_offset = false;
    float temp_offset = 0.0f;
    bool has_hum_offset = false;
    float hum_offset = 0.0f;
    bool has_display_name = false;
    String display_name;
    bool restart_requested = false;
};

struct ParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    SettingsUpdate update{};
};

ParseResult parseSettingsUpdate(ArduinoJson::JsonVariantConst root,
                                const SettingsSnapshot &current_settings,
                                bool storage_available,
                                size_t display_name_max_len);

void fillSettingsJson(ArduinoJson::JsonObject settings,
                      const SettingsSnapshot *snapshot,
                      const Config::StoredConfig *cfg);

} // namespace WebSettingsUtils
