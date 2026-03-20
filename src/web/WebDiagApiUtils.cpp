// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDiagApiUtils.h"

#include "web/WebEventsUtils.h"
#include "web/WebNetworkUtils.h"
#include "web/WebStreamPolicy.h"

namespace WebDiagApiUtils {

bool accessAllowed(bool ap_mode, bool sta_connected) {
    return ap_mode || sta_connected;
}

void fillJson(ArduinoJson::JsonObject root,
              const Payload &payload,
              const Logger::RecentEntry *recent_errors,
              size_t recent_error_count,
              size_t max_error_items) {
    root["success"] = true;
    root["uptime_s"] = payload.uptime_s;
    root["ota_busy"] = payload.ota_busy;

    ArduinoJson::JsonObject heap = root["heap"].to<ArduinoJson::JsonObject>();
    heap["free"] = payload.heap_free;
    heap["min_free"] = payload.heap_min_free;

    ArduinoJson::JsonObject network = root["network"].to<ArduinoJson::JsonObject>();
    WebNetworkUtils::fillDiagJson(network, payload.network);

    ArduinoJson::JsonArray last_errors = root["last_errors"].to<ArduinoJson::JsonArray>();
    root["error_count"] = WebEventsUtils::fillRecentErrorsJson(
        last_errors, recent_errors, recent_error_count, max_error_items);

    const WebTransferSnapshot &web_stream_snapshot = payload.web_stream;
    ArduinoJson::JsonObject web_stream = root["web_stream"].to<ArduinoJson::JsonObject>();
    web_stream["ok_count"] = web_stream_snapshot.stats.ok_count;
    web_stream["abort_count"] = web_stream_snapshot.stats.abort_count;
    web_stream["slow_count"] = web_stream_snapshot.stats.slow_count;
    web_stream["active_transfers"] = web_stream_snapshot.active_transfers;
    web_stream["mqtt_pause_remaining_ms"] = web_stream_snapshot.mqtt_pause_remaining_ms;
    web_stream["mqtt_connect_deferred_count"] = web_stream_snapshot.stats.mqtt_connect_deferred_count;
    web_stream["mqtt_publish_deferred_count"] = web_stream_snapshot.stats.mqtt_publish_deferred_count;
    web_stream["last_abort_reason"] = stream_abort_reason_text(web_stream_snapshot.stats.last_abort_reason);
    web_stream["last_errno"] = web_stream_snapshot.stats.last_errno;
    web_stream["last_sent"] = static_cast<uint32_t>(web_stream_snapshot.stats.last_sent);
    web_stream["last_total"] = static_cast<uint32_t>(web_stream_snapshot.stats.last_total);
    if (web_stream_snapshot.stats.last_total > 0) {
        web_stream["last_sent_ratio"] =
            static_cast<float>(web_stream_snapshot.stats.last_sent) /
            static_cast<float>(web_stream_snapshot.stats.last_total);
    } else {
        web_stream["last_sent_ratio"] = 1.0f;
    }
    web_stream["last_max_write_ms"] = web_stream_snapshot.stats.last_max_write_ms;
    web_stream["last_uri"] = web_stream_snapshot.stats.last_uri;
}

} // namespace WebDiagApiUtils
