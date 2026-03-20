// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace WebOtaApiUtils {

struct Result {
    bool success = false;
    bool rebooting = false;
    int status_code = 500;
    size_t written_size = 0;
    size_t slot_size = 0;
    bool size_known = false;
    size_t expected_size = 0;
    String error;
    String message;
};

Result buildUpdateResult(bool has_upload,
                         bool success,
                         size_t written_size,
                         size_t slot_size,
                         bool size_known,
                         size_t expected_size,
                         const String &error);

void fillUpdateJson(ArduinoJson::JsonObject root, const Result &result);

} // namespace WebOtaApiUtils
