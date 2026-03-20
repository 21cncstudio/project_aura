// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace WebNetworkUtils {

struct Snapshot {
    bool wifi_enabled = false;
    bool ap_mode = false;
    bool sta_connected = false;
    bool scan_in_progress = false;
    int sta_status = 0;
    String wifi_ssid;
    String ip;
    bool has_hostname = false;
    String hostname;
    bool has_rssi = false;
    int rssi = 0;
    bool has_mqtt_broker = false;
    String mqtt_broker;
    bool mqtt_enabled = false;
    bool mqtt_connected = false;
};

void fillDiagJson(ArduinoJson::JsonObject network, const Snapshot &snapshot);
void fillStateJson(ArduinoJson::JsonObject network, const Snapshot &snapshot);

} // namespace WebNetworkUtils
