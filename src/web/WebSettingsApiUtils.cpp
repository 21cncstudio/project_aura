// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSettingsApiUtils.h"

namespace WebSettingsApiUtils {

namespace {

ParseResult fail_result(uint16_t status_code, const char *message) {
    ParseResult result;
    result.success = false;
    result.status_code = status_code;
    result.error_message = message;
    return result;
}

} // namespace

ParseResult parseUpdateRequestBody(const String &body,
                                   const WebSettingsUtils::SettingsSnapshot &current_settings,
                                   bool storage_available,
                                   size_t display_name_max_len) {
    if (body.length() == 0) {
        return fail_result(400, "Missing body");
    }

    ArduinoJson::JsonDocument doc;
    const ArduinoJson::DeserializationError err = ArduinoJson::deserializeJson(doc, body);
    if (err) {
        return fail_result(400, "Invalid JSON");
    }

    const WebSettingsUtils::ParseResult settings_result =
        WebSettingsUtils::parseSettingsUpdate(doc.as<ArduinoJson::JsonVariantConst>(),
                                             current_settings,
                                             storage_available,
                                             display_name_max_len);

    ParseResult result;
    result.success = settings_result.success;
    result.status_code = settings_result.status_code;
    result.error_message = settings_result.error_message;
    result.update = settings_result.update;
    return result;
}

void fillUpdateSuccessJson(ArduinoJson::JsonObject root,
                           const WebSettingsUtils::SettingsSnapshot &snapshot,
                           bool restart_requested) {
    root["success"] = true;
    root["restart"] = restart_requested;
    ArduinoJson::JsonObject settings = root["settings"].to<ArduinoJson::JsonObject>();
    WebSettingsUtils::fillSettingsJson(settings, &snapshot, nullptr);
}

} // namespace WebSettingsApiUtils
