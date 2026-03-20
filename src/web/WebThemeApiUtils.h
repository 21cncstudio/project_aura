// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config/AppData.h"

namespace WebThemeApiUtils {

void fillStateJson(ArduinoJson::JsonObject root, const ThemeColors &colors);
void fillErrorJson(ArduinoJson::JsonObject root, const char *message);

struct ApiAccessResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
};

ApiAccessResult validateApiAccess(bool wifi_ready,
                                  bool theme_screen_open,
                                  bool theme_custom_screen_open);

struct ParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    ThemeColors colors{};
};

ParseResult parseApplyRequestBody(const String &body, const ThemeColors &current_colors);
ParseResult parseApplyUpdate(ArduinoJson::JsonVariantConst root, const ThemeColors &current_colors);

} // namespace WebThemeApiUtils
