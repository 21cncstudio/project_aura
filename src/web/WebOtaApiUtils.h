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
    String error_code;
    String error;
    String message;
};

struct PrepareResult {
    bool success = false;
    int status_code = 500;
    size_t slot_size = 0;
    bool size_known = false;
    size_t expected_size = 0;
    uint32_t upload_timeout_ms = 0;
    uint32_t response_wait_ms = 0;
    String error_code;
    String error;
    String message;
};

Result buildUpdateResult(bool has_upload,
                         bool success,
                         size_t written_size,
                         size_t slot_size,
                         bool size_known,
                         size_t expected_size,
                         const String &error,
                         const String &error_code = "");

uint32_t responseWaitTimeoutMs(uint32_t upload_timeout_ms);

PrepareResult buildPrepareResult(bool available,
                                 bool size_supplied,
                                 bool size_valid,
                                 size_t slot_size,
                                 bool size_known,
                                 size_t expected_size,
                                 uint32_t upload_timeout_ms);

void fillUpdateJson(ArduinoJson::JsonObject root, const Result &result);
void fillPrepareJson(ArduinoJson::JsonObject root, const PrepareResult &result);

} // namespace WebOtaApiUtils
