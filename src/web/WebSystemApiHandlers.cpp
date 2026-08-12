// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSystemApiHandlers.h"

#include <time.h>

#include <ArduinoJson.h>

#include "core/AppVersion.h"
#include "core/BoardInit.h"
#include "core/BootDiagnostics.h"
#include "core/BootHelpers.h"
#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "core/WebRuntimeState.h"
#include "web/WebDiagApiUtils.h"
#include "web/WebEventsApiUtils.h"
#include "web/WebResponseUtils.h"
#include "web/WebRuntimeCapture.h"
#include "web/WebStateApiUtils.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"
#include "web/WebUiBridgeAdapters.h"

namespace {

constexpr size_t kEventsApiMaxEntries = 48;
constexpr size_t kDiagMaxErrorItems = 12;
constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";
Logger::RecentEntry g_events_snapshot[kEventsApiMaxEntries];

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

}  // namespace

namespace WebSystemApiHandlers {

void handleDiagRoot(WebHandlerContext &context,
                    const WebResponseUtils::StreamContext &stream_context) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!WebDiagApiUtils::accessAllowed(connectivity.wifi_ap_mode, connectivity.wifi_connected)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }
    WebResponseUtils::sendHtmlStreamProgmem(*context.server,
                                            reinterpret_cast<const uint8_t *>(
                                                WebTemplates::kDiagPageTemplate),
                                            sizeof(WebTemplates::kDiagPageTemplate) - 1,
                                            false,
                                            stream_context);
}

void handleDiagData(WebHandlerContext &context,
                    bool ota_busy,
                    const WebTransferSnapshot &web_stream_snapshot) {
    if (!context.server || !context.connectivity_runtime) {
        return;
    }
    const ConnectivityRuntimeSnapshot connectivity = context.connectivity_runtime->snapshot();
    if (!WebDiagApiUtils::accessAllowed(connectivity.wifi_ap_mode, connectivity.wifi_connected)) {
        context.server->send(404, "text/plain", "Not found");
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    ArduinoJson::JsonDocument doc;
    const size_t event_count = Logger::copyRecentAlerts(g_events_snapshot, kEventsApiMaxEntries);
    WebDiagApiUtils::Payload payload{};
    payload.uptime_s = millis() / 1000UL;
    payload.ota_busy = ota_busy;
    payload.heap_free = ESP.getFreeHeap();
    payload.heap_min_free = ESP.getMinFreeHeap();
    payload.network = WebRuntimeCapture::captureNetworkSnapshot(context);
    payload.web_stream = web_stream_snapshot;
    const BootDiagnostics::Snapshot &boot = BootDiagnostics::state;
    payload.boot.reset_reason = BootHelpers::resetReasonText(boot.reset_reason);
    payload.boot.auto_recovery_boot = boot.auto_recovery_boot;
    payload.boot.i2c_status = I2cBusRecovery::statusText(boot.i2c_status);
    payload.boot.sda_high = boot.sda_high;
    payload.boot.scl_high = boot.scl_high;
    payload.boot.board_ready = boot.board_ready;
    payload.boot.board_rounds = boot.board_rounds;
    payload.boot.board_begin_attempts = boot.board_begin_attempts;
    payload.boot.cold_power_start = boot.cold_power_start;
    payload.boot.cold_power_wait_ms = boot.cold_power_wait_ms;
    payload.boot.expander_probe_status = Ch422gReadyProbe::statusText(boot.expander_probe_status);
    payload.boot.expander_probe_result_valid =
        boot.expander_probe_status != Ch422gReadyProbe::Status::NotRun;
    payload.boot.expander_probe_attempts = boot.expander_probe_attempts;
    payload.boot.expander_probe_wait_ms = boot.expander_probe_wait_ms;
    payload.boot.expander_probe_error = boot.expander_probe_error;
    payload.boot.expander_probe_phase = Ch422gReadyProbe::phaseText(boot.expander_probe_phase);
    payload.boot.expander_probe_failed_address = boot.expander_probe_failed_address;
    payload.boot.expander_probe_failed_value = boot.expander_probe_failed_value;
    payload.boot.expander_probe_bus_recoveries = boot.expander_probe_bus_recoveries;
    payload.boot.expander_probe_failure_lines_valid = boot.expander_probe_failure_lines_valid;
    payload.boot.expander_probe_failure_sda_high = boot.expander_probe_failure_sda_high;
    payload.boot.expander_probe_failure_scl_high = boot.expander_probe_failure_scl_high;
    payload.boot.expander_probe_recovery_sda_high = boot.expander_probe_recovery_sda_high;
    payload.boot.expander_probe_recovery_scl_high = boot.expander_probe_recovery_scl_high;
    payload.boot.expander_probe_recovery_pulses = boot.expander_probe_recovery_pulses;
    payload.boot.board_stage = BoardInit::stageText(boot.board_stage);
    payload.boot.board_failure = BoardInit::failureText(boot.board_failure);
    payload.boot.lvgl_ready = boot.lvgl_ready;
    WebDiagApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(),
                              payload,
                              g_events_snapshot,
                              event_count,
                              kDiagMaxErrorItems);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

void handleStateData(WebHandlerContext &context, bool ota_busy, const WebOtaSnapshot &ota_snapshot) {
    if (!context.server || !context.web_runtime) {
        return;
    }

    const WebRuntimeSnapshot runtime = context.web_runtime->snapshot();
    const uint32_t uptime_s = millis() / 1000UL;
    const time_t now_epoch = time(nullptr);

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::Payload payload{};
    payload.data = runtime.data;
    payload.gas_warmup = runtime.gas_warmup;
    payload.uptime_s = uptime_s;
    payload.timestamp_ms = millis();
    payload.has_time_epoch = now_epoch > 0;
    payload.time_epoch_s = static_cast<int64_t>(now_epoch);
    payload.network = WebRuntimeCapture::captureNetworkSnapshot(context);
    const WebUiBridge::Snapshot ui_snapshot =
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{};
    payload.settings = WebUiBridgeAdapters::captureSettingsSnapshot(ui_snapshot);
    payload.thresholds = context.display_thresholds
        ? context.display_thresholds->snapshot()
        : DisplayThresholds::defaults();
    payload.ntp_active = ui_snapshot.ntp_active;
    payload.ntp_syncing = ui_snapshot.ntp_syncing;
    payload.ntp_error = ui_snapshot.ntp_error;
    payload.ntp_last_sync_ms = ui_snapshot.ntp_last_sync_ms;
    payload.dac_available = runtime.fan.available;
    payload.ota_busy = ota_busy;
    payload.ota = ota_snapshot;
    payload.firmware = AppVersion::fullVersion();
    payload.build_date = __DATE__;
    payload.build_time = __TIME__;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

void handleEventsData(WebHandlerContext &context, bool ota_busy) {
    if (!context.server) {
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    const size_t count = Logger::copyRecent(g_events_snapshot, kEventsApiMaxEntries);

    ArduinoJson::JsonDocument doc;
    WebEventsApiUtils::fillJson(
        doc.to<ArduinoJson::JsonObject>(), g_events_snapshot, count, millis() / 1000UL);

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(*context.server);
    context.server->send(200, "application/json", json);
}

}  // namespace WebSystemApiHandlers
