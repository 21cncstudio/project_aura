// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stddef.h>

#include "web/WebSettingsUtils.h"

namespace WebSettingsApiUtils {

struct ParseResult {
    bool success = false;
    uint16_t status_code = 400;
    String error_message;
    WebSettingsUtils::SettingsUpdate update{};
};

ParseResult parseUpdateRequestBody(const String &body,
                                   const WebSettingsUtils::SettingsSnapshot &current_settings,
                                   bool storage_available,
                                   size_t display_name_max_len);

void fillUpdateSuccessJson(ArduinoJson::JsonObject root,
                           const WebSettingsUtils::SettingsSnapshot &snapshot,
                           bool restart_requested);

} // namespace WebSettingsApiUtils
