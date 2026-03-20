// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <atomic>
#include <memory>

#include <Arduino.h>
#include "modules/StorageManager.h"
#include "web/WebContext.h"
#include "web/WebTransport.h"

class MqttRuntime;
class NetworkCommandQueue;
class ThemeManager;
class ChartsRuntimeState;
class ConnectivityRuntime;
class WebUiBridge;
class WebRuntimeState;

class AuraNetworkManager {
public:
    enum WifiState { WIFI_STATE_OFF, WIFI_STATE_STA_CONNECTING, WIFI_STATE_STA_CONNECTED, WIFI_STATE_AP_CONFIG };

    using StateChangeCallback = void (*)(WifiState prev, WifiState curr, bool connected, void *ctx);

    void begin(StorageManager &storage);
    void attachMqttContext(MqttRuntime &mqtt_runtime,
                           bool &mqtt_user_enabled,
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
    void attachChartsRuntime(ChartsRuntimeState &chartsRuntime);
    void attachConnectivityRuntime(ConnectivityRuntime &connectivityRuntime);
    void attachWebRuntime(WebRuntimeState &webRuntime);
    void attachWebUiBridge(WebUiBridge &webUiBridge);
    void attachCommandQueue(NetworkCommandQueue &commandQueue);
    void setStateChangeCallback(StateChangeCallback cb, void *ctx);
    void poll();
    void noteStaConnectTransientFailure(uint32_t reason);

    bool setEnabled(bool enabled);
    bool applyEnabledIfDirty();
    void applySavedWiFiSettings(const String &ssid, const String &pass, bool enabled);
    void clearCredentials();
    void connectSta();
    void startApOnDemand();
    void startScan();
    void stopScan();

    bool isEnabled() const { return wifi_enabled_; }
    bool isEnabledDirty() const { return wifi_enabled_dirty_; }
    WifiState state() const { return wifi_state_; }
    bool isConnected() const { return wifi_state_ == WIFI_STATE_STA_CONNECTED; }
    uint32_t staConnectedElapsedMs() const {
        if (wifi_state_ != WIFI_STATE_STA_CONNECTED || wifi_connected_since_ms_ == 0) {
            return 0;
        }
        return millis() - wifi_connected_since_ms_;
    }
    bool isUiDirty() const { return wifi_ui_dirty_.load(std::memory_order_acquire); }
    void clearUiDirty() { wifi_ui_dirty_.store(false, std::memory_order_release); }
    void markUiDirty() { wifi_ui_dirty_.store(true, std::memory_order_release); }
    const String &ssid() const { return wifi_ssid_; }
    const String &pass() const { return wifi_pass_; }
    const String &hostname() const { return hostname_; }
    const String &apSsid() const { return ap_ssid_; }
    String localUrl(const char *path = nullptr) const;
    uint8_t retryCount() const { return wifi_retry_count_; }
    const String &scanOptions() const { return wifi_scan_options_; }
    bool scanInProgress() const { return wifi_scan_in_progress_; }

private:
    void registerServerRoutes();
    void ensureServerBackend();
    WebServerBackend &serverBackend();
    void startServerIfNeeded();
    void resetStaConnectAttemptState();
    void scheduleStaRetry(const char *log_reason, bool warn = true);
    void resetColdBootStaAssist();
    bool resolveStaConnectTarget(int32_t &channel_out, uint8_t bssid_out[6], int32_t &rssi_out);
    void warmupIfDisabled();
    void startSta();
    void startAp();
    void stopAp();
    void startMdns();
    void stopMdns();
    void notifyStateChangeIfNeeded();

    StorageManager *storage_ = nullptr;
    std::unique_ptr<WebServerBackend> server_backend_;
    WebHandlerContext web_ctx_{};

    WifiState wifi_state_ = WIFI_STATE_OFF;
    WifiState wifi_state_last_ = WIFI_STATE_OFF;
    uint32_t wifi_connect_start_ms_ = 0;
    uint32_t wifi_connected_since_ms_ = 0;
    uint8_t sta_link_fail_streak_ = 0;
    String wifi_ssid_;
    String wifi_pass_;
    String hostname_;
    String ap_ssid_;
    String wifi_scan_options_;
    bool wifi_scan_in_progress_ = false;
    uint32_t wifi_scan_started_ms_ = 0;
    uint8_t wifi_retry_count_ = 0;
    uint32_t wifi_retry_at_ms_ = 0;
    bool wifi_enabled_ = false;
    bool wifi_enabled_dirty_ = false;
    std::atomic<bool> wifi_ui_dirty_{false};
    std::atomic<uint8_t> wifi_connect_transient_failures_{0};
    std::atomic<uint32_t> wifi_connect_last_transient_reason_{0};
    bool wifi_cold_boot_warmup_pending_ = false;
    bool wifi_cold_boot_warmup_active_ = false;
    uint8_t wifi_cold_boot_soft_connects_left_ = 0;
    bool wifi_cold_boot_targeted_connect_active_ = false;
    bool server_routes_registered_ = false;
    bool mdns_started_ = false;
    StateChangeCallback state_change_cb_ = nullptr;
    void *state_change_ctx_ = nullptr;
};
