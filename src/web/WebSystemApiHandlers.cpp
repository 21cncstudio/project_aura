// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebSystemApiHandlers.h"

#include <time.h>

#include <ArduinoJson.h>

#include "core/AppVersion.h"
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

void handleStateData(WebHandlerContext &context, bool ota_busy) {
    if (!context.server || !context.web_runtime) {
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    const WebRuntimeSnapshot runtime = context.web_runtime->snapshot();
    const uint32_t uptime_s = millis() / 1000UL;
    const time_t now_epoch = time(nullptr);

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::Payload payload{};
    payload.data = runtime.data;
    payload.uptime_s = uptime_s;
    payload.timestamp_ms = millis();
    payload.has_time_epoch = now_epoch > 0;
    payload.time_epoch_s = static_cast<int64_t>(now_epoch);
    payload.network = WebRuntimeCapture::captureNetworkSnapshot(context);
    payload.settings = WebUiBridgeAdapters::captureSettingsSnapshot(
        context.web_ui_bridge ? context.web_ui_bridge->snapshot() : WebUiBridge::Snapshot{});
    payload.dac_available = runtime.fan.available;
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
