// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebRuntimeCapture.h"

#include "core/ConnectivityRuntime.h"

namespace WebRuntimeCapture {

WebNetworkUtils::Snapshot captureNetworkSnapshot(const WebHandlerContext &context) {
    WebNetworkUtils::Snapshot snapshot{};
    const ConnectivityRuntimeSnapshot connectivity =
        context.connectivity_runtime ? context.connectivity_runtime->snapshot()
                                     : ConnectivityRuntimeSnapshot{};

    snapshot.wifi_enabled = connectivity.wifi_enabled;
    snapshot.ap_mode = connectivity.wifi_ap_mode;
    snapshot.sta_connected = connectivity.wifi_connected;
    snapshot.scan_in_progress = connectivity.wifi_scan_in_progress;
    snapshot.sta_status = connectivity.wifi_sta_status;

    if (snapshot.ap_mode) {
        snapshot.wifi_ssid = connectivity.ap_ssid;
    } else {
        snapshot.wifi_ssid = connectivity.wifi_ssid;
    }

    snapshot.ip = snapshot.ap_mode ? connectivity.ap_ip : connectivity.sta_ip;

    if (!connectivity.hostname.isEmpty()) {
        snapshot.has_hostname = true;
        snapshot.hostname = connectivity.hostname;
    }

    if (connectivity.has_rssi) {
        snapshot.has_rssi = true;
        snapshot.rssi = connectivity.rssi;
    }

    if (!connectivity.mqtt_host.isEmpty()) {
        snapshot.has_mqtt_broker = true;
        snapshot.mqtt_broker = connectivity.mqtt_host;
    }
    snapshot.mqtt_enabled = connectivity.mqtt_enabled;
    snapshot.mqtt_connected = connectivity.mqtt_connected;

    return snapshot;
}

} // namespace WebRuntimeCapture
