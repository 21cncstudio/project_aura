// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "modules/StorageManager.h"
#include "web/WebHandlers.h"

class PubSubClient;
class ThemeManager;

class AuraNetworkManager {
public:
    enum WifiState { WIFI_STATE_OFF, WIFI_STATE_STA_CONNECTING, WIFI_STATE_STA_CONNECTED, WIFI_STATE_AP_CONFIG };

    using StateChangeCallback = void (*)(WifiState prev, WifiState curr, bool connected, void *ctx);

    void begin(StorageManager &storage);
    void attachMqttContext(PubSubClient &client,
                           bool &mqtt_user_enabled,
                           uint8_t &mqtt_connect_fail_count,
                           String &mqtt_host,
                           uint16_t &mqtt_port,
                           String &mqtt_user,
                           String &mqtt_pass,
                           String &mqtt_device_name,
                           String &mqtt_base_topic,
                           String &mqtt_device_id,
                           bool &mqtt_discovery,
                           bool &mqtt_anonymous,
                           void (*mqtt_sync_with_wifi)());
    void attachThemeContext(ThemeManager &themeManager);
    void setStateChangeCallback(StateChangeCallback cb, void *ctx);
    void poll();

    bool setEnabled(bool enabled);
    bool applyEnabledIfDirty();
    void clearCredentials();
    void connectSta();
    void startApOnDemand();
    void startScan();
    void setMqttScreenOpen(bool open) { mqtt_ui_open_ = open; }

    bool isEnabled() const { return wifi_enabled_; }
    bool isEnabledDirty() const { return wifi_enabled_dirty_; }
    WifiState state() const { return wifi_state_; }
    bool isConnected() const { return wifi_state_ == WIFI_STATE_STA_CONNECTED; }
    bool isUiDirty() const { return wifi_ui_dirty_; }
    void clearUiDirty() { wifi_ui_dirty_ = false; }
    void markUiDirty() { wifi_ui_dirty_ = true; }
    const String &ssid() const { return wifi_ssid_; }
    const String &pass() const { return wifi_pass_; }
    uint8_t retryCount() const { return wifi_retry_count_; }
    const String &scanOptions() const { return wifi_scan_options_; }
    bool scanInProgress() const { return wifi_scan_in_progress_; }

private:
    void warmupIfDisabled();
    void startSta();
    void startAp();
    void stopAp();
    void notifyStateChangeIfNeeded();

    StorageManager *storage_ = nullptr;
    WebServer server_{80};
    WebHandlerContext web_ctx_{};

    WifiState wifi_state_ = WIFI_STATE_OFF;
    WifiState wifi_state_last_ = WIFI_STATE_OFF;
    uint32_t wifi_connect_start_ms_ = 0;
    String wifi_ssid_;
    String wifi_pass_;
    String wifi_scan_options_;
    bool wifi_scan_in_progress_ = false;
    uint32_t wifi_scan_started_ms_ = 0;
    uint8_t wifi_retry_count_ = 0;
    uint32_t wifi_retry_at_ms_ = 0;
    bool wifi_enabled_ = false;
    bool wifi_enabled_dirty_ = false;
    bool wifi_ui_dirty_ = false;
    bool mqtt_ui_open_ = false;
    StateChangeCallback state_change_cb_ = nullptr;
    void *state_change_ctx_ = nullptr;
};
