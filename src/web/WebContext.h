// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <atomic>

#include "web/WebTransport.h"

class MqttRuntime;
class StorageManager;
class ThemeManager;
class ChartsRuntimeState;
class ConnectivityRuntime;
class WebUiBridge;
class WebRuntimeState;

struct WebHandlerContext {
    WebRequest *server = nullptr;
    StorageManager *storage = nullptr;
    ThemeManager *theme_manager = nullptr;
    const String *hostname = nullptr;

    String *wifi_ssid = nullptr;
    String *wifi_pass = nullptr;
    bool *wifi_enabled = nullptr;
    bool *wifi_enabled_dirty = nullptr;
    std::atomic<bool> *wifi_ui_dirty = nullptr;
    bool *wifi_scan_in_progress = nullptr;
    String *wifi_scan_options = nullptr;
    bool (*wifi_is_connected)() = nullptr;
    uint32_t (*wifi_sta_connected_elapsed_ms)() = nullptr;
    bool (*wifi_is_ap_mode)() = nullptr;
    void (*wifi_start_scan)() = nullptr;
    void (*wifi_stop_scan)() = nullptr;
    void (*wifi_start_sta)() = nullptr;

    MqttRuntime *mqtt_runtime = nullptr;
    bool *mqtt_user_enabled = nullptr;
    String *mqtt_host = nullptr;
    uint16_t *mqtt_port = nullptr;
    String *mqtt_user = nullptr;
    String *mqtt_pass = nullptr;
    String *mqtt_device_name = nullptr;
    String *mqtt_base_topic = nullptr;
    String *mqtt_device_id = nullptr;
    bool *mqtt_discovery = nullptr;
    bool *mqtt_anonymous = nullptr;
    void (*mqtt_sync_with_wifi)() = nullptr;
    ChartsRuntimeState *charts_runtime = nullptr;
    ConnectivityRuntime *connectivity_runtime = nullptr;
    WebRuntimeState *web_runtime = nullptr;
    WebUiBridge *web_ui_bridge = nullptr;
};
