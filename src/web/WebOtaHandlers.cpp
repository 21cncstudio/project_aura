// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebOtaHandlers.h"

#include <Update.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>

#include "core/Logger.h"
#include "core/OtaRollback.h"
#include "web/WebOtaApiUtils.h"
#include "web/WebResponseUtils.h"
#include "web/WebTextUtils.h"

namespace {

constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";
constexpr const char kOtaBootPendingVerifyError[] =
    "Firmware boot validation is still pending; wait until the device is stable before starting another OTA.";
constexpr const char kApiErrorOtaBootPendingVerifyJson[] =
    "{\"success\":false,"
    "\"error\":\"Firmware boot validation is still pending; wait until the device is stable before starting another OTA.\","
    "\"error_code\":\"OTA_BOOT_PENDING_VERIFY\"}";
constexpr const char kOtaPhysicalConfirmRequiredError[] =
    "Firmware update confirmation is required.";
constexpr const char kOtaPhysicalConfirmDeniedError[] =
    "Firmware update was denied on the device.";
constexpr const char kOtaPhysicalConfirmExpiredError[] =
    "Firmware update confirmation expired.";
constexpr const char kOtaPhysicalConfirmMismatchError[] =
    "Firmware update confirmation does not match this upload.";
constexpr size_t kOtaAbortDrainMaxBytes = 32UL * 1024UL;
constexpr uint32_t kOtaAbortDrainTimeoutMs = 1500;

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

void send_ota_boot_pending_verify_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(409, "application/json", kApiErrorOtaBootPendingVerifyJson);
}

bool parse_confirm_id_arg(WebRequest &server, uint32_t &confirm_id) {
    size_t parsed = 0;
    if (!server.hasArg("ota_confirm_id") ||
        !WebTextUtils::parsePositiveSize(server.arg("ota_confirm_id"), parsed) ||
        parsed > static_cast<size_t>(UINT32_MAX)) {
        return false;
    }
    confirm_id = static_cast<uint32_t>(parsed);
    return confirm_id != 0;
}

const char *prepare_confirm_error_code(OtaPhysicalConfirm::PrepareStatus status) {
    switch (status) {
        case OtaPhysicalConfirm::PrepareStatus::Required:
            return "OTA_PHYSICAL_CONFIRM_REQUIRED";
        case OtaPhysicalConfirm::PrepareStatus::Busy:
            return "OTA_PHYSICAL_CONFIRM_BUSY";
        case OtaPhysicalConfirm::PrepareStatus::Denied:
            return "OTA_PHYSICAL_CONFIRM_DENIED";
        case OtaPhysicalConfirm::PrepareStatus::Expired:
            return "OTA_PHYSICAL_CONFIRM_EXPIRED";
        case OtaPhysicalConfirm::PrepareStatus::Mismatch:
            return "OTA_PHYSICAL_CONFIRM_MISMATCH";
        case OtaPhysicalConfirm::PrepareStatus::Ready:
        default:
            return "OTA_PHYSICAL_CONFIRM_REQUIRED";
    }
}

const char *prepare_confirm_error_message(OtaPhysicalConfirm::PrepareStatus status) {
    switch (status) {
        case OtaPhysicalConfirm::PrepareStatus::Required:
            return "Confirm firmware update on the device screen.";
        case OtaPhysicalConfirm::PrepareStatus::Busy:
            return "Another firmware update confirmation is already pending on the device.";
        case OtaPhysicalConfirm::PrepareStatus::Denied:
            return "Firmware update was denied on the device.";
        case OtaPhysicalConfirm::PrepareStatus::Expired:
            return "Firmware update confirmation expired. Start the upload again.";
        case OtaPhysicalConfirm::PrepareStatus::Mismatch:
            return "Firmware update confirmation does not match this firmware file.";
        case OtaPhysicalConfirm::PrepareStatus::Ready:
        default:
            return "Confirm firmware update on the device screen.";
    }
}

int prepare_confirm_http_status(OtaPhysicalConfirm::PrepareStatus status) {
    switch (status) {
        case OtaPhysicalConfirm::PrepareStatus::Busy:
        case OtaPhysicalConfirm::PrepareStatus::Mismatch:
            return 409;
        case OtaPhysicalConfirm::PrepareStatus::Required:
        case OtaPhysicalConfirm::PrepareStatus::Denied:
        case OtaPhysicalConfirm::PrepareStatus::Expired:
        default:
            return 403;
    }
}

void send_invalid_confirm_id_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(400, "application/json",
                "{\"success\":false,\"error_code\":\"INVALID_CONFIRM_ID\","
                "\"error\":\"Invalid OTA confirmation id.\"}");
}

void send_ota_physical_confirm_prepare_json(
    WebRequest &server,
    const OtaPhysicalConfirm::PrepareDecision &decision) {
    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();
    root["success"] = false;
    root["error_code"] = prepare_confirm_error_code(decision.status);
    root["error"] = prepare_confirm_error_message(decision.status);
    root["confirm_id"] = decision.confirm_id;

    if (decision.status == OtaPhysicalConfirm::PrepareStatus::Required) {
        root["confirm_required"] = true;
        root["retry_after_ms"] = decision.retry_after_ms;
        root["confirm_timeout_ms"] = decision.confirm_timeout_ms;
    }

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(prepare_confirm_http_status(decision.status), "application/json", json);
}

void send_ota_busy_upload_response(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    const size_t pending_body_bytes = server.pendingRequestBodyBytes();
    if (pending_body_bytes > 0) {
        const size_t drained =
            server.drainPendingRequestBody(kOtaAbortDrainMaxBytes,
                                           kOtaAbortDrainTimeoutMs);
        LOGI("OTA", "drained %u/%u pending request bytes before busy response",
             static_cast<unsigned>(drained),
             static_cast<unsigned>(pending_body_bytes));
    }
    server.sendHeader("Connection", "close");
    server.send(503, "application/json", kApiErrorOtaBusyJson);
    server.stopClient();
}

void send_ota_boot_pending_verify_upload_response(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    const size_t pending_body_bytes = server.pendingRequestBodyBytes();
    if (pending_body_bytes > 0) {
        const size_t drained =
            server.drainPendingRequestBody(kOtaAbortDrainMaxBytes,
                                           kOtaAbortDrainTimeoutMs);
        LOGI("OTA", "drained %u/%u pending request bytes before boot-validation response",
             static_cast<unsigned>(drained),
             static_cast<unsigned>(pending_body_bytes));
    }
    server.sendHeader("Connection", "close");
    server.send(409, "application/json", kApiErrorOtaBootPendingVerifyJson);
    server.stopClient();
}

int upload_confirm_error_status(const String &error) {
    if (error == kOtaPhysicalConfirmMismatchError) {
        return 409;
    }
    return 403;
}

const char *upload_confirm_error_code(const String &error) {
    if (error == kOtaPhysicalConfirmDeniedError) {
        return "OTA_PHYSICAL_CONFIRM_DENIED";
    }
    if (error == kOtaPhysicalConfirmExpiredError) {
        return "OTA_PHYSICAL_CONFIRM_EXPIRED";
    }
    if (error == kOtaPhysicalConfirmMismatchError) {
        return "OTA_PHYSICAL_CONFIRM_MISMATCH";
    }
    return "OTA_PHYSICAL_CONFIRM_REQUIRED";
}

bool is_upload_confirm_error(const String &error) {
    return error == kOtaPhysicalConfirmRequiredError ||
           error == kOtaPhysicalConfirmDeniedError ||
           error == kOtaPhysicalConfirmExpiredError ||
           error == kOtaPhysicalConfirmMismatchError;
}

void send_ota_physical_confirm_upload_response(WebRequest &server, const String &error) {
    WebResponseUtils::sendNoStoreHeaders(server);
    const size_t pending_body_bytes = server.pendingRequestBodyBytes();
    if (pending_body_bytes > 0) {
        const size_t drained =
            server.drainPendingRequestBody(kOtaAbortDrainMaxBytes,
                                           kOtaAbortDrainTimeoutMs);
        LOGI("OTA", "drained %u/%u pending request bytes before physical-confirm response",
             static_cast<unsigned>(drained),
             static_cast<unsigned>(pending_body_bytes));
    }

    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();
    root["success"] = false;
    root["error_code"] = upload_confirm_error_code(error);
    root["error"] = error;
    String json;
    serializeJson(doc, json);

    server.sendHeader("Connection", "close");
    server.send(upload_confirm_error_status(error), "application/json", json);
    server.stopClient();
}

String ota_error_prefixed(const char *prefix) {
    String error = prefix;
    error += ": ";
    error += Update.errorString();
    return error;
}

String ota_abort_error_message(const WebOtaSnapshot &ota,
                               WebUploadAbortReason abort_reason,
                               uint32_t now_ms,
                               bool client_connected) {
    if (abort_reason == WebUploadAbortReason::TotalTimeout) {
        String error = "Upload timed out after total deadline of ";
        error += String(ota.totalDurationMs(now_ms));
        error += " ms";
        return error;
    }

    if (abort_reason == WebUploadAbortReason::IdleTimeout) {
        String error = "Upload timed out after ";
        error += String(ota.lastChunkAgeMs(now_ms));
        error += " ms without data";
        return error;
    }

    if (abort_reason == WebUploadAbortReason::ClientDisconnected) {
        return "Upload interrupted: client disconnected";
    }

    if (abort_reason == WebUploadAbortReason::SocketError) {
        return "Upload interrupted by socket error";
    }

    if (!ota.first_chunk_seen) {
        return client_connected ? "Upload timed out before first chunk"
                                : "Upload interrupted before first chunk";
    }

    const uint32_t idle_ms = ota.lastChunkAgeMs(now_ms);
    if (idle_ms > 0) {
        String error = client_connected ? "Upload timed out after " : "Upload interrupted after ";
        error += String(idle_ms);
        error += " ms without data";
        return error;
    }

    return client_connected ? "Upload aborted" : "Upload interrupted";
}

const char *ota_abort_reason_text(WebUploadAbortReason reason) {
    switch (reason) {
        case WebUploadAbortReason::IdleTimeout:
            return "idle_timeout";
        case WebUploadAbortReason::TotalTimeout:
            return "total_timeout";
        case WebUploadAbortReason::ClientDisconnected:
            return "client_disconnected";
        case WebUploadAbortReason::SocketError:
            return "socket_error";
        case WebUploadAbortReason::None:
            return "none";
        default:
            return "unknown";
    }
}

void abort_update_if_running() {
    if (Update.isRunning()) {
        Update.abort();
    }
}

void fail_upload(WebOtaHandlers::Runtime &runtime, const String &error) {
    abort_update_if_running();
    if (runtime.set_error) {
        runtime.set_error(error);
    }
}

const char *upload_confirm_error_message(OtaPhysicalConfirm::ConsumeStatus status) {
    switch (status) {
        case OtaPhysicalConfirm::ConsumeStatus::Denied:
            return kOtaPhysicalConfirmDeniedError;
        case OtaPhysicalConfirm::ConsumeStatus::Expired:
            return kOtaPhysicalConfirmExpiredError;
        case OtaPhysicalConfirm::ConsumeStatus::Mismatch:
            return kOtaPhysicalConfirmMismatchError;
        case OtaPhysicalConfirm::ConsumeStatus::Missing:
        case OtaPhysicalConfirm::ConsumeStatus::NotAllowed:
        case OtaPhysicalConfirm::ConsumeStatus::Consumed:
        default:
            return kOtaPhysicalConfirmRequiredError;
    }
}

void cleanup_after_update_response(WebOtaHandlers::Runtime &runtime, bool success) {
    if (!success && runtime.set_ui_screen) {
        runtime.set_ui_screen(WebUiBridge::FirmwareUpdateScreenMode::Hidden);
    }
    if (runtime.restore_wifi_power_save) {
        runtime.restore_wifi_power_save();
    }
    runtime.ota_state.clearBusy();
}

void ota_log_abort_summary(WebRequest &server,
                           const WebOtaSnapshot &ota,
                           WebUploadAbortReason abort_reason) {
    const uint32_t now_ms = millis();
    const wl_status_t wifi_status = WiFi.status();
    const bool current_rssi_valid = wifi_status == WL_CONNECTED;
    const int current_rssi = current_rssi_valid ? WiFi.RSSI() : 0;
    const bool client_connected = server.clientConnected();

    LOGW("OTA",
         "upload aborted (session=%u reason=%s written=%u slot=%u expected=%u known=%s total=%u ms first_chunk_seen=%s first_chunk=%u ms last_chunk_age=%u ms chunks=%u chunk[min/avg/max]=%u/%u/%u bytes client_connected=%s wifi_status=%d start_rssi_valid=%s start_rssi=%d current_rssi_valid=%s current_rssi=%d)",
         static_cast<unsigned>(ota.session_id),
         ota_abort_reason_text(abort_reason),
         static_cast<unsigned>(ota.written_size),
         static_cast<unsigned>(ota.slot_size),
         static_cast<unsigned>(ota.expected_size),
         ota.size_known ? "YES" : "NO",
         static_cast<unsigned>(ota.totalDurationMs(now_ms)),
         ota.first_chunk_seen ? "YES" : "NO",
         static_cast<unsigned>(ota.firstChunkDelayMs()),
         static_cast<unsigned>(ota.lastChunkAgeMs(now_ms)),
         static_cast<unsigned>(ota.chunk_count),
         static_cast<unsigned>(ota.chunk_min_size),
         static_cast<unsigned>(ota.avgChunkSize()),
         static_cast<unsigned>(ota.chunk_max_size),
         client_connected ? "YES" : "NO",
         static_cast<int>(wifi_status),
         ota.start_rssi_valid ? "YES" : "NO",
         ota.start_rssi,
         current_rssi_valid ? "YES" : "NO",
         current_rssi);
}

}  // namespace

namespace WebOtaHandlers {

void handlePrepare(Runtime &runtime, bool ota_busy) {
    if (!runtime.context.server) {
        return;
    }

    WebRequest &server = *runtime.context.server;
    if (ota_busy) {
        send_ota_busy_json(server);
        return;
    }
    if (OtaRollback::isPendingVerify()) {
        LOGW("OTA", "reject prepare while boot validation is pending");
        send_ota_boot_pending_verify_json(server);
        return;
    }

    size_t expected_size = 0;
    const bool size_supplied = server.hasArg("ota_size");
    const bool size_valid = size_supplied ? WebTextUtils::parsePositiveSize(server.arg("ota_size"), expected_size)
                                          : false;
    const bool size_known = size_supplied && size_valid;

    const esp_partition_t *target_partition = esp_ota_get_next_update_partition(nullptr);
    const size_t slot_size = target_partition ? target_partition->size : 0;
    const bool available = runtime.arm_preflight_ui && runtime.upload_timeout_ms && target_partition;
    const uint32_t upload_timeout_ms =
        available && runtime.upload_timeout_ms
            ? runtime.upload_timeout_ms(size_known ? expected_size : 0)
            : 0;
    const WebOtaApiUtils::PrepareResult result =
        WebOtaApiUtils::buildPrepareResult(available,
                                           size_supplied,
                                           size_valid,
                                           slot_size,
                                           size_known,
                                           expected_size,
                                           upload_timeout_ms);

    uint32_t confirm_id = 0;
    const bool confirm_id_supplied = server.hasArg("ota_confirm_id");
    const bool has_confirm_id = confirm_id_supplied && parse_confirm_id_arg(server, confirm_id);
    if (result.success && confirm_id_supplied && !has_confirm_id) {
        send_invalid_confirm_id_json(server);
        return;
    }

    if (result.success && runtime.prepare_physical_confirm) {
        const OtaPhysicalConfirm::PrepareDecision confirm_decision =
            runtime.prepare_physical_confirm(expected_size, has_confirm_id, confirm_id);
        if (confirm_decision.status != OtaPhysicalConfirm::PrepareStatus::Ready) {
            if (confirm_decision.status == OtaPhysicalConfirm::PrepareStatus::Required &&
                runtime.set_ui_screen) {
                runtime.set_ui_screen(WebUiBridge::FirmwareUpdateScreenMode::ConfirmPending);
            }
            if (!has_confirm_id ||
                confirm_decision.status != OtaPhysicalConfirm::PrepareStatus::Required) {
                LOGI("OTA", "prepare physical confirm status=%s confirm_id=%u expected=%u",
                     OtaPhysicalConfirm::prepareStatusText(confirm_decision.status),
                     static_cast<unsigned>(confirm_decision.confirm_id),
                     static_cast<unsigned>(expected_size));
            }
            send_ota_physical_confirm_prepare_json(server, confirm_decision);
            return;
        }
        confirm_id = confirm_decision.confirm_id;
    }

    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();
    WebOtaApiUtils::fillPrepareJson(root, result);
    if (result.success && confirm_id != 0) {
        root["confirm_id"] = confirm_id;
    }
    String json;
    serializeJson(doc, json);

    if (result.success) {
        runtime.arm_preflight_ui();
    }
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(result.status_code, "application/json", json);
}

void handleUpload(Runtime &runtime, bool ota_busy) {
    if (!runtime.context.server) {
        return;
    }

    WebRequest &server = *runtime.context.server;
    const WebUpload upload = server.upload();

    if (upload.status == WebUploadStatus::Start) {
        if (ota_busy) {
            LOGW("OTA", "reject upload start while OTA is busy");
            server.rejectUpload();
            return;
        }
        if (OtaRollback::isPendingVerify()) {
            LOGW("OTA", "reject upload start while boot validation is pending");
            runtime.ota_state.beginUpload(millis());
            fail_upload(runtime, kOtaBootPendingVerifyError);
            server.rejectUpload();
            return;
        }

        size_t expected_size = 0;
        const bool size_supplied = server.hasArg("ota_size");
        const bool size_known =
            size_supplied && WebTextUtils::parsePositiveSize(server.arg("ota_size"), expected_size);
        if (!size_known) {
            const uint32_t now_ms = millis();
            runtime.ota_state.beginUpload(now_ms);
            fail_upload(runtime,
                        size_supplied ? "Invalid firmware size" : "Firmware size is required");
            server.rejectUpload();
            LOGW("OTA", "reject upload start without valid ota_size");
            return;
        }

        uint32_t confirm_id = 0;
        const bool has_confirm_id = parse_confirm_id_arg(server, confirm_id);
        if (runtime.consume_physical_confirm) {
            const OtaPhysicalConfirm::ConsumeDecision confirm_decision =
                runtime.consume_physical_confirm(expected_size, has_confirm_id, confirm_id);
            if (confirm_decision.status != OtaPhysicalConfirm::ConsumeStatus::Consumed) {
                const uint32_t now_ms = millis();
                runtime.ota_state.beginUpload(now_ms);
                fail_upload(runtime, upload_confirm_error_message(confirm_decision.status));
                server.rejectUpload();
                LOGW("OTA", "reject upload start physical_confirm=%s confirm_id=%u expected=%u",
                     OtaPhysicalConfirm::consumeStatusText(confirm_decision.status),
                     static_cast<unsigned>(confirm_decision.confirm_id),
                     static_cast<unsigned>(expected_size));
                return;
            }
            LOGI("OTA", "physical confirm consumed (confirm_id=%u, expected=%u)",
                 static_cast<unsigned>(confirm_decision.confirm_id),
                 static_cast<unsigned>(expected_size));
        }

        if (runtime.cancel_preflight_ui) {
            runtime.cancel_preflight_ui();
        }
        const uint32_t start_ms = millis();
        runtime.ota_state.beginUpload(start_ms);
        if (runtime.disable_wifi_power_save_for_upload) {
            runtime.disable_wifi_power_save_for_upload();
        }
        if (WiFi.status() == WL_CONNECTED) {
            runtime.ota_state.setStartRssi(WiFi.RSSI());
        }
        const uint32_t client_timeout_ms = runtime.upload_timeout_ms
                                               ? runtime.upload_timeout_ms(size_known ? expected_size : 0)
                                               : 0;
        server.setUploadDeadlineMs(client_timeout_ms);
        runtime.ota_state.setTotalTimeoutMs(client_timeout_ms);
        if (runtime.context.wifi_stop_scan) {
            runtime.context.wifi_stop_scan();
        }
        if (runtime.set_ui_screen) {
            runtime.set_ui_screen(WebUiBridge::FirmwareUpdateScreenMode::Installing);
        }

        const esp_partition_t *target_partition = esp_ota_get_next_update_partition(nullptr);
        if (!target_partition) {
            fail_upload(runtime, "OTA partition unavailable");
            LOGE("OTA", "no target partition");
            return;
        }
        runtime.ota_state.setSlotSize(target_partition->size);
        runtime.ota_state.setExpectedSize(size_known, expected_size);
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        if (ota.size_known) {
            if (ota.expected_size > ota.slot_size) {
                fail_upload(runtime,
                            String("Firmware too large for OTA slot: ") +
                                String(ota.expected_size) + " > " + String(ota.slot_size));
                LOGW("OTA", "reject oversized image: %u > %u",
                     static_cast<unsigned>(ota.expected_size),
                     static_cast<unsigned>(ota.slot_size));
                return;
            }
            if (!Update.begin(ota.expected_size, U_FLASH)) {
                fail_upload(runtime, ota_error_prefixed("Update begin failed"));
                LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
                return;
            }
        } else {
            if (!Update.begin(ota.slot_size, U_FLASH)) {
                fail_upload(runtime, ota_error_prefixed("Update begin failed"));
                LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
                return;
            }
        }

        if (ota.start_rssi_valid) {
            LOGI("OTA", "upload started (session=%u, slot=%u, expected=%u, known=%s, timeout=%u ms, rssi=%d dBm)",
                 static_cast<unsigned>(ota.session_id),
                 static_cast<unsigned>(ota.slot_size),
                 static_cast<unsigned>(ota.expected_size),
                 ota.size_known ? "YES" : "NO",
                 static_cast<unsigned>(client_timeout_ms),
                 ota.start_rssi);
        } else {
            LOGI("OTA", "upload started (session=%u, slot=%u, expected=%u, known=%s, timeout=%u ms, rssi=n/a)",
                 static_cast<unsigned>(ota.session_id),
                 static_cast<unsigned>(ota.slot_size),
                 static_cast<unsigned>(ota.expected_size),
                 ota.size_known ? "YES" : "NO",
                 static_cast<unsigned>(client_timeout_ms));
        }
        return;
    }

    if (server.uploadRejected()) {
        return;
    }

    if (upload.status == WebUploadStatus::Write) {
        if (!runtime.ota_state.isActive() ||
            runtime.ota_state.hasError() ||
            upload.currentSize == 0) {
            return;
        }
        const uint32_t now_ms = millis();
        if (runtime.ota_state.totalTimeoutExceeded(now_ms)) {
            const WebOtaSnapshot ota = runtime.ota_state.snapshot();
            fail_upload(runtime,
                        ota_abort_error_message(ota,
                                                WebUploadAbortReason::TotalTimeout,
                                                now_ms,
                                                server.clientConnected()));
            LOGW("OTA", "upload exceeded total timeout during transfer");
            return;
        }
        if (runtime.ota_state.noteChunk(upload.currentSize, now_ms)) {
            const WebOtaSnapshot chunk_ota = runtime.ota_state.snapshot();
            LOGI("OTA", "first chunk received after %u ms (size=%u bytes)",
                 static_cast<unsigned>(chunk_ota.firstChunkDelayMs()),
                 static_cast<unsigned>(upload.currentSize));
        }
        if (runtime.ota_state.wouldExceedSlot(upload.currentSize)) {
            const WebOtaSnapshot overflow_ota = runtime.ota_state.snapshot();
            fail_upload(runtime,
                        String("Firmware too large for OTA slot: ") +
                            String(overflow_ota.written_size + upload.currentSize) +
                            " > " + String(overflow_ota.slot_size));
            LOGW("OTA", "upload exceeded slot size");
            return;
        }
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            fail_upload(runtime, ota_error_prefixed("Update write failed"));
            LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
            return;
        }
        runtime.ota_state.addWritten(upload.currentSize);
        return;
    }

    if (upload.status == WebUploadStatus::End) {
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        if (!ota.active || ota.hasError()) {
            abort_update_if_running();
            return;
        }
        if (runtime.ota_state.totalTimeoutExceeded(millis())) {
            const uint32_t now_ms = millis();
            fail_upload(runtime,
                        ota_abort_error_message(ota,
                                                WebUploadAbortReason::TotalTimeout,
                                                now_ms,
                                                server.clientConnected()));
            LOGW("OTA", "upload exceeded total timeout before finalize");
            return;
        }
        if (!runtime.ota_state.writtenMatchesExpected()) {
            fail_upload(runtime,
                        String("Firmware size mismatch: expected ") +
                            String(ota.expected_size) + ", got " + String(ota.written_size));
            LOGW("OTA", "size mismatch");
            return;
        }
        const uint32_t finalize_start_ms = millis();
        if (!Update.end(true)) {
            runtime.ota_state.markFinalizeDuration(millis() - finalize_start_ms);
            fail_upload(runtime, ota_error_prefixed("Update finalize failed"));
            LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
            return;
        }
        runtime.ota_state.markFinalizeDuration(millis() - finalize_start_ms);
        runtime.ota_state.markSuccess(millis());
        const WebOtaSnapshot complete_ota = runtime.ota_state.snapshot();
        LOGI("OTA",
             "upload complete (session=%u, written=%u bytes, total=%u ms, first_chunk_seen=%s, first_chunk=%u ms, finalize=%u ms, chunks=%u, chunk[min/avg/max]=%u/%u/%u bytes)",
             static_cast<unsigned>(complete_ota.session_id),
             static_cast<unsigned>(complete_ota.written_size),
             static_cast<unsigned>(complete_ota.totalDurationMs(millis())),
             complete_ota.first_chunk_seen ? "YES" : "NO",
             static_cast<unsigned>(complete_ota.firstChunkDelayMs()),
             static_cast<unsigned>(complete_ota.finalize_ms),
             static_cast<unsigned>(complete_ota.chunk_count),
             static_cast<unsigned>(complete_ota.chunk_min_size),
             static_cast<unsigned>(complete_ota.avgChunkSize()),
             static_cast<unsigned>(complete_ota.chunk_max_size));
        return;
    }

    if (upload.status == WebUploadStatus::Aborted) {
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        const uint32_t now_ms = millis();
        const bool client_connected = server.clientConnected();
        ota_log_abort_summary(server, ota, upload.abort_reason);
        fail_upload(runtime, ota_abort_error_message(ota, upload.abort_reason, now_ms, client_connected));
    }
}

void handleUpdate(Runtime &runtime, bool ota_busy) {
    if (!runtime.context.server) {
        return;
    }

    WebRequest &server = *runtime.context.server;
    if (server.uploadRejected()) {
        const WebOtaSnapshot rejected_ota = runtime.ota_state.snapshot();
        if (rejected_ota.hasError() && rejected_ota.error == kOtaBootPendingVerifyError) {
            send_ota_boot_pending_verify_upload_response(server);
            if (runtime.cancel_preflight_ui) {
                runtime.cancel_preflight_ui();
            }
            cleanup_after_update_response(runtime, false);
            return;
        }
        if (rejected_ota.hasError() && is_upload_confirm_error(rejected_ota.error)) {
            send_ota_physical_confirm_upload_response(server, rejected_ota.error);
            if (runtime.cancel_preflight_ui) {
                runtime.cancel_preflight_ui();
            }
            cleanup_after_update_response(runtime, false);
            return;
        }
        if (!rejected_ota.upload_seen || !rejected_ota.hasError()) {
            send_ota_busy_upload_response(server);
            return;
        }
    }
    const WebOtaSnapshot ota = runtime.ota_state.snapshot();
    if (ota_busy && ota.reboot_pending) {
        send_ota_busy_json(server);
        return;
    }
    if (ota_busy && !ota.success && !ota.upload_seen) {
        send_ota_busy_json(server);
        return;
    }
    const bool has_upload = ota.upload_seen;
    const WebOtaApiUtils::Result result =
        WebOtaApiUtils::buildUpdateResult(has_upload,
                                          has_upload && ota.success && !ota.hasError(),
                                          ota.written_size,
                                          ota.slot_size,
                                          ota.size_known,
                                          ota.expected_size,
                                          ota.error);

    ArduinoJson::JsonDocument doc;
    WebOtaApiUtils::fillUpdateJson(doc.to<ArduinoJson::JsonObject>(), result);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    if (!result.success) {
        const size_t pending_body_bytes = server.pendingRequestBodyBytes();
        if (pending_body_bytes > 0) {
            const size_t drained =
                server.drainPendingRequestBody(kOtaAbortDrainMaxBytes,
                                               kOtaAbortDrainTimeoutMs);
            LOGI("OTA", "drained %u/%u pending request bytes before failure response",
                 static_cast<unsigned>(drained),
                 static_cast<unsigned>(pending_body_bytes));
        }
    }
    server.sendHeader("Connection", "close");
    server.send(result.status_code, "application/json", json);
    server.stopClient();

    if (has_upload) {
        LOGI("OTA",
             "summary session=%u success=%s written=%u slot=%u expected=%u known=%s total=%u ms first_chunk_seen=%s first_chunk=%u ms transfer=%u ms finalize=%u ms chunks=%u chunk[min/avg/max]=%u/%u/%u bytes",
             static_cast<unsigned>(ota.session_id),
             result.success ? "YES" : "NO",
             static_cast<unsigned>(result.written_size),
             static_cast<unsigned>(result.slot_size),
             static_cast<unsigned>(result.expected_size),
             result.size_known ? "YES" : "NO",
             static_cast<unsigned>(ota.totalDurationMs(millis())),
             ota.first_chunk_seen ? "YES" : "NO",
             static_cast<unsigned>(ota.firstChunkDelayMs()),
             static_cast<unsigned>(ota.transferPhaseMs()),
             static_cast<unsigned>(ota.finalize_ms),
             static_cast<unsigned>(ota.chunk_count),
             static_cast<unsigned>(ota.chunk_min_size),
             static_cast<unsigned>(ota.avgChunkSize()),
             static_cast<unsigned>(ota.chunk_max_size));
    }

    if (result.success) {
        runtime.ota_state.markRebootPending();
        LOGI("OTA", "response sent, deferred reboot in %u ms",
             static_cast<unsigned>(runtime.deferred_restart_delay_ms));
        runtime.restart_controller.schedule(millis(), runtime.deferred_restart_delay_ms);
    }
    if (runtime.cancel_preflight_ui) {
        runtime.cancel_preflight_ui();
    }
    cleanup_after_update_response(runtime, result.success);
}

}  // namespace WebOtaHandlers
