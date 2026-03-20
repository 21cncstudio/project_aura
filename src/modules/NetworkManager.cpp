// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/NetworkManager.h"

#include <cstring>
#include <memory>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include "core/BootState.h"
#include "core/NetworkCommandQueue.h"
#include "core/Logger.h"
// [NEW] Included for system event publishing to Home Assistant.
#include "modules/MqttManager.h"
#include "config/AppConfig.h"
#include "ui/ThemeManager.h"
#include "web/WebHandlers.h"
#include "web/WebInputValidation.h"
#include "web/WebRuntime.h"
#include "web/WebTemplates.h"
#include "web/WebUiBridge.h"

namespace {

AuraNetworkManager *g_network = nullptr;
NetworkCommandQueue *g_network_command_queue = nullptr;
wifi_event_id_t g_wifi_disconnect_event_handle = 0;
const uint32_t kInitialWifiConnectDelayMs = 3000;
constexpr uint32_t kWifiInternalHeapMinFreeForStart = 32UL * 1024UL;
constexpr uint32_t kWifiInternalHeapMinLargestForStart = 16UL * 1024UL;
constexpr uint32_t kWifiScanTimeoutMs = 20000UL;
constexpr uint8_t kStaLinkFailThreshold = 3;
constexpr uint8_t kStaConnectTransientFailureThreshold = 2;
constexpr uint32_t kWifiStaTransitionTimeoutMs = 1000UL;
constexpr uint32_t kWifiStaTransitionPollMs = 10UL;
constexpr uint32_t kWifiStaStartSettleMs = 150UL;
constexpr uint32_t kWifiColdBootWarmupMs = 2500UL;
constexpr uint8_t kWifiColdBootSoftConnectAttempts = 3;

bool is_retryable_connect_reason(wifi_err_reason_t reason) {
    return reason == WIFI_REASON_AUTH_EXPIRE ||
           reason == WIFI_REASON_AUTH_LEAVE ||
           reason == WIFI_REASON_NO_AP_FOUND ||
           reason == WIFI_REASON_ASSOC_EXPIRE;
}

bool has_internal_heap_for_wifi_start(uint32_t &free_bytes, uint32_t &largest_block_bytes) {
    free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    largest_block_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return (free_bytes >= kWifiInternalHeapMinFreeForStart) &&
           (largest_block_bytes >= kWifiInternalHeapMinLargestForStart);
}

uint32_t mac_suffix_24bit() {
    return static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
}

String build_wifi_hostname() {
    char hostname[16];
    snprintf(hostname, sizeof(hostname), "aura-%06x", static_cast<unsigned>(mac_suffix_24bit()));
    return String(hostname);
}

String build_ap_ssid() {
    char ssid[20];
    snprintf(ssid, sizeof(ssid), "Aura-%06X-AP", static_cast<unsigned>(mac_suffix_24bit()));
    return String(ssid);
}

void format_wifi_event_ssid(const uint8_t *ssid, uint8_t ssid_len, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }

    size_t write = 0;
    const size_t limit = ssid_len < 32 ? ssid_len : 32;
    for (size_t i = 0; i < limit && write + 1 < out_size; ++i) {
        const char c = static_cast<char>(ssid[i]);
        out[write++] = (c >= 32 && c <= 126) ? c : '?';
    }
    out[write] = '\0';
}

void format_wifi_event_bssid(const uint8_t *bssid, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             static_cast<unsigned>(bssid[0]),
             static_cast<unsigned>(bssid[1]),
             static_cast<unsigned>(bssid[2]),
             static_cast<unsigned>(bssid[3]),
             static_cast<unsigned>(bssid[4]),
             static_cast<unsigned>(bssid[5]));
}

void network_wifi_event(arduino_event_id_t event, arduino_event_info_t info) {
    if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        return;
    }

    const wifi_event_sta_disconnected_t &disc = info.wifi_sta_disconnected;
    char ssid[33];
    char bssid[18];
    format_wifi_event_ssid(disc.ssid, disc.ssid_len, ssid, sizeof(ssid));
    format_wifi_event_bssid(disc.bssid, bssid, sizeof(bssid));

    const wifi_err_reason_t reason = static_cast<wifi_err_reason_t>(disc.reason);
    const char *reason_name = WiFi.disconnectReasonName(reason);
    const wl_status_t status = WiFi.status();
    const bool is_connecting =
        g_network && g_network->state() == AuraNetworkManager::WIFI_STATE_STA_CONNECTING;
    const bool retryable_connect_reason = is_connecting && is_retryable_connect_reason(reason);

    Logger::log(retryable_connect_reason ? Logger::Info : Logger::Warn,
                "WiFi",
                retryable_connect_reason
                    ? "STA connect transient (reason=%u %s, rssi=%d dBm, status=%d, ota_busy=%s, ssid=%s, bssid=%s)"
                    : "STA disconnected (reason=%u %s, rssi=%d dBm, status=%d, ota_busy=%s, ssid=%s, bssid=%s)",
                static_cast<unsigned>(disc.reason),
                (reason_name && reason_name[0] != '\0') ? reason_name : "unknown",
                static_cast<int>(disc.rssi),
                static_cast<int>(status),
                WebHandlersIsOtaBusy() ? "YES" : "NO",
                ssid[0] != '\0' ? ssid : "<empty>",
                bssid);

    if (retryable_connect_reason && g_network) {
        g_network->noteStaConnectTransientFailure(static_cast<uint32_t>(reason));
    }
}

void network_wifi_start_scan() {
    if (g_network_command_queue) {
        if (g_network_command_queue->enqueue(NetworkCommandQueue::Type::RequestWifiScanStart) ||
            g_network_command_queue->publishWifiScanRequest(
                NetworkCommandQueue::Type::RequestWifiScanStart)) {
            return;
        }
    }
    LOGW("WiFi", "scan start request dropped: command queue unavailable");
}

void network_wifi_stop_scan() {
    if (g_network_command_queue) {
        if (g_network_command_queue->enqueue(NetworkCommandQueue::Type::RequestWifiScanStop) ||
            g_network_command_queue->publishWifiScanRequest(
                NetworkCommandQueue::Type::RequestWifiScanStop)) {
            return;
        }
    }
    LOGW("WiFi", "scan stop request dropped: command queue unavailable");
}

void network_wifi_start_sta() {
    if (g_network) {
        g_network->connectSta();
    }
}

bool network_wifi_is_connected() {
    return g_network && g_network->isConnected();
}

bool wait_for_sta_started(uint32_t timeout_ms) {
    const uint32_t deadline = millis() + timeout_ms;
    while (!WiFi.STA.started()) {
        if (static_cast<int32_t>(millis() - deadline) >= 0) {
            return false;
        }
        delay(kWifiStaTransitionPollMs);
    }
    return true;
}

bool wait_for_sta_stopped(uint32_t timeout_ms) {
    const uint32_t deadline = millis() + timeout_ms;
    while (WiFi.STA.started()) {
        if (static_cast<int32_t>(millis() - deadline) >= 0) {
            return false;
        }
        delay(kWifiStaTransitionPollMs);
    }
    return true;
}

uint32_t network_wifi_sta_connected_elapsed_ms() {
    return g_network ? g_network->staConnectedElapsedMs() : 0;
}

bool network_wifi_is_ap_mode() {
    return g_network && g_network->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG;
}

} // namespace

void AuraNetworkManager::ensureServerBackend() {
    if (!server_backend_) {
        server_backend_ = createDefaultWebServerBackend(80);
    }
}

WebServerBackend &AuraNetworkManager::serverBackend() {
    ensureServerBackend();
    return *server_backend_;
}

void AuraNetworkManager::begin(StorageManager &storage) {
    storage_ = &storage;
    g_network = this;
    mdns_started_ = false;
    ensureServerBackend();
    WiFi.persistent(false);
    if (g_wifi_disconnect_event_handle == 0) {
        g_wifi_disconnect_event_handle =
            WiFi.onEvent(network_wifi_event, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    }
    hostname_ = build_wifi_hostname();
    if (hostname_.isEmpty()) {
        hostname_ = "aura";
    }
    ap_ssid_ = build_ap_ssid();
    if (ap_ssid_.isEmpty()) {
        ap_ssid_ = Config::WIFI_AP_SSID;
    }
    LOGI("WiFi", "hostname: %s", hostname_.c_str());
    LOGI("WiFi", "AP SSID: %s", ap_ssid_.c_str());
    LOGI("WiFi", "web backend: %s", serverBackend().name());
    resetColdBootStaAssist();

    web_ctx_.server = &serverBackend().request();
    web_ctx_.storage = storage_;
    web_ctx_.wifi_scan_options = &wifi_scan_options_;
    web_ctx_.wifi_start_scan = network_wifi_start_scan;
    web_ctx_.wifi_stop_scan = network_wifi_stop_scan;
    web_ctx_.wifi_start_sta = network_wifi_start_sta;
    WebHandlersInit(&web_ctx_);
    registerServerRoutes();

    storage_->loadWiFiSettings(wifi_ssid_, wifi_pass_, wifi_enabled_);
    wifi_enabled_dirty_ = false;
    if (!wifi_ssid_.isEmpty() &&
        !WebInputValidation::isWifiSsidValid(wifi_ssid_, WebInputValidation::kWifiSsidMaxBytes)) {
        LOGW("WiFi", "SSID invalid length, clearing saved credentials");
        storage_->clearWiFiCredentials();
        wifi_ssid_ = "";
        wifi_pass_ = "";
    }

    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            wifi_cold_boot_warmup_pending_ = (boot_reset_reason == ESP_RST_POWERON);
            wifi_cold_boot_targeted_connect_active_ = (boot_reset_reason == ESP_RST_POWERON);
            wifi_state_ = WIFI_STATE_OFF;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = millis() + kInitialWifiConnectDelayMs;
            wifi_connect_start_ms_ = 0;
            wifi_connected_since_ms_ = 0;
            wifi_ui_dirty_ = true;
            Logger::log(Logger::Info, "WiFi",
                        "delaying initial connect %u ms",
                        static_cast<unsigned>(kInitialWifiConnectDelayMs));
        } else {
            startAp();
        }
    } else {
        warmupIfDisabled();
        stopMdns();
        WiFi.persistent(false);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        resetColdBootStaAssist();
        wifi_state_ = WIFI_STATE_OFF;
    }
    wifi_state_last_ = wifi_state_;
}

void AuraNetworkManager::attachMqttContext(MqttRuntime &mqtt_runtime,
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
                                           void (*mqtt_sync_with_wifi)()) {
    web_ctx_.mqtt_runtime = &mqtt_runtime;
    web_ctx_.mqtt_user_enabled = &mqtt_user_enabled;
    web_ctx_.mqtt_host = &mqtt_host;
    web_ctx_.mqtt_port = &mqtt_port;
    web_ctx_.mqtt_user = &mqtt_user;
    web_ctx_.mqtt_pass = &mqtt_pass;
    web_ctx_.mqtt_device_name = &mqtt_device_name;
    web_ctx_.mqtt_base_topic = &mqtt_base_topic;
    web_ctx_.mqtt_device_id = &mqtt_device_id;
    web_ctx_.mqtt_discovery = &mqtt_discovery;
    web_ctx_.mqtt_anonymous = &mqtt_anonymous;
    web_ctx_.mqtt_sync_with_wifi = mqtt_sync_with_wifi;
}

void AuraNetworkManager::attachThemeContext(ThemeManager &themeManager) {
    web_ctx_.theme_manager = &themeManager;
}

void AuraNetworkManager::attachChartsRuntime(ChartsRuntimeState &chartsRuntime) {
    web_ctx_.charts_runtime = &chartsRuntime;
}

void AuraNetworkManager::attachConnectivityRuntime(ConnectivityRuntime &connectivityRuntime) {
    web_ctx_.connectivity_runtime = &connectivityRuntime;
}

void AuraNetworkManager::attachWebRuntime(WebRuntimeState &webRuntime) {
    web_ctx_.web_runtime = &webRuntime;
}

void AuraNetworkManager::attachWebUiBridge(WebUiBridge &webUiBridge) {
    web_ctx_.web_ui_bridge = &webUiBridge;
}

void AuraNetworkManager::attachCommandQueue(NetworkCommandQueue &commandQueue) {
    g_network_command_queue = &commandQueue;
}

void AuraNetworkManager::registerServerRoutes() {
    if (server_routes_registered_) {
        return;
    }

    WebServerBackend &server = serverBackend();
    server.onGet("/", dashboard_handle_root);
    server.onGet("/dashboard", dashboard_handle_root);
    server.onGet(WebTemplates::kDashboardStylesCssPath, dashboard_handle_styles);
    server.onGet(WebTemplates::kDashboardAppJsPath, dashboard_handle_app);
    server.onGet("/wifi", wifi_handle_root);
    server.onGet("/diag", diag_handle_root);
    server.onPost("/save", wifi_handle_save);
    server.onGet("/mqtt", mqtt_handle_root);
    server.onPost("/mqtt", mqtt_handle_save);
    server.onGet("/theme", theme_handle_root);
    server.onGet(WebTemplates::kThemeStylesCssPath, theme_handle_styles);
    server.onGet(WebTemplates::kThemeAppJsPath, theme_handle_app);
    server.onGet("/theme/state", theme_handle_state);
    server.onPost("/theme/apply", theme_handle_apply);
    server.onGet("/dac", dac_handle_root);
    server.onGet(WebTemplates::kDacStylesCssPath, dac_handle_styles);
    server.onGet(WebTemplates::kDacAppJsPath, dac_handle_app);
    server.onGet("/dac/state", dac_handle_state);
    server.onPost("/dac/action", dac_handle_action);
    server.onPost("/dac/auto", dac_handle_auto);
    server.onGet("/api/charts", charts_handle_data);
    server.onGet("/api/state", state_handle_data);
    server.onGet("/api/events", events_handle_data);
    server.onGet("/api/diag", diag_handle_data);
    server.onPost("/api/settings", settings_handle_update);
    server.onPostUpload("/api/ota", ota_handle_update, ota_handle_upload);
    server.onNotFound(wifi_handle_not_found);
    server_routes_registered_ = true;
}

void AuraNetworkManager::startServerIfNeeded() {
    serverBackend().begin();
}

void AuraNetworkManager::resetStaConnectAttemptState() {
    wifi_connect_transient_failures_.store(0, std::memory_order_release);
    wifi_connect_last_transient_reason_.store(0, std::memory_order_release);
}

void AuraNetworkManager::resetColdBootStaAssist() {
    wifi_cold_boot_warmup_pending_ = false;
    wifi_cold_boot_warmup_active_ = false;
    wifi_cold_boot_soft_connects_left_ = 0;
    wifi_cold_boot_targeted_connect_active_ = false;
}

bool AuraNetworkManager::resolveStaConnectTarget(int32_t &channel_out,
                                                 uint8_t bssid_out[6],
                                                 int32_t &rssi_out) {
    channel_out = 0;
    rssi_out = -128;
    if (!bssid_out) {
        return false;
    }
    memset(bssid_out, 0, 6);

    WiFi.scanDelete();
    const int found = WiFi.scanNetworks(false, false);
    if (found <= 0) {
        WiFi.scanDelete();
        return false;
    }

    int best_index = -1;
    int32_t best_rssi = INT32_MIN;
    for (int i = 0; i < found; ++i) {
        if (WiFi.SSID(i) != wifi_ssid_) {
            continue;
        }
        const int32_t candidate_rssi = WiFi.RSSI(i);
        if (best_index < 0 || candidate_rssi > best_rssi) {
            best_index = i;
            best_rssi = candidate_rssi;
        }
    }

    if (best_index < 0) {
        WiFi.scanDelete();
        return false;
    }

    const uint8_t *scan_bssid = WiFi.BSSID(best_index);
    if (!scan_bssid) {
        WiFi.scanDelete();
        return false;
    }

    memcpy(bssid_out, scan_bssid, 6);
    channel_out = WiFi.channel(best_index);
    rssi_out = WiFi.RSSI(best_index);
    WiFi.scanDelete();
    return channel_out > 0;
}

void AuraNetworkManager::scheduleStaRetry(const char *log_reason, bool warn) {
    resetStaConnectAttemptState();
    WiFi.disconnect(false, false);
    if (wifi_retry_count_ < Config::WIFI_CONNECT_MAX_RETRIES) {
        wifi_retry_count_++;
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        wifi_state_ = WIFI_STATE_OFF;
        wifi_ui_dirty_ = true;
        Logger::log(warn ? Logger::Warn : Logger::Info, "WiFi",
                    "%s, retry %u/%u",
                    (log_reason && log_reason[0] != '\0') ? log_reason : "connect failed",
                    static_cast<unsigned>(wifi_retry_count_),
                    static_cast<unsigned>(Config::WIFI_CONNECT_MAX_RETRIES));
    } else {
        Logger::log(warn ? Logger::Warn : Logger::Info, "WiFi",
                    "%s, enter error state",
                    (log_reason && log_reason[0] != '\0') ? log_reason : "connect failed");
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_at_ms_ = 0;
        wifi_ui_dirty_ = true;
    }
}

void AuraNetworkManager::noteStaConnectTransientFailure(uint32_t reason) {
    if (wifi_state_ != WIFI_STATE_STA_CONNECTING) {
        return;
    }
    const uint8_t current = wifi_connect_transient_failures_.load(std::memory_order_acquire);
    if (current < UINT8_MAX) {
        wifi_connect_transient_failures_.store(current + 1, std::memory_order_release);
    }
    wifi_connect_last_transient_reason_.store(reason, std::memory_order_release);
}

String AuraNetworkManager::localUrl(const char *path) const {
    String url = "http://";
    if (hostname_.isEmpty()) {
        url += "aura";
    } else {
        url += hostname_;
    }
    url += ".local";
    if (path && path[0] != '\0') {
        if (path[0] != '/') {
            url += '/';
        }
        url += path;
    }
    return url;
}

void AuraNetworkManager::setStateChangeCallback(StateChangeCallback cb, void *ctx) {
    state_change_cb_ = cb;
    state_change_ctx_ = ctx;
}

void AuraNetworkManager::notifyStateChangeIfNeeded() {
    if (wifi_state_ == wifi_state_last_) {
        return;
    }
    Logger::log(Logger::Info, "WiFi",
                "state changed: %d -> %d (connected=%s)",
                static_cast<int>(wifi_state_last_),
                static_cast<int>(wifi_state_),
                isConnected() ? "YES" : "NO");
    if (state_change_cb_) {
        state_change_cb_(wifi_state_last_, wifi_state_, isConnected(), state_change_ctx_);
    }
    wifi_state_last_ = wifi_state_;
}

bool AuraNetworkManager::setEnabled(bool enabled) {
    if (enabled == wifi_enabled_) {
        return false;
    }
    wifi_enabled_ = enabled;
    wifi_enabled_dirty_ = true;
    wifi_ui_dirty_ = true;
    if (storage_) {
        storage_->saveWiFiEnabled(wifi_enabled_);
        wifi_enabled_dirty_ = false;
    }
    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            startSta();
        } else {
            startAp();
        }
    } else {
        stopMdns();
        stopAp();
        WiFi.scanDelete();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_count_ = 0;
        wifi_retry_at_ms_ = 0;
        wifi_connect_start_ms_ = 0;
        wifi_connected_since_ms_ = 0;
        resetStaConnectAttemptState();
        resetColdBootStaAssist();
    }
    return true;
}

bool AuraNetworkManager::applyEnabledIfDirty() {
    if (!wifi_enabled_dirty_) {
        return false;
    }
    if (storage_) {
        storage_->saveWiFiEnabled(wifi_enabled_);
    }
    wifi_enabled_dirty_ = false;
    wifi_ui_dirty_ = true;
    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            startSta();
        } else {
            startAp();
        }
    } else {
        stopMdns();
        stopAp();
        WiFi.scanDelete();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_count_ = 0;
        wifi_retry_at_ms_ = 0;
        wifi_connect_start_ms_ = 0;
        wifi_connected_since_ms_ = 0;
        resetStaConnectAttemptState();
        resetColdBootStaAssist();
    }
    return true;
}

void AuraNetworkManager::applySavedWiFiSettings(const String &ssid, const String &pass, bool enabled) {
    wifi_ssid_ = ssid;
    wifi_pass_ = pass;
    wifi_enabled_ = enabled;
    wifi_enabled_dirty_ = false;
    wifi_retry_count_ = 0;
    wifi_retry_at_ms_ = 0;
    wifi_connect_start_ms_ = 0;
    wifi_connected_since_ms_ = 0;
    resetStaConnectAttemptState();
    resetColdBootStaAssist();
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::clearCredentials() {
    if (storage_) {
        storage_->clearWiFiCredentials();
    }
    wifi_ssid_ = "";
    wifi_pass_ = "";
    wifi_retry_count_ = 0;
    wifi_retry_at_ms_ = 0;
    wifi_connect_start_ms_ = 0;
    wifi_connected_since_ms_ = 0;
    resetStaConnectAttemptState();
    resetColdBootStaAssist();
    wifi_scan_options_.clear();
    wifi_scan_in_progress_ = false;
    stopMdns();
    stopAp();
    WiFi.scanDelete();
    WiFi.disconnect(true, true);
    if (wifi_enabled_) {
        startAp();
    } else {
        WiFi.mode(WIFI_OFF);
        wifi_state_ = WIFI_STATE_OFF;
    }
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::startScan() {
    if (WebHandlersIsOtaBusy()) {
        return;
    }
    if (wifi_scan_in_progress_) {
        return;
    }
    WiFi.scanDelete();
    int ret = WiFi.scanNetworks(true);
    if (ret == WIFI_SCAN_RUNNING) {
        wifi_scan_in_progress_ = true;
        wifi_scan_started_ms_ = millis();
    } else if (ret >= 0) {
        wifi_build_scan_items(ret);
        WiFi.scanDelete();
        wifi_scan_in_progress_ = false;
        wifi_scan_started_ms_ = 0;
    } else {
        wifi_scan_in_progress_ = false;
        wifi_scan_started_ms_ = 0;
        LOGW("WiFi", "scan start failed: %d", ret);
    }
}

void AuraNetworkManager::stopScan() {
    if (!wifi_scan_in_progress_) {
        return;
    }
    WiFi.scanDelete();
    wifi_scan_in_progress_ = false;
    wifi_scan_started_ms_ = 0;
}

void AuraNetworkManager::connectSta() {
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 1;
    startSta();
    wifi_retry_count_ = 0;
}

void AuraNetworkManager::startApOnDemand() {
    if (!wifi_enabled_) {
        wifi_enabled_ = true;
        wifi_enabled_dirty_ = true;
        if (storage_) {
            storage_->saveWiFiEnabled(wifi_enabled_);
            wifi_enabled_dirty_ = false;
        }
    }
    startAp();
}

void AuraNetworkManager::poll() {
    const bool ota_busy = WebHandlersIsOtaBusy();

    if (wifi_state_ == WIFI_STATE_STA_CONNECTING) {
        const uint8_t transient_failures =
            wifi_connect_transient_failures_.load(std::memory_order_acquire);
        const wifi_err_reason_t last_transient_reason = static_cast<wifi_err_reason_t>(
            wifi_connect_last_transient_reason_.load(std::memory_order_acquire));
        const bool early_retry_reason =
            transient_failures >= kStaConnectTransientFailureThreshold ||
            (transient_failures >= 1 && last_transient_reason == WIFI_REASON_NO_AP_FOUND);

        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            wifi_state_ = WIFI_STATE_STA_CONNECTED;
            sta_link_fail_streak_ = 0;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            wifi_connected_since_ms_ = millis();
            resetStaConnectAttemptState();
            resetColdBootStaAssist();
            wifi_ui_dirty_ = true;
            startMdns();
            Logger::log(Logger::Info, "WiFi",
                        "connected, IP: %s",
                        WiFi.localIP().toString().c_str());
            {
                char msg[48];
                snprintf(msg, sizeof(msg), "WiFi connected, IP: %s",
                         WiFi.localIP().toString().c_str());
                MqttPublishSystemEvent("WIFI", "info", msg);
            }
        } else if (early_retry_reason) {
            scheduleStaRetry("connect transient failed early", false);
        } else if (st == WL_CONNECT_FAILED ||
                   (millis() - wifi_connect_start_ms_ > Config::WIFI_CONNECT_TIMEOUT_MS)) {
            scheduleStaRetry("connect failed");
        }
    } else if (wifi_state_ == WIFI_STATE_OFF && wifi_enabled_ && wifi_retry_at_ms_ != 0) {
        if (millis() >= wifi_retry_at_ms_) {
            wifi_retry_at_ms_ = 0;
            startSta();
        }
    }

    if (wifi_state_ == WIFI_STATE_AP_CONFIG) {
        if (wifi_scan_in_progress_) {
            if (ota_busy) {
                stopScan();
            } else {
                const uint32_t now_ms = millis();
                if (wifi_scan_started_ms_ != 0 &&
                    static_cast<uint32_t>(now_ms - wifi_scan_started_ms_) > kWifiScanTimeoutMs) {
                    LOGW("WiFi", "scan timeout after %u ms, aborting",
                         static_cast<unsigned>(kWifiScanTimeoutMs));
                    wifi_scan_options_.clear();
                    stopScan();
                }
                if (wifi_scan_in_progress_) {
                    int n = WiFi.scanComplete();
                    if (n >= 0) {
                        wifi_build_scan_items(n);
                        WiFi.scanDelete();
                        wifi_scan_in_progress_ = false;
                        wifi_scan_started_ms_ = 0;
                    } else if (n == WIFI_SCAN_FAILED) {
                        wifi_scan_options_.clear();
                        wifi_scan_in_progress_ = false;
                        wifi_scan_started_ms_ = 0;
                    }
                }
            }
        }
    } else if (wifi_state_ == WIFI_STATE_STA_CONNECTED) {
        // Periodic check of actual WiFi link while connected.
        static uint32_t last_check_ms = 0;
        static uint32_t last_ota_defer_log_ms = 0;
        uint32_t now = millis();
        if (now - last_check_ms >= 5000) {
            last_check_ms = now;
            wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) {
                sta_link_fail_streak_ = 0;
            } else {
                if (ota_busy) {
                    sta_link_fail_streak_ = 0;
                    if (now - last_ota_defer_log_ms >= 5000) {
                        last_ota_defer_log_ms = now;
                        Logger::log(Logger::Warn, "WiFi",
                                    "link status=%d while OTA is busy, defer STA server stop",
                                    static_cast<int>(st));
                    }
                } else {
                    if (sta_link_fail_streak_ < UINT8_MAX) {
                        sta_link_fail_streak_++;
                    }
                    if (sta_link_fail_streak_ < kStaLinkFailThreshold) {
                        Logger::log(Logger::Warn, "WiFi",
                                    "transient link status=%d (%u/%u), keep server alive",
                                    static_cast<int>(st),
                                    static_cast<unsigned>(sta_link_fail_streak_),
                                    static_cast<unsigned>(kStaLinkFailThreshold));
                    }
                    if (sta_link_fail_streak_ >= kStaLinkFailThreshold) {
                        const char *rssi_text = "n/a";
                        Logger::log(Logger::Warn, "WiFi",
                                    "connection lost detected (status=%d after %u checks, was connected for %u seconds, RSSI was %s)",
                                    static_cast<int>(st),
                                    static_cast<unsigned>(sta_link_fail_streak_),
                                    static_cast<unsigned>(wifi_connected_since_ms_ == 0 ? 0 : (now - wifi_connected_since_ms_) / 1000),
                                    rssi_text);
                        stopMdns();
                        wifi_state_ = WIFI_STATE_OFF;
                        wifi_connected_since_ms_ = 0;
                        sta_link_fail_streak_ = 0;
                        wifi_retry_at_ms_ = now + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                        wifi_retry_count_ = 0;
                        wifi_ui_dirty_ = true;
                    }
                }
            }
        }
    }
    WebHandlersPollDeferred();
    notifyStateChangeIfNeeded();
}

void AuraNetworkManager::warmupIfDisabled() {
    if (wifi_enabled_) {
        return;
    }
    LOGD("WiFi", "warmup (disabled mode)");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    delay(50);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void AuraNetworkManager::startSta() {
    if (wifi_ssid_.isEmpty()) {
        return;
    }

    uint32_t internal_free = 0;
    uint32_t internal_largest = 0;
    if (!has_internal_heap_for_wifi_start(internal_free, internal_largest)) {
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        wifi_ui_dirty_ = true;
        Logger::log(Logger::Warn, "WiFi",
                    "defer STA start: low internal heap free=%u largest=%u (need free>=%u largest>=%u)",
                    internal_free,
                    internal_largest,
                    static_cast<unsigned>(kWifiInternalHeapMinFreeForStart),
                    static_cast<unsigned>(kWifiInternalHeapMinLargestForStart));
        return;
    }

    stopMdns();
    stopAp();
    WiFi.persistent(false);

    if (wifi_cold_boot_warmup_pending_) {
        if (!wifi_cold_boot_warmup_active_) {
            LOGI("WiFi", "starting cold-boot STA warmup %u ms",
                 static_cast<unsigned>(kWifiColdBootWarmupMs));
            resetStaConnectAttemptState();
            WiFi.mode(WIFI_OFF);
            if (!wait_for_sta_stopped(kWifiStaTransitionTimeoutMs)) {
                LOGW("WiFi", "STA stop timeout before warmup");
            }
            if (!WiFi.mode(WIFI_STA)) {
                LOGW("WiFi", "failed to enter STA mode for warmup, retrying");
                wifi_state_ = WIFI_STATE_OFF;
                wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_ui_dirty_ = true;
                return;
            }
            if (!wait_for_sta_started(kWifiStaTransitionTimeoutMs)) {
                LOGW("WiFi", "STA did not report started for warmup, retrying");
                wifi_state_ = WIFI_STATE_OFF;
                wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_ui_dirty_ = true;
                return;
            }
            WiFi.setAutoReconnect(false);
            delay(kWifiStaStartSettleMs);
            wifi_cold_boot_warmup_active_ = true;
            wifi_retry_at_ms_ = millis() + kWifiColdBootWarmupMs;
            wifi_ui_dirty_ = true;
            return;
        }

        LOGI("WiFi", "cold-boot STA warmup complete");
        wifi_cold_boot_warmup_pending_ = false;
        wifi_cold_boot_warmup_active_ = false;
        wifi_cold_boot_soft_connects_left_ = kWifiColdBootSoftConnectAttempts;
    }

    const bool use_soft_cold_boot_connect = (wifi_cold_boot_soft_connects_left_ > 0);
    if (use_soft_cold_boot_connect) {
        wifi_cold_boot_soft_connects_left_--;
        LOGI("WiFi", "using soft STA connect after warmup (%u left)",
             static_cast<unsigned>(wifi_cold_boot_soft_connects_left_));
    } else {
        // A clean STA reset avoids stale auth state on the first post-boot connect too.
        LOGI("WiFi", "forcing STA reset before connect");
        resetStaConnectAttemptState();
        WiFi.mode(WIFI_OFF);
        if (!wait_for_sta_stopped(kWifiStaTransitionTimeoutMs)) {
            LOGW("WiFi", "STA stop timeout after forced reset");
        }
    }
    wifi_mode_t mode = WiFi.getMode();
    if ((mode & WIFI_STA) == 0) {
        if (!WiFi.mode(WIFI_STA)) {
            LOGW("WiFi", "failed to enter STA mode, retrying");
            wifi_state_ = WIFI_STATE_OFF;
            wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
            wifi_ui_dirty_ = true;
            return;
        }
    }
    if (!wait_for_sta_started(kWifiStaTransitionTimeoutMs)) {
        LOGW("WiFi", "STA did not report started before connect, retrying");
        wifi_state_ = WIFI_STATE_OFF;
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        wifi_ui_dirty_ = true;
        return;
    }
    // Rely on our own retry logic instead of Arduino's internal reconnect churn.
    WiFi.setAutoReconnect(false);
    // Give the STA interface a brief settle window after mode transition.
    delay(kWifiStaStartSettleMs);
    // Clear any previous association state before a fresh begin(), but keep config intact.
    WiFi.disconnect(false, false);
    delay(50);
    if (!hostname_.isEmpty() && !WiFi.setHostname(hostname_.c_str())) {
        LOGW("WiFi", "setHostname failed");
    }

    bool targeted_connect = false;
    if (wifi_cold_boot_targeted_connect_active_) {
        wifi_cold_boot_targeted_connect_active_ = false;
        int32_t target_channel = 0;
        int32_t target_rssi = -128;
        uint8_t target_bssid[6] = {};
        if (resolveStaConnectTarget(target_channel, target_bssid, target_rssi)) {
            char bssid_text[18];
            format_wifi_event_bssid(target_bssid, bssid_text, sizeof(bssid_text));
            Logger::log(Logger::Info, "WiFi",
                        "targeted connect prepared (channel=%d, rssi=%d dBm, bssid=%s)",
                        static_cast<int>(target_channel),
                        static_cast<int>(target_rssi),
                        bssid_text);
            WiFi.begin(wifi_ssid_.c_str(), wifi_pass_.c_str(), target_channel, target_bssid, true);
            targeted_connect = true;
        } else {
            LOGI("WiFi", "targeted connect fallback: SSID not found in scan");
        }
    }
    if (!targeted_connect) {
        WiFi.begin(wifi_ssid_.c_str(), wifi_pass_.c_str());
    }
    startServerIfNeeded();
    wifi_state_ = WIFI_STATE_STA_CONNECTING;
    wifi_connect_start_ms_ = millis();
    wifi_connected_since_ms_ = 0;
    wifi_ui_dirty_ = true;
    String safe_ssid = wifi_label_safe(wifi_ssid_);
    Logger::log(Logger::Info, "WiFi",
                "connecting to: %s",
                safe_ssid.c_str());
}

void AuraNetworkManager::startAp() {
    stopMdns();
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    const char *ap_ssid = ap_ssid_.isEmpty() ? Config::WIFI_AP_SSID : ap_ssid_.c_str();
    if (!WiFi.softAP(ap_ssid)) {
        LOGW("WiFi", "failed to start AP: %s", ap_ssid);
        // [NEW] AP mode failed to start.
        MqttPublishSystemEvent("WIFI", "warning", "WiFi AP failed to start");
        if (WiFi.status() == WL_CONNECTED) {
            wifi_state_ = WIFI_STATE_STA_CONNECTED;
            sta_link_fail_streak_ = 0;
        } else {
            wifi_state_ = WIFI_STATE_OFF;
            sta_link_fail_streak_ = 0;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            resetStaConnectAttemptState();
            resetColdBootStaAssist();
        }
        wifi_ui_dirty_ = true;
        return;
    }
    startServerIfNeeded();
    IPAddress ip = WiFi.softAPIP();
    startScan();
    wifi_state_ = WIFI_STATE_AP_CONFIG;
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 0;
    resetStaConnectAttemptState();
    resetColdBootStaAssist();
    wifi_ui_dirty_ = true;
    Logger::log(Logger::Info, "WiFi", "AP started: %s", ap_ssid);
    Logger::log(Logger::Info, "WiFi", "AP IP: %s", ip.toString().c_str());
}

void AuraNetworkManager::stopAp() {
    if (wifi_state_ == WIFI_STATE_AP_CONFIG) {
        WiFi.enableAP(false);
    }
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::startMdns() {
    if (mdns_started_) {
        return;
    }
    if (MDNS.begin(hostname_.c_str())) {
        Logger::log(Logger::Info, "mDNS",
                    "responder started: %s.local",
                    hostname_.c_str());
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "mDNS responder started: %s.local",
                     hostname_.c_str());
            MqttPublishSystemEvent("WIFI", "info", msg);
        }
        MDNS.addService("http", "tcp", 80);
        mdns_started_ = true;
    } else {
        LOGW("mDNS", "start failed");
        // [NEW] mDNS responder failed to start.
        MqttPublishSystemEvent("WIFI", "warning", "mDNS start failed");
    }
}

void AuraNetworkManager::stopMdns() {
    if (!mdns_started_) {
        return;
    }
    MDNS.end();
    mdns_started_ = false;
}
