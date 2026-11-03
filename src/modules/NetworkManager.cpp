// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/NetworkManager.h"
#include "core/WakePowerGuard.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#endif
#include "core/BootState.h"
#include "core/WifiPowerSaveGuard.h"
#include "web/WebWifiScanUtils.h"
#include "core/NetworkCommandQueue.h"
#include "core/Logger.h"
#include "core/NetworkIdentity.h"
#include "config/AppConfig.h"
#include "ui/ThemeManager.h"
#include "web/WebHandlers.h"
#include "web/WebInputValidation.h"
#include "web/WebRouteRegistry.h"
#include "web/WebRuntime.h"
#include "web/WebUiBridge.h"

namespace {

AuraNetworkManager *g_network = nullptr;
NetworkCommandQueue *g_network_command_queue = nullptr;
wifi_event_id_t g_wifi_event_handle = 0;
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
constexpr uint32_t kWifiRecoveryRetryDelayMs = 30000UL;
constexpr uint32_t kHostnameCheckIntervalMs = 5000UL;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
constexpr wifi_err_reason_t kWifiInactiveReason = WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY;
#else
constexpr wifi_err_reason_t kWifiInactiveReason = WIFI_REASON_ASSOC_EXPIRE;
#endif
static_assert(kWifiInactiveReason == 4, "Preserve the existing inactivity retry policy");

bool is_retryable_connect_reason(wifi_err_reason_t reason) {
    return reason == WIFI_REASON_AUTH_EXPIRE ||
           reason == WIFI_REASON_AUTH_LEAVE ||
           reason == WIFI_REASON_NO_AP_FOUND ||
           reason == kWifiInactiveReason;
}

bool has_internal_heap_for_wifi_start(uint32_t &free_bytes, uint32_t &largest_block_bytes) {
    free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    largest_block_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return (free_bytes >= kWifiInternalHeapMinFreeForStart) &&
           (largest_block_bytes >= kWifiInternalHeapMinLargestForStart);
}

int32_t set_netif_hostname(void *context, const char *hostname) {
    return esp_netif_set_hostname(static_cast<esp_netif_t *>(context), hostname);
}

int32_t get_netif_hostname(void *context, const char **hostname) {
    return esp_netif_get_hostname(static_cast<esp_netif_t *>(context), hostname);
}

void clear_sta_enterprise_state() {
#if __has_include("esp_eap_client.h")
    esp_wifi_sta_enterprise_disable();
    esp_eap_client_clear_identity();
    esp_eap_client_clear_username();
    esp_eap_client_clear_password();
    esp_eap_client_clear_ca_cert();
    esp_eap_client_clear_certificate_and_key();
#endif
}

wpa2_auth_method_t to_wpa2_auth_method(Config::WifiEapMethod method) {
    switch (method) {
        case Config::WifiEapMethod::Ttls:
            return WPA2_AUTH_TTLS;
        case Config::WifiEapMethod::Tls:
            return WPA2_AUTH_TLS;
        case Config::WifiEapMethod::Peap:
        default:
            return WPA2_AUTH_PEAP;
    }
}

int to_ttls_phase2_type(Config::WifiTtlsPhase2 phase2) {
#if __has_include("esp_eap_client.h")
    switch (phase2) {
        case Config::WifiTtlsPhase2::Pap:
            return ESP_EAP_TTLS_PHASE2_PAP;
        case Config::WifiTtlsPhase2::Chap:
            return ESP_EAP_TTLS_PHASE2_CHAP;
        case Config::WifiTtlsPhase2::Mschap:
            return ESP_EAP_TTLS_PHASE2_MSCHAP;
        case Config::WifiTtlsPhase2::Eap:
            return ESP_EAP_TTLS_PHASE2_EAP;
        case Config::WifiTtlsPhase2::Mschapv2:
        default:
            return ESP_EAP_TTLS_PHASE2_MSCHAPV2;
    }
#else
    (void)phase2;
    return -1;
#endif
}

uint32_t mac_suffix_24bit() {
    uint8_t mac[6] = {};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        return 0;
    }
    return NetworkIdentity::macSuffix24(mac);
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
    if (g_network && (event == ARDUINO_EVENT_WIFI_STA_START ||
                      event == ARDUINO_EVENT_WIFI_STA_STOP ||
                      event == ARDUINO_EVENT_WIFI_STA_GOT_IP)) {
        // Never read/mutate a netif in the Arduino callback task. GOT_IP also
        // covers connections completed outside Aura's normal state transition.
        g_network->requestHostnameCheck();
    }
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

bool network_wifi_start_scan() {
    if (g_network_command_queue) {
        if (g_network_command_queue->enqueue(NetworkCommandQueue::Type::RequestWifiScanStart) ||
            g_network_command_queue->publishWifiScanRequest(
                NetworkCommandQueue::Type::RequestWifiScanStart)) {
            return true;
        }
    }
    LOGW("WiFi", "scan start request dropped: command queue unavailable");
    return false;
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

bool network_wifi_prepare_sta() {
    return g_network && g_network->prepareWifiStartSta();
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

void apply_sta_default_power_save(const char *context) {
    const wifi_mode_t mode = WiFi.getMode();
    if ((mode & WIFI_MODE_STA) == 0) {
        return;
    }

    if (!WifiPowerSaveGuard::disableByDefault()) {
        Logger::log(Logger::Warn, "WiFi", "failed to set default power-save mode during %s",
                    context ? context : "unknown");
    }
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
    if (g_wifi_event_handle == 0) {
        g_wifi_event_handle = WiFi.onEvent(network_wifi_event);
    }
    hostname_ = build_wifi_hostname();
    if (hostname_.isEmpty()) {
        hostname_ = "aura";
    }
    // Arduino-ESP32 copies this staged hostname into the STA esp-netif when
    // WIFI_STA is enabled. Calling WiFi.setHostname() after WiFi.mode() only
    // changes the value used for a future interface start, leaving DHCP option
    // 12 on the current interface at the default "esp32s3-*" hostname.
    if (!WiFi.setHostname(hostname_.c_str())) {
        LOGW("WiFi", "failed to stage hostname before interface start");
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
    web_ctx_.wifi_prepare_sta = network_wifi_prepare_sta;
    web_ctx_.wifi_start_sta = network_wifi_start_sta;
    WebHandlersInit(&web_ctx_);
    registerServerRoutes();

    Config::WifiSettings stored_wifi_settings{};
    storage_->loadWiFiSettings(stored_wifi_settings);
    applyRuntimeWiFiSettings(stored_wifi_settings);
    wifi_enabled_dirty_ = false;
    if (!wifi_ssid_.isEmpty() &&
        !WebInputValidation::isWifiSsidValid(wifi_ssid_, WebInputValidation::kWifiSsidMaxBytes)) {
        LOGW("WiFi", "SSID invalid length, clearing saved credentials");
        storage_->clearWiFiCredentials();
        wifi_ssid_ = "";
        wifi_pass_ = "";
        wifi_auth_mode_ = Config::WifiAuthMode::Personal;
        wifi_eap_method_ = Config::WifiEapMethod::Peap;
        wifi_ttls_phase2_ = Config::WifiTtlsPhase2::Mschapv2;
        wifi_identity_ = "";
        wifi_username_ = "";
        wifi_enterprise_pass_ = "";
        wifi_eap_ca_cert_pem_ = "";
        wifi_eap_client_cert_pem_ = "";
        wifi_eap_client_key_pem_ = "";
    }

    if (wifi_enabled_) {
        if (!wifi_ssid_.isEmpty()) {
            wifi_cold_boot_warmup_pending_ = boot_board_cold_start;
            setWifiState(WIFI_STATE_OFF);
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
        network_hostname_.clearLive();
        resetColdBootStaAssist();
        setWifiState(WIFI_STATE_OFF);
    }
    wifi_state_last_ = state();
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

void AuraNetworkManager::attachDisplayThresholds(DisplayThresholdManager &displayThresholds) {
    web_ctx_.display_thresholds = &displayThresholds;
}

void AuraNetworkManager::attachSensorManager(SensorManager &sensorManager) {
    web_ctx_.sensor_manager = &sensorManager;
}

void AuraNetworkManager::attachDailyHistory(SdCardManager &sdCard, DailyExtremaHistory &dailyExtrema) {
    web_ctx_.sd_card = &sdCard;
    web_ctx_.daily_extrema = &dailyExtrema;
}

void AuraNetworkManager::attachCommandQueue(NetworkCommandQueue &commandQueue) {
    g_network_command_queue = &commandQueue;
}

void AuraNetworkManager::registerServerRoutes() {
    if (server_routes_registered_) {
        return;
    }

    WebRouteRegistry::registerRoutes(serverBackend());
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
}

void AuraNetworkManager::scheduleStaRetry(const char *log_reason, bool warn) {
    resetStaConnectAttemptState();
    WiFi.disconnect(false, false);
    if (wifi_retry_count_ < Config::WIFI_CONNECT_MAX_RETRIES) {
        wifi_retry_count_++;
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        setWifiState(WIFI_STATE_OFF);
        wifi_ui_dirty_ = true;
        Logger::log(warn ? Logger::Warn : Logger::Info, "WiFi",
                    "%s, retry %u/%u",
                    (log_reason && log_reason[0] != '\0') ? log_reason : "connect failed",
                    static_cast<unsigned>(wifi_retry_count_),
                    static_cast<unsigned>(Config::WIFI_CONNECT_MAX_RETRIES));
    } else {
        Logger::log(warn ? Logger::Warn : Logger::Info, "WiFi",
                    "%s, enter error state; background retry in %u seconds",
                    (log_reason && log_reason[0] != '\0') ? log_reason : "connect failed",
                    static_cast<unsigned>(kWifiRecoveryRetryDelayMs / 1000UL));
        setWifiState(WIFI_STATE_OFF);
        wifi_retry_count_ = Config::WIFI_CONNECT_MAX_RETRIES;
        wifi_retry_at_ms_ = millis() + kWifiRecoveryRetryDelayMs;
        wifi_ui_dirty_ = true;
    }
}

void AuraNetworkManager::noteStaConnectTransientFailure(uint32_t reason) {
    if (state() != WIFI_STATE_STA_CONNECTING) {
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
    const WifiState current_state = state();
    if (current_state == wifi_state_last_) {
        return;
    }
    Logger::log(Logger::Info, "WiFi",
                "state changed: %d -> %d (connected=%s)",
                static_cast<int>(wifi_state_last_),
                static_cast<int>(current_state),
                current_state == WIFI_STATE_STA_CONNECTED ? "YES" : "NO");
    if (state_change_cb_) {
        state_change_cb_(wifi_state_last_,
                         current_state,
                         current_state == WIFI_STATE_STA_CONNECTED,
                         state_change_ctx_);
    }
    wifi_state_last_ = current_state;
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
        shutdownWifi(false);
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
        shutdownWifi(false);
    }
    return true;
}

void AuraNetworkManager::shutdownWifi(bool erase_sdk_credentials) {
    if (wifi_scan_session_.active() || wifi_scan_transition_.pending()) {
        wifi_scan_transition_.shutdown(erase_sdk_credentials);
        stopScan();
        return;
    }
    stopMdns();
    stopAp();
    // Retain worker ownership until its blocking SDK operation has returned.
    stopScan();
    wifi_scan_options_.clear();
    WiFi.disconnect(true, erase_sdk_credentials);
    WiFi.mode(WIFI_OFF);
    network_hostname_.clearLive();
    setWifiState(WIFI_STATE_OFF);
    wifi_retry_count_ = 0;
    wifi_retry_at_ms_ = 0;
    wifi_connect_start_ms_ = 0;
    wifi_connected_since_ms_ = 0;
    sta_last_rssi_valid_ = false;
    sta_last_rssi_dbm_ = 0;
    resetStaConnectAttemptState();
    resetColdBootStaAssist();
}

void AuraNetworkManager::applyRuntimeWiFiSettings(const Config::WifiSettings &settings) {
    wifi_ssid_ = settings.ssid;
    wifi_pass_ = settings.pass;
    wifi_enabled_ = settings.enabled;
    wifi_auth_mode_ = settings.auth_mode;
    wifi_eap_method_ = settings.eap_method;
    wifi_ttls_phase2_ = settings.ttls_phase2;
    wifi_identity_ = settings.identity;
    wifi_username_ = settings.username;
    wifi_enterprise_pass_ = settings.enterprise_password;
    wifi_eap_ca_cert_pem_ = settings.ca_cert_pem;
    wifi_eap_client_cert_pem_ = settings.client_cert_pem;
    wifi_eap_client_key_pem_ = settings.client_key_pem;
}

void AuraNetworkManager::applySavedWiFiSettings(const Config::WifiSettings &settings) {
    applyRuntimeWiFiSettings(settings);
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
    wifi_auth_mode_ = Config::WifiAuthMode::Personal;
    wifi_eap_method_ = Config::WifiEapMethod::Peap;
    wifi_ttls_phase2_ = Config::WifiTtlsPhase2::Mschapv2;
    wifi_identity_ = "";
    wifi_username_ = "";
    wifi_enterprise_pass_ = "";
    wifi_eap_ca_cert_pem_ = "";
    wifi_eap_client_cert_pem_ = "";
    wifi_eap_client_key_pem_ = "";
    shutdownWifi(true);
    if (wifi_enabled_) {
        startAp();
    }
    wifi_ui_dirty_ = true;
}

void AuraNetworkManager::buildScanOptions(int count) {
    WebWifiScanUtils::WifiScanRow rows[WifiScanSession::kMaxRecords];
    size_t used = 0;
    for (int i = 0; i < count; ++i) {
        const auto *row = wifi_scan_session_.record(static_cast<size_t>(i));
        if (!row) continue;
        const bool enterprise = row->auth_mode == WIFI_AUTH_WPA2_ENTERPRISE ||
                                row->auth_mode == WIFI_AUTH_WPA3_ENT_192;
        WebWifiScanUtils::addOrReplaceBestNetwork(rows, used, WifiScanSession::kMaxRecords,
            String(row->ssid), row->rssi, row->auth_mode == WIFI_AUTH_OPEN, enterprise);
    }
    WebWifiScanUtils::sortNetworksByRssiDesc(rows, used);
    wifi_scan_options_ = WebWifiScanUtils::renderNetworkItemsHtml(rows, used);
}

void AuraNetworkManager::startScan() {
    if (WebHandlersIsOtaBusy()) return;
    if (wifi_scan_session_.active()) {
        if (wifi_scan_session_.cancelling()) wifi_scan_restart_pending_ = true;
        return;
    }
    const int ret = wifi_scan_session_.start();
    wifi_scan_in_progress_ = wifi_scan_session_.active();
    wifi_scan_started_ms_ = wifi_scan_in_progress_ ? millis() : 0;
    if (ret >= 0) {
        buildScanOptions(ret);
        wifi_scan_session_.releaseResults();
    } else if (ret != WIFI_SCAN_RUNNING) {
        LOGW("WiFi", "scan start failed: %d", ret);
    }
}

void AuraNetworkManager::stopScan() {
    wifi_scan_restart_pending_ = false;
    wifi_scan_session_.cancel();
    wifi_scan_in_progress_ = wifi_scan_session_.active();
    if (!wifi_scan_in_progress_) wifi_scan_started_ms_ = 0;
}

bool AuraNetworkManager::prepareWifiStartSta() {
    // Called only by WebHandlersPollDeferred on the network owner, with an
    // admitted activity lease. A successful stop request is not completion.
    wifi_scan_restart_pending_ = false;
    if (wifi_scan_session_.active() && !wifi_scan_session_.cancelling()) stopScan();
    return !wifi_scan_session_.active();
}

void AuraNetworkManager::connectSta() {
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 0;
    startSta();
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

void AuraNetworkManager::serviceWakeQuiescence() {
    if (!wifi_scan_session_.active() || !WakePowerGuard::backgroundPaused(millis())) return;
    if (!wifi_scan_session_.cancelling() && !wifi_scan_transition_.pending() &&
        wifi_enabled_ && state() == WIFI_STATE_AP_CONFIG)
        wifi_scan_restart_pending_ = true;
    wifi_scan_session_.serviceWakeQuiescence();
}

void AuraNetworkManager::poll() {
    // The network owner already holds admission. No DNS I/O runs in the
    // protected display phases or on an independent UDP callback task.
    if (state() == WIFI_STATE_AP_CONFIG) {
        const uint32_t now = millis();
        if (!captive_dns_.running() && now - captive_dns_last_attempt_ms_ >= 5000) {
            captive_dns_last_attempt_ms_ = now;
            captive_dns_.start(static_cast<uint32_t>(WiFi.softAPIP()));
        }
        captive_dns_.poll();
    } else {
        captive_dns_.stop();
    }
    const bool ota_busy = WebHandlersIsOtaBusy();
    const uint32_t hostname_now_ms = millis();
    if (hostname_check_pending_.exchange(false, std::memory_order_acq_rel) ||
        hostname_now_ms - hostname_last_poll_ms_ >= kHostnameCheckIntervalMs) {
        observeStaHostname();
        hostname_last_poll_ms_ = hostname_now_ms;
    }

    if (state() == WIFI_STATE_STA_CONNECTING) {
        const uint8_t transient_failures =
            wifi_connect_transient_failures_.load(std::memory_order_acquire);
        const wifi_err_reason_t last_transient_reason = static_cast<wifi_err_reason_t>(
            wifi_connect_last_transient_reason_.load(std::memory_order_acquire));
        const bool early_retry_reason =
            transient_failures >= kStaConnectTransientFailureThreshold ||
            (transient_failures >= 1 && last_transient_reason == WIFI_REASON_NO_AP_FOUND);

        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            observeStaHostname();
            apply_sta_default_power_save("sta connected");
            setWifiState(WIFI_STATE_STA_CONNECTED);
            sta_link_fail_streak_ = 0;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            wifi_connected_since_ms_ = millis();
            sta_last_rssi_dbm_ = WiFi.RSSI();
            sta_last_rssi_valid_ = true;
            resetStaConnectAttemptState();
            resetColdBootStaAssist();
            wifi_ui_dirty_ = true;
            startMdns();
            Logger::log(Logger::Info, "WiFi",
                        "connected, IP: %s",
                        WiFi.localIP().toString().c_str());
        } else if (early_retry_reason) {
            scheduleStaRetry("connect transient failed early", false);
        } else if (st == WL_CONNECT_FAILED ||
                   (millis() - wifi_connect_start_ms_ > Config::WIFI_CONNECT_TIMEOUT_MS)) {
            scheduleStaRetry("connect failed");
        }
    } else if (state() == WIFI_STATE_OFF && wifi_enabled_ && wifi_retry_at_ms_ != 0) {
        const uint32_t now_ms = millis();
        if (static_cast<int32_t>(now_ms - wifi_retry_at_ms_) >= 0) {
            wifi_retry_at_ms_ = 0;
            startSta();
        }
    }

    // Poll the independent scan worker even after leaving AP mode. STA starts
    // after the old driver operation ends, without waiting for Arduino events.
    if (wifi_scan_in_progress_) {
        const bool timed_out = static_cast<uint32_t>(millis() - wifi_scan_started_ms_) > kWifiScanTimeoutMs;
        if ((ota_busy || timed_out) && !wifi_scan_session_.cancelling()) {
            if (timed_out) LOGW("WiFi", "scan timeout; waiting for driver cancellation");
            stopScan();
        }
        int count = 0;
        const auto result = wifi_scan_session_.poll(count);
        if (result == WifiScanSession::Result::Ready) {
            buildScanOptions(count);
            wifi_scan_session_.releaseResults();
        } else if (result == WifiScanSession::Result::Failed) {
            wifi_scan_options_.clear();
        }
        wifi_scan_in_progress_ = wifi_scan_session_.active();
        if (!wifi_scan_in_progress_) {
            wifi_scan_started_ms_ = 0;
        }
    }

    const auto transition = wifi_scan_transition_.take(wifi_scan_session_.active());
    if (transition) {
        wifi_scan_restart_pending_ = false;
        if (transition.shutdown) shutdownWifi(transition.erase_credentials);
        if (wifi_enabled_) {
            if (transition.start == WifiScanTransition::Start::Sta) startSta();
            else if (transition.start == WifiScanTransition::Start::Ap) startAp();
        }
    } else if (!wifi_scan_session_.active() && wifi_scan_restart_pending_ &&
               wifi_enabled_ && state() == WIFI_STATE_AP_CONFIG && !ota_busy) {
        wifi_scan_restart_pending_ = false;
        startScan();
    }

    if (state() == WIFI_STATE_STA_CONNECTED) {
        // Periodic check of actual WiFi link while connected.
        static uint32_t last_check_ms = 0;
        static uint32_t last_ota_defer_log_ms = 0;
        uint32_t now = millis();
        if (now - last_check_ms >= 5000) {
            last_check_ms = now;
            wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) {
                sta_link_fail_streak_ = 0;
                sta_last_rssi_dbm_ = WiFi.RSSI();
                sta_last_rssi_valid_ = true;
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
                        char rssi_text[16] = "n/a";
                        if (sta_last_rssi_valid_) {
                            snprintf(rssi_text,
                                     sizeof(rssi_text),
                                     "%d dBm",
                                     static_cast<int>(sta_last_rssi_dbm_));
                        }
                        Logger::log(Logger::Warn, "WiFi",
                                    "connection lost detected (status=%d after %u checks, was connected for %u seconds, RSSI was %s)",
                                    static_cast<int>(st),
                                    static_cast<unsigned>(sta_link_fail_streak_),
                                    static_cast<unsigned>(wifi_connected_since_ms_ == 0 ? 0 : (now - wifi_connected_since_ms_) / 1000),
                                    rssi_text);
                        stopMdns();
                        setWifiState(WIFI_STATE_OFF);
                        wifi_connected_since_ms_ = 0;
                        sta_last_rssi_valid_ = false;
                        sta_last_rssi_dbm_ = 0;
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
    network_hostname_.clearLive();
}

bool AuraNetworkManager::enterpriseSettingsReadyForConnect() const {
    if (wifi_auth_mode_ != Config::WifiAuthMode::Enterprise) {
        return true;
    }
    if (wifi_eap_method_ == Config::WifiEapMethod::Peap ||
        wifi_eap_method_ == Config::WifiEapMethod::Ttls) {
        return !wifi_identity_.isEmpty() &&
               !wifi_username_.isEmpty() &&
               !wifi_enterprise_pass_.isEmpty();
    }
    return !wifi_identity_.isEmpty() &&
           !wifi_eap_client_cert_pem_.isEmpty() &&
           !wifi_eap_client_key_pem_.isEmpty();
}

void AuraNetworkManager::beginStaConnect(int32_t channel, const uint8_t *bssid) {
    if (wifi_auth_mode_ != Config::WifiAuthMode::Enterprise) {
        clear_sta_enterprise_state();
        WiFi.begin(wifi_ssid_.c_str(), wifi_pass_.c_str(), channel, bssid, true);
        return;
    }

    clear_sta_enterprise_state();
#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
    const wpa2_auth_method_t method = to_wpa2_auth_method(wifi_eap_method_);
    const int phase2 = wifi_eap_method_ == Config::WifiEapMethod::Ttls
                           ? to_ttls_phase2_type(wifi_ttls_phase2_)
                           : -1;
    const char *ca_pem = wifi_eap_ca_cert_pem_.isEmpty()
                             ? nullptr
                             : wifi_eap_ca_cert_pem_.c_str();
    const char *client_crt = wifi_eap_client_cert_pem_.isEmpty()
                                 ? nullptr
                                 : wifi_eap_client_cert_pem_.c_str();
    const char *client_key = wifi_eap_client_key_pem_.isEmpty()
                                 ? nullptr
                                 : wifi_eap_client_key_pem_.c_str();
    WiFi.begin(wifi_ssid_.c_str(),
               method,
               wifi_identity_.c_str(),
               wifi_username_.c_str(),
               wifi_enterprise_pass_.c_str(),
               ca_pem,
               client_crt,
               client_key,
               phase2,
               channel,
               bssid,
               true);
#else
    (void)channel;
    (void)bssid;
    LOGW("WiFi", "enterprise support is not enabled in SDK config");
#endif
}

bool AuraNetworkManager::applyStaHostname() {
    const bool ok = network_hostname_.apply(hostname_.c_str(), WiFi.STA.netif(),
                                           set_netif_hostname, get_netif_hostname, millis());
    const auto &status = network_hostname_.snapshot();
    Logger::log(ok ? Logger::Info : Logger::Warn, "WiFi",
                "STA hostname apply: expected=%s actual=%s result=%s failures=%lu",
                hostname_.c_str(), status.available ? status.actual : "<unavailable>",
                esp_err_to_name(status.apply_error),
                static_cast<unsigned long>(status.apply_failures));
    return ok;
}

void AuraNetworkManager::observeStaHostname() {
    if ((WiFi.getMode() & WIFI_STA) == 0 || !WiFi.STA.started()) {
        network_hostname_.clearLive();
        return;
    }
    const auto previous = network_hostname_.snapshot();
    network_hostname_.observe(hostname_.c_str(), WiFi.STA.netif(), get_netif_hostname, millis());
    const auto &status = network_hostname_.snapshot();
    if (status.mismatch_count != previous.mismatch_count ||
        status.read_error != previous.read_error) {
        Logger::log(Logger::Warn, "WiFi",
                    "STA hostname check: expected=%s actual=%s read=%s mismatches=%lu",
                    hostname_.c_str(), status.available ? status.actual : "<unavailable>",
                    esp_err_to_name(status.read_error),
                    static_cast<unsigned long>(status.mismatch_count));
    }
}

void AuraNetworkManager::startSta() {
    if (wifi_ssid_.isEmpty()) {
        return;
    }
    if (!enterpriseSettingsReadyForConnect()) {
        LOGW("WiFi", "enterprise settings incomplete, starting AP config mode");
        startAp();
        return;
    }
    if (wifi_scan_session_.active() || wifi_scan_transition_.pending()) {
        wifi_scan_transition_.start(WifiScanTransition::Start::Sta);
        stopScan();
        return;
    }
    sta_last_rssi_valid_ = false;
    sta_last_rssi_dbm_ = 0;

    uint32_t internal_free = 0;
    uint32_t internal_largest = 0;
    if (!has_internal_heap_for_wifi_start(internal_free, internal_largest)) {
        setWifiState(WIFI_STATE_OFF);
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
            network_hostname_.clearLive();
            if (!wait_for_sta_stopped(kWifiStaTransitionTimeoutMs)) {
                LOGW("WiFi", "STA stop timeout before warmup");
            }
            if (!WiFi.mode(WIFI_STA)) {
                LOGW("WiFi", "failed to enter STA mode for warmup, retrying");
                setWifiState(WIFI_STATE_OFF);
                wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
                wifi_ui_dirty_ = true;
                return;
            }
            if (!wait_for_sta_started(kWifiStaTransitionTimeoutMs)) {
                LOGW("WiFi", "STA did not report started for warmup, retrying");
                setWifiState(WIFI_STATE_OFF);
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
        network_hostname_.clearLive();
        if (!wait_for_sta_stopped(kWifiStaTransitionTimeoutMs)) {
            LOGW("WiFi", "STA stop timeout after forced reset");
        }
    }
    wifi_mode_t mode = WiFi.getMode();
    if ((mode & WIFI_STA) == 0) {
        if (!WiFi.mode(WIFI_STA)) {
            LOGW("WiFi", "failed to enter STA mode, retrying");
            setWifiState(WIFI_STATE_OFF);
            wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
            wifi_ui_dirty_ = true;
            return;
        }
    }
    if (!wait_for_sta_started(kWifiStaTransitionTimeoutMs)) {
        LOGW("WiFi", "STA did not report started before connect, retrying");
        setWifiState(WIFI_STATE_OFF);
        wifi_retry_at_ms_ = millis() + Config::WIFI_CONNECT_RETRY_DELAY_MS;
        wifi_ui_dirty_ = true;
        return;
    }
    // The maintained Arduino STA overlay makes this include its first retry.
    WiFi.setAutoReconnect(false);
    // Give the STA interface a brief settle window after mode transition.
    delay(kWifiStaStartSettleMs);
    apply_sta_default_power_save("sta start");
    // Clear any previous association state before a fresh begin(), but keep config intact.
    WiFi.disconnect(false, false);
    delay(50);
    // Apply the hostname to the live STA esp-netif before WiFi.begin() starts
    // DHCP. This also covers a STA interface created earlier for cold-boot
    // warmup, where the staged value alone would not update the live netif.
    if (!applyStaHostname()) {
        scheduleStaRetry("hostname apply/verification failed");
        return;
    }

    // Use the driver's ordinary SSID selection on the first cold attempt too.
    // Avoid a blocking pre-scan and a separate BSSID-pinned connection path.
    beginStaConnect(0, nullptr);
    startServerIfNeeded();
    setWifiState(WIFI_STATE_STA_CONNECTING);
    wifi_connect_start_ms_ = millis();
    wifi_connected_since_ms_ = 0;
    wifi_ui_dirty_ = true;
    String safe_ssid = wifi_label_safe(wifi_ssid_);
    Logger::log(Logger::Info, "WiFi",
                "connecting to: %s",
                safe_ssid.c_str());
}

void AuraNetworkManager::startAp() {
    if (wifi_scan_session_.active() || wifi_scan_transition_.pending()) {
        wifi_scan_transition_.start(WifiScanTransition::Start::Ap);
        stopScan();
        return;
    }
    stopMdns();
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    const char *ap_ssid = ap_ssid_.isEmpty() ? Config::WIFI_AP_SSID : ap_ssid_.c_str();
    if (!WiFi.softAP(ap_ssid)) {
        LOGW("WiFi", "failed to start AP: %s", ap_ssid);
        if (WiFi.status() == WL_CONNECTED) {
            setWifiState(WIFI_STATE_STA_CONNECTED);
            sta_link_fail_streak_ = 0;
        } else {
            setWifiState(WIFI_STATE_OFF);
            sta_link_fail_streak_ = 0;
            wifi_retry_count_ = 0;
            wifi_retry_at_ms_ = 0;
            resetStaConnectAttemptState();
            resetColdBootStaAssist();
        }
        wifi_ui_dirty_ = true;
        return;
    }
    observeStaHostname();
    startServerIfNeeded();
    IPAddress ip = WiFi.softAPIP();
    captive_dns_last_attempt_ms_ = millis();
    if (!captive_dns_.start(static_cast<uint32_t>(ip))) {
        LOGW("WiFi", "captive DNS unavailable; retrying while AP is active");
    }
    startScan();
    setWifiState(WIFI_STATE_AP_CONFIG);
    wifi_retry_at_ms_ = 0;
    wifi_retry_count_ = 0;
    resetStaConnectAttemptState();
    resetColdBootStaAssist();
    wifi_ui_dirty_ = true;
    Logger::log(Logger::Info, "WiFi", "AP started: %s", ap_ssid);
    Logger::log(Logger::Info, "WiFi", "AP IP: %s", ip.toString().c_str());
}

void AuraNetworkManager::stopAp() {
    WebHandlersCancelWifiSave();
    captive_dns_.stop();
    if (state() == WIFI_STATE_AP_CONFIG) {
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
        MDNS.addService("http", "tcp", 80);
        mdns_started_ = true;
    } else {
        LOGW("mDNS", "start failed");
    }
}

void AuraNetworkManager::stopMdns() {
    if (!mdns_started_) {
        return;
    }
    MDNS.end();
    mdns_started_ = false;
}
