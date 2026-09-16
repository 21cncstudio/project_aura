// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebNetworkUtils.h"

namespace {

const char *mode_text(const WebNetworkUtils::Snapshot &snapshot) {
    if (snapshot.ap_mode) {
        return "ap";
    }
    if (snapshot.sta_connected) {
        return "sta";
    }
    return "off";
}

const char *diag_hostname(const WebNetworkUtils::Snapshot &snapshot) {
    if (snapshot.has_hostname && snapshot.hostname.length() > 0) {
        return snapshot.hostname.c_str();
    }
    return "aura";
}

const char *state_hostname(const WebNetworkUtils::Snapshot &snapshot) {
    if (snapshot.has_hostname) {
        return snapshot.hostname.c_str();
    }
    return "aura";
}

void fill_hostname_status(ArduinoJson::JsonObject network, const NetworkHostnameSnapshot &status) {
    auto json = network["hostname_status"].to<ArduinoJson::JsonObject>();
    json["sta"] = status.available ? status.actual : nullptr;
    if (status.available) {
        json["matches"] = status.matches;
    } else {
        json["matches"] = nullptr;
    }
    if (status.apply_attempted) {
        json["apply_error"] = status.apply_error;
    } else {
        json["apply_error"] = nullptr;
    }
    json["apply_failures"] = status.apply_failures;
    json["mismatch_count"] = status.mismatch_count;
    if (status.apply_failures != 0) {
        json["last_failure_error"] = status.last_failure_error;
    } else {
        json["last_failure_error"] = nullptr;
    }
    if (status.checked) {
        json["read_error"] = status.read_error;
        json["checked_at_ms"] = status.checked_at_ms;
    } else {
        json["read_error"] = nullptr;
        json["checked_at_ms"] = nullptr;
    }
}

} // namespace

namespace WebNetworkUtils {

void fillDiagJson(ArduinoJson::JsonObject network, const Snapshot &snapshot) {
    network["wifi_enabled"] = snapshot.wifi_enabled;
    network["mode"] = mode_text(snapshot);
    network["sta_status"] = snapshot.sta_status;
    network["scan_in_progress"] = snapshot.scan_in_progress;
    network["wifi_ssid"] = snapshot.wifi_ssid;
    network["ip"] = snapshot.ip;
    network["hostname"] = diag_hostname(snapshot);
    fill_hostname_status(network, snapshot.hostname_status);
    if (snapshot.has_rssi) {
        network["rssi"] = snapshot.rssi;
    } else {
        network["rssi"] = nullptr;
    }
}

void fillStateJson(ArduinoJson::JsonObject network, const Snapshot &snapshot) {
    network["wifi_enabled"] = snapshot.wifi_enabled;
    network["mode"] = mode_text(snapshot);
    network["wifi_ssid"] = snapshot.wifi_ssid;
    network["ip"] = snapshot.ip;
    if (snapshot.has_rssi) {
        network["rssi"] = snapshot.rssi;
    } else {
        network["rssi"] = nullptr;
    }
    network["hostname"] = state_hostname(snapshot);
    fill_hostname_status(network, snapshot.hostname_status);
    network["mqtt_broker"] = snapshot.has_mqtt_broker ? snapshot.mqtt_broker.c_str() : "";
    network["mqtt_enabled"] = snapshot.mqtt_enabled;
    network["mqtt_connected"] = snapshot.mqtt_connected;
}

} // namespace WebNetworkUtils
