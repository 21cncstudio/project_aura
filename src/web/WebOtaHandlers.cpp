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
#include "web/WebOtaApiUtils.h"
#include "web/WebResponseUtils.h"
#include "web/WebTextUtils.h"

namespace {

constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

String ota_error_prefixed(const char *prefix) {
    String error = prefix;
    error += ": ";
    error += Update.errorString();
    return error;
}

void ota_log_abort_summary(WebRequest &server, const WebOtaSnapshot &ota) {
    const uint32_t now_ms = millis();
    const wl_status_t wifi_status = WiFi.status();
    const bool current_rssi_valid = wifi_status == WL_CONNECTED;
    const int current_rssi = current_rssi_valid ? WiFi.RSSI() : 0;
    const bool client_connected = server.clientConnected();

    LOGW("OTA",
         "upload aborted (written=%u slot=%u expected=%u known=%s total=%u ms first_chunk_seen=%s first_chunk=%u ms last_chunk_age=%u ms chunks=%u chunk[min/avg/max]=%u/%u/%u bytes client_connected=%s wifi_status=%d start_rssi_valid=%s start_rssi=%d current_rssi_valid=%s current_rssi=%d)",
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

void handleUpload(Runtime &runtime, bool ota_busy) {
    if (!runtime.context.server) {
        return;
    }

    WebRequest &server = *runtime.context.server;
    const WebUpload upload = server.upload();

    if (upload.status == WebUploadStatus::Start) {
        if (ota_busy) {
            LOGW("OTA", "reject upload start while OTA is busy");
            return;
        }
        const uint32_t start_ms = millis();
        runtime.ota_state.reset();
        runtime.ota_state.beginUpload(start_ms);
        if (runtime.disable_wifi_power_save_for_upload) {
            runtime.disable_wifi_power_save_for_upload();
        }
        if (WiFi.status() == WL_CONNECTED) {
            runtime.ota_state.setStartRssi(WiFi.RSSI());
        }
        if (runtime.extend_task_wdt) {
            runtime.extend_task_wdt();
        }
        size_t expected_size = 0;
        const bool size_known = WebTextUtils::parsePositiveSize(server.arg("ota_size"), expected_size);
        const uint32_t client_timeout_ms = runtime.upload_timeout_ms
                                               ? runtime.upload_timeout_ms(size_known ? expected_size : 0)
                                               : 0;
        if (runtime.context.wifi_stop_scan) {
            runtime.context.wifi_stop_scan();
        }
        if (runtime.set_ui_screen) {
            runtime.set_ui_screen(true);
        }

        const esp_partition_t *target_partition = esp_ota_get_next_update_partition(nullptr);
        if (!target_partition) {
            if (runtime.set_error) {
                runtime.set_error("OTA partition unavailable");
            }
            LOGE("OTA", "no target partition");
            return;
        }
        runtime.ota_state.setSlotSize(target_partition->size);
        runtime.ota_state.setExpectedSize(size_known, expected_size);
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        if (ota.size_known) {
            if (ota.expected_size > ota.slot_size) {
                if (runtime.set_error) {
                    runtime.set_error(String("Firmware too large for OTA slot: ") +
                                      String(ota.expected_size) + " > " + String(ota.slot_size));
                }
                LOGW("OTA", "reject oversized image: %u > %u",
                     static_cast<unsigned>(ota.expected_size),
                     static_cast<unsigned>(ota.slot_size));
                return;
            }
            if (!Update.begin(ota.expected_size, U_FLASH)) {
                if (runtime.set_error) {
                    runtime.set_error(ota_error_prefixed("Update begin failed"));
                }
                LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
                return;
            }
        } else {
            if (!Update.begin(ota.slot_size, U_FLASH)) {
                if (runtime.set_error) {
                    runtime.set_error(ota_error_prefixed("Update begin failed"));
                }
                LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
                return;
            }
        }

        if (ota.start_rssi_valid) {
            LOGI("OTA", "upload started (slot=%u, expected=%u, known=%s, timeout=%u ms, rssi=%d dBm)",
                 static_cast<unsigned>(ota.slot_size),
                 static_cast<unsigned>(ota.expected_size),
                 ota.size_known ? "YES" : "NO",
                 static_cast<unsigned>(client_timeout_ms),
                 ota.start_rssi);
        } else {
            LOGI("OTA", "upload started (slot=%u, expected=%u, known=%s, timeout=%u ms, rssi=n/a)",
                 static_cast<unsigned>(ota.slot_size),
                 static_cast<unsigned>(ota.expected_size),
                 ota.size_known ? "YES" : "NO",
                 static_cast<unsigned>(client_timeout_ms));
        }
        return;
    }

    if (upload.status == WebUploadStatus::Write) {
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        if (!ota.active || ota.hasError() || upload.currentSize == 0) {
            return;
        }
        const uint32_t now_ms = millis();
        if (runtime.ota_state.noteChunk(upload.currentSize, now_ms)) {
            const WebOtaSnapshot chunk_ota = runtime.ota_state.snapshot();
            LOGI("OTA", "first chunk received after %u ms (size=%u bytes)",
                 static_cast<unsigned>(chunk_ota.firstChunkDelayMs()),
                 static_cast<unsigned>(upload.currentSize));
        }
        if (runtime.ota_state.wouldExceedSlot(upload.currentSize)) {
            if (Update.isRunning()) {
                Update.abort();
            }
            const WebOtaSnapshot overflow_ota = runtime.ota_state.snapshot();
            if (runtime.set_error) {
                runtime.set_error(String("Firmware too large for OTA slot: ") +
                                  String(overflow_ota.written_size + upload.currentSize) +
                                  " > " + String(overflow_ota.slot_size));
            }
            LOGW("OTA", "upload exceeded slot size");
            return;
        }
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            if (Update.isRunning()) {
                Update.abort();
            }
            if (runtime.set_error) {
                runtime.set_error(ota_error_prefixed("Update write failed"));
            }
            LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
            return;
        }
        runtime.ota_state.addWritten(upload.currentSize);
        return;
    }

    if (upload.status == WebUploadStatus::End) {
        const WebOtaSnapshot ota = runtime.ota_state.snapshot();
        if (!ota.active || ota.hasError()) {
            if (Update.isRunning()) {
                Update.abort();
            }
            return;
        }
        if (!runtime.ota_state.writtenMatchesExpected()) {
            if (Update.isRunning()) {
                Update.abort();
            }
            if (runtime.set_error) {
                runtime.set_error(String("Firmware size mismatch: expected ") +
                                  String(ota.expected_size) + ", got " + String(ota.written_size));
            }
            LOGW("OTA", "size mismatch");
            return;
        }
        const uint32_t finalize_start_ms = millis();
        if (!Update.end(true)) {
            runtime.ota_state.markFinalizeDuration(millis() - finalize_start_ms);
            if (runtime.set_error) {
                runtime.set_error(ota_error_prefixed("Update finalize failed"));
            }
            LOGE("OTA", "%s", runtime.ota_state.snapshot().error.c_str());
            return;
        }
        runtime.ota_state.markFinalizeDuration(millis() - finalize_start_ms);
        runtime.ota_state.markSuccess();
        const WebOtaSnapshot complete_ota = runtime.ota_state.snapshot();
        LOGI("OTA",
             "upload complete, written=%u bytes, total=%u ms, first_chunk_seen=%s, first_chunk=%u ms, finalize=%u ms, chunks=%u, chunk[min/avg/max]=%u/%u/%u bytes",
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
        if (Update.isRunning()) {
            Update.abort();
        }
        ota_log_abort_summary(server, runtime.ota_state.snapshot());
        if (runtime.set_error) {
            runtime.set_error("Upload aborted");
        }
    }
}

void handleUpdate(Runtime &runtime, bool ota_busy) {
    if (!runtime.context.server) {
        return;
    }

    WebRequest &server = *runtime.context.server;
    const WebOtaSnapshot ota = runtime.ota_state.snapshot();
    if (ota_busy && !ota.success) {
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
    server.send(result.status_code, "application/json", json);

    if (has_upload) {
        LOGI("OTA",
             "summary success=%s written=%u slot=%u expected=%u known=%s total=%u ms first_chunk_seen=%s first_chunk=%u ms transfer=%u ms finalize=%u ms chunks=%u chunk[min/avg/max]=%u/%u/%u bytes",
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
        LOGI("OTA", "response sent, deferred reboot in %u ms",
             static_cast<unsigned>(runtime.deferred_restart_delay_ms));
        runtime.restart_controller.schedule(millis(), runtime.deferred_restart_delay_ms);
    } else if (runtime.set_ui_screen) {
        runtime.set_ui_screen(false);
    }
    if (runtime.restore_wifi_power_save) {
        runtime.restore_wifi_power_save();
    }
    if (runtime.restore_task_wdt) {
        runtime.restore_task_wdt();
    }
    runtime.ota_state.reset();
}

}  // namespace WebOtaHandlers
