// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebOtaApiUtils.h"

#include <string.h>

namespace WebOtaApiUtils {

Result buildUpdateResult(bool has_upload,
                         bool success,
                         size_t written_size,
                         size_t slot_size,
                         bool size_known,
                         size_t expected_size,
                         const String &error) {
    Result result{};
    result.success = success;
    result.rebooting = success;
    result.written_size = written_size;
    result.slot_size = slot_size;
    result.size_known = size_known;
    result.expected_size = expected_size;

    if (success) {
        result.status_code = 200;
        result.message = "Firmware uploaded. Device will reboot.";
        return result;
    }

    result.error = error;
    if (result.error.length() == 0) {
        if (!has_upload) {
            result.error = "Firmware file is missing";
        } else {
            result.error = "OTA update failed";
        }
    }

    const char *error_text = result.error.c_str();
    if (strstr(error_text, "too large") != nullptr) {
        result.status_code = 413;
    } else if (strstr(error_text, "missing") != nullptr ||
               strstr(error_text, "aborted") != nullptr ||
               strstr(error_text, "mismatch") != nullptr) {
        result.status_code = 400;
    } else {
        result.status_code = 500;
    }
    return result;
}

void fillUpdateJson(ArduinoJson::JsonObject root, const Result &result) {
    root["success"] = result.success;
    root["written"] = static_cast<uint32_t>(result.written_size);
    root["slot_size"] = static_cast<uint32_t>(result.slot_size);
    if (result.size_known) {
        root["expected"] = static_cast<uint32_t>(result.expected_size);
    } else {
        root["expected"] = nullptr;
    }

    if (result.success) {
        root["message"] = result.message;
        root["rebooting"] = true;
    } else {
        root["error"] = result.error;
        root["rebooting"] = false;
    }
}

} // namespace WebOtaApiUtils
