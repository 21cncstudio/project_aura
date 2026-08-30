// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebOtaApiUtils.h"

#include <string.h>
#include <string>

namespace WebOtaApiUtils {

namespace {

constexpr uint32_t kPrepareResponseWaitMarginMs = 30000;

bool error_contains(const String &text, const char *needle) {
    return needle && needle[0] != '\0' && strstr(text.c_str(), needle) != nullptr;
}

void classify_failure(Result &result, bool has_upload) {
    if (result.error.length() == 0) {
        if (!has_upload) {
            result.error = "Firmware file is missing";
        } else {
            result.error = "OTA update failed";
        }
    }

    // These codes are set by the device's BIN validator, not inferred from
    // English text ("mismatch" and "missing" also have older size/file meanings).
    if (result.error_code == "HARDWARE_TARGET_MISMATCH") {
        result.status_code = 409;
        return;
    }
    if (result.error_code == "HARDWARE_TARGET_MISSING" ||
        result.error_code == "HARDWARE_METADATA_UNSUPPORTED" ||
        result.error_code == "INVALID_FIRMWARE") {
        result.status_code = 400;
        return;
    }

    if (!has_upload || error_contains(result.error, "missing")) {
        result.status_code = 400;
        result.error_code = "MISSING_FILE";
        return;
    }

    if (error_contains(result.error, "too large")) {
        result.status_code = 413;
        result.error_code = "IMAGE_TOO_LARGE";
        return;
    }

    if (error_contains(result.error, "timed out")) {
        result.status_code = 408;
        result.error_code = "UPLOAD_TIMEOUT";
        return;
    }

    if (error_contains(result.error, "client disconnected")) {
        result.status_code = 499;
        result.error_code = "CLIENT_DISCONNECTED";
        return;
    }

    if (error_contains(result.error, "aborted") || error_contains(result.error, "interrupted")) {
        result.status_code = 400;
        result.error_code = "UPLOAD_ABORTED";
        return;
    }

    if (error_contains(result.error, "mismatch")) {
        result.status_code = 400;
        result.error_code = "SIZE_MISMATCH";
        return;
    }

    if (error_contains(result.error, "partition unavailable")) {
        result.status_code = 503;
        result.error_code = "OTA_UNAVAILABLE";
        return;
    }

    if (error_contains(result.error, "begin failed")) {
        result.status_code = 500;
        result.error_code = "BEGIN_FAILED";
        return;
    }

    if (error_contains(result.error, "write failed")) {
        result.status_code = 500;
        result.error_code = "WRITE_FAILED";
        return;
    }

    if (error_contains(result.error, "finalize failed")) {
        result.status_code = 500;
        result.error_code = "FINALIZE_FAILED";
        return;
    }

    result.status_code = 500;
    result.error_code = "OTA_FAILED";
}

} // namespace

uint32_t responseWaitTimeoutMs(uint32_t upload_timeout_ms) {
    const uint64_t wait_ms =
        static_cast<uint64_t>(upload_timeout_ms) + static_cast<uint64_t>(kPrepareResponseWaitMarginMs);
    return wait_ms >= static_cast<uint64_t>(UINT32_MAX)
               ? UINT32_MAX
               : static_cast<uint32_t>(wait_ms);
}

Result buildUpdateResult(bool has_upload,
                         bool success,
                         size_t written_size,
                         size_t slot_size,
                         bool size_known,
                         size_t expected_size,
                         const String &error,
                         const String &error_code) {
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
    result.error_code = error_code;
    classify_failure(result, has_upload);
    return result;
}

PrepareResult buildPrepareResult(bool available,
                                 bool size_supplied,
                                 bool size_valid,
                                 size_t slot_size,
                                 bool size_known,
                                 size_t expected_size,
                                 uint32_t upload_timeout_ms) {
    PrepareResult result{};
    result.slot_size = slot_size;
    result.size_known = size_known;
    result.expected_size = size_known ? expected_size : 0;
    result.upload_timeout_ms = upload_timeout_ms;
    result.response_wait_ms = responseWaitTimeoutMs(upload_timeout_ms);

    if (!available) {
        result.status_code = 503;
        result.error_code = "OTA_PREPARE_UNAVAILABLE";
        result.error = "OTA prepare unavailable";
        return result;
    }

    if (size_supplied && !size_valid) {
        result.status_code = 400;
        result.error_code = "INVALID_SIZE";
        result.error = "Invalid firmware size";
        return result;
    }

    if (size_known && expected_size > slot_size) {
        result.status_code = 413;
        result.error_code = "IMAGE_TOO_LARGE";
        result.error = "Firmware too large for OTA slot: ";
        result.error += std::to_string(expected_size).c_str();
        result.error += " > ";
        result.error += std::to_string(slot_size).c_str();
        return result;
    }

    result.success = true;
    result.status_code = 200;
    result.message = "Device ready for firmware upload";
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
        if (result.error_code.length() > 0) {
            root["error_code"] = result.error_code;
        }
        root["error"] = result.error;
        root["rebooting"] = false;
    }
}

void fillPrepareJson(ArduinoJson::JsonObject root, const PrepareResult &result) {
    root["success"] = result.success;
    root["slot_size"] = static_cast<uint32_t>(result.slot_size);
    if (result.size_known) {
        root["expected"] = static_cast<uint32_t>(result.expected_size);
    } else {
        root["expected"] = nullptr;
    }
    root["upload_timeout_ms"] = result.upload_timeout_ms;
    root["response_wait_ms"] = result.response_wait_ms;

    if (result.success) {
        root["message"] = result.message;
        return;
    }

    if (result.error_code.length() > 0) {
        root["error_code"] = result.error_code;
    }
    root["error"] = result.error;
}

} // namespace WebOtaApiUtils
