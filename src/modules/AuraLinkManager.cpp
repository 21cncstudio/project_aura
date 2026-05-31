// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/AuraLinkManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <string.h>

#include "core/AppVersion.h"
#include "core/Logger.h"
#include "core/MathUtils.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "modules/DisplayThresholds.h"
#include "modules/StorageManager.h"
#include "modules/TimeManager.h"
#include "ui/UiOptionalGasProfile.h"

namespace {

constexpr uint32_t kAuraLinkTaskStackSize = 12288;
constexpr UBaseType_t kAuraLinkTaskPriority = 1;
constexpr BaseType_t kAuraLinkTaskCore = 0;
constexpr uint32_t kInitialUploadDelayMs = 60UL * 1000UL;
constexpr uint8_t kInitialFastUploadCount = 3;
constexpr uint32_t kFastUploadIntervalMs = 60UL * 1000UL;
constexpr uint32_t kUrgentUploadBootGraceMs = kInitialUploadDelayMs;
constexpr uint32_t kUrgentUploadMinIntervalMs = 60UL * 1000UL;
constexpr uint32_t kUrgentWatchSustainMs = 30UL * 1000UL;
constexpr uint32_t kUrgentAlertSustainMs = 10UL * 1000UL;
constexpr uint32_t kActiveAlertFastWindowMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kActiveAlertFollowupIntervalMs = 60UL * 1000UL;
constexpr uint32_t kUrgentUploadQuotaWindowMs = 60UL * 60UL * 1000UL;
constexpr uint8_t kUrgentUploadMaxPerWindow = 6;
constexpr uint32_t kMinUploadIntervalSeconds = 30;
constexpr uint32_t kMaxUploadIntervalSeconds = 24UL * 60UL * 60UL;

constexpr uint32_t kAlertMetricTemp = 1UL << 0;
constexpr uint32_t kAlertMetricHumidity = 1UL << 1;
constexpr uint32_t kAlertMetricDewPoint = 1UL << 2;
constexpr uint32_t kAlertMetricAbsoluteHumidity = 1UL << 3;
constexpr uint32_t kAlertMetricCo2 = 1UL << 4;
constexpr uint32_t kAlertMetricPm05 = 1UL << 5;
constexpr uint32_t kAlertMetricPm1 = 1UL << 6;
constexpr uint32_t kAlertMetricPm25 = 1UL << 7;
constexpr uint32_t kAlertMetricPm4 = 1UL << 8;
constexpr uint32_t kAlertMetricPm10 = 1UL << 9;
constexpr uint32_t kAlertMetricVoc = 1UL << 10;
constexpr uint32_t kAlertMetricNox = 1UL << 11;
constexpr uint32_t kAlertMetricHcho = 1UL << 12;
constexpr uint32_t kAlertMetricCo = 1UL << 13;
constexpr uint32_t kAlertMetricOptionalGas = 1UL << 14;

struct AlertMetricName {
    uint32_t mask = 0;
    const char *name = "";
};

constexpr AlertMetricName kAlertMetricNames[] = {
    {kAlertMetricTemp, "temperature"},
    {kAlertMetricHumidity, "humidity"},
    {kAlertMetricDewPoint, "dew_point"},
    {kAlertMetricAbsoluteHumidity, "absolute_humidity"},
    {kAlertMetricCo2, "co2"},
    {kAlertMetricPm05, "pm05"},
    {kAlertMetricPm1, "pm1"},
    {kAlertMetricPm25, "pm25"},
    {kAlertMetricPm4, "pm4"},
    {kAlertMetricPm10, "pm10"},
    {kAlertMetricVoc, "voc"},
    {kAlertMetricNox, "nox"},
    {kAlertMetricHcho, "hcho"},
    {kAlertMetricCo, "co"},
    {kAlertMetricOptionalGas, "optional_gas"},
};

bool base_url_configured() {
    return Config::AURA_LINK_BASE_URL && Config::AURA_LINK_BASE_URL[0] != '\0';
}

String build_url(const char *path) {
    String base = Config::AURA_LINK_BASE_URL;
    while (base.endsWith("/")) {
        base.remove(base.length() - 1);
    }
    if (!path || path[0] == '\0') {
        return base;
    }
    if (path[0] == '/') {
        return base + path;
    }
    return base + "/" + path;
}

bool is_https_url(const String &url) {
    return url.startsWith("https://");
}

bool is_http_url(const String &url) {
    return url.startsWith("http://");
}

int post_json(const char *path, const String &body, const char *bearer_token, String &response) {
    response = "";
    if (!base_url_configured()) {
        return -1;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return -1;
    }

    const String url = build_url(path);
    HTTPClient http;
    http.setTimeout(Config::AURA_LINK_HTTP_TIMEOUT_MS);
    http.setReuse(false);

    int status = -1;
    if (is_https_url(url)) {
        WiFiClientSecure client;
        if (Config::AURA_LINK_ROOT_CA_PEM && Config::AURA_LINK_ROOT_CA_PEM[0] != '\0') {
            client.setCACert(Config::AURA_LINK_ROOT_CA_PEM);
        } else if (Config::AURA_LINK_ALLOW_INSECURE_TLS) {
            client.setInsecure();
        } else {
            return -1;
        }
        if (!http.begin(client, url)) {
            return -1;
        }
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");
        if (bearer_token && bearer_token[0] != '\0') {
            http.addHeader("Authorization", String("Bearer ") + bearer_token);
        }
        status = http.POST(body);
        response = http.getString();
        http.end();
        return status;
    }

    if (!is_http_url(url) || !Config::AURA_LINK_ALLOW_INSECURE_TLS) {
        return -1;
    }

    WiFiClient client;
    if (!http.begin(client, url)) {
        return -1;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    if (bearer_token && bearer_token[0] != '\0') {
        http.addHeader("Authorization", String("Bearer ") + bearer_token);
    }
    status = http.POST(body);
    response = http.getString();
    http.end();
    return status;
}

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

void copy_string(char *dst, size_t dst_size, const String &src) {
    copy_string(dst, dst_size, src.c_str());
}

void format_recorded_at(time_t epoch, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (epoch <= Config::TIME_VALID_EPOCH) {
        snprintf(out, out_size, "1970-01-01T00:00:00.000Z");
        return;
    }
    tm utc_tm{};
    gmtime_r(&epoch, &utc_tm);
    snprintf(out,
             out_size,
             "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
             utc_tm.tm_year + 1900,
             utc_tm.tm_mon + 1,
             utc_tm.tm_mday,
             utc_tm.tm_hour,
             utc_tm.tm_min,
             utc_tm.tm_sec);
}

void add_capabilities(JsonObject root) {
    JsonObject hardware = root["hardware"].to<JsonObject>();
    hardware["model"] = "project-aura-aq";
    hardware["display"] = "touch-800x480";
    hardware["flash_mb"] = static_cast<uint32_t>(ESP.getFlashChipSize() / (1024UL * 1024UL));

    JsonObject capabilities = root["capabilities"].to<JsonObject>();
    JsonArray sensors = capabilities["sensors"].to<JsonArray>();
    sensors.add("temperature");
    sensors.add("humidity");
    sensors.add("pressure");
    sensors.add("pressure_delta");
    sensors.add("dew_point");
    sensors.add("absolute_humidity");
    sensors.add("mold_risk");
    sensors.add("pm05");
    sensors.add("pm1");
    sensors.add("pm25");
    sensors.add("pm4");
    sensors.add("pm10");
    sensors.add("co2");
    sensors.add("voc");
    sensors.add("nox");
    sensors.add("hcho");
    sensors.add("co");
    sensors.add("optional_gas");
    capabilities["ota"] = true;
    capabilities["touch"] = true;
}

String response_error_code(const String &body) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        return "";
    }
    const char *error = doc["error"] | "";
    return String(error);
}

uint32_t clamp_upload_interval(uint32_t seconds) {
    if (seconds < kMinUploadIntervalSeconds) {
        return kMinUploadIntervalSeconds;
    }
    if (seconds > kMaxUploadIntervalSeconds) {
        return kMaxUploadIntervalSeconds;
    }
    return seconds;
}

bool add_float(JsonObject obj, const char *key, float value, bool valid) {
    if (!valid || !isfinite(value)) {
        return false;
    }
    obj[key] = value;
    return true;
}

bool add_int(JsonObject obj, const char *key, int value, bool valid) {
    if (!valid) {
        return false;
    }
    obj[key] = value;
    return true;
}

uint8_t alert_level_from_display_band(DisplayThresholds::Band band) {
    switch (band) {
        case DisplayThresholds::Band::Red:
            return 2;
        case DisplayThresholds::Band::Orange:
            return 1;
        case DisplayThresholds::Band::Green:
        case DisplayThresholds::Band::Yellow:
        case DisplayThresholds::Band::Invalid:
        default:
            return 0;
    }
}

uint8_t alert_level_from_high(float value, float yellow_max, float orange_max) {
    if (!isfinite(value) || value < 0.0f) {
        return 0;
    }
    if (value > orange_max) {
        return 2;
    }
    if (value > yellow_max) {
        return 1;
    }
    return 0;
}

bool urgent_quota_available(uint32_t now_ms, uint32_t window_started_ms, uint8_t count) {
    if (window_started_ms == 0) {
        return true;
    }
    if (static_cast<uint32_t>(now_ms - window_started_ms) >= kUrgentUploadQuotaWindowMs) {
        return true;
    }
    return count < kUrgentUploadMaxPerWindow;
}

} // namespace

void AuraLinkManager::begin(StorageManager &storage) {
    storage_ = &storage;
    loadState();
    if (enabled_ && !device_token_.isEmpty()) {
        beginFastUploadPhase(millis());
    }

    if (worker_task_ != nullptr) {
        started_ = true;
        return;
    }

    TaskHandle_t created = nullptr;
    const BaseType_t ok = xTaskCreatePinnedToCore(workerTaskTrampoline,
                                                  "aura_link",
                                                  kAuraLinkTaskStackSize,
                                                  this,
                                                  kAuraLinkTaskPriority,
                                                  &created,
                                                  kAuraLinkTaskCore);
    if (ok != pdPASS || created == nullptr) {
        LOGW("AuraLink", "background task unavailable; Link disabled until restart");
        started_ = false;
        return;
    }
    worker_task_ = created;
    started_ = true;
}

void AuraLinkManager::poll(uint32_t now_ms,
                           const SensorData &data,
                           bool sensor_warmup_active,
                           const TimeManager &time_manager,
                           const DisplayThresholds::Config &thresholds) {
    consumeWorkerResult(now_ms);
    if (!started_) {
        return;
    }

    const AlertSnapshot alert_state =
        updateAlertState(now_ms, data, sensor_warmup_active, thresholds);
    updateActiveAlertWindow(now_ms, alert_state);

    if (shouldRetryUpload(now_ms)) {
        if (submitCommand(retry_ingest_command_)) {
            inflight_ingest_command_ = retry_ingest_command_;
            inflight_ingest_valid_ = true;
            last_upload_attempt_ms_ = now_ms;
            scheduleNextUpload(now_ms, Config::AURA_LINK_UPLOAD_RETRY_MS);
        }
        return;
    }

    const bool regular_upload_due = shouldUpload(now_ms, data);
    const bool alert_state_changed = !sameAlertSnapshot(alert_state, reported_alert_state_);
    const bool urgent_upload_due =
        !regular_upload_due && shouldTriggerUrgentUpload(now_ms, data, alert_state);
    const UrgentDiagnostics urgent_diagnostics =
        buildUrgentDiagnostics(now_ms,
                               data,
                               alert_state,
                               regular_upload_due,
                               urgent_upload_due,
                               alert_state_changed);
    const uint32_t next_upload_delay_ms = plannedNextUploadDelay(now_ms, alert_state);
    if (!regular_upload_due && !urgent_upload_due) {
        return;
    }

    WorkerCommand command;
    UrgentDiagnostics command_diagnostics = urgent_diagnostics;
    command_diagnostics.planned_next_upload_delay_ms = next_upload_delay_ms;
    if (!buildIngestCommand(command,
                            data,
                            sensor_warmup_active,
                            time_manager,
                            alert_state,
                            alert_state_changed ? UploadReason::AlertTransition : UploadReason::Scheduled,
                            command_diagnostics)) {
        scheduleNextUpload(now_ms, Config::AURA_LINK_UPLOAD_RETRY_MS);
        return;
    }

    const uint32_t next_sequence = sequence_ + 1;
    command.sequence = next_sequence;
    if (!submitCommand(command)) {
        return;
    }

    inflight_ingest_command_ = command;
    inflight_ingest_valid_ = true;
    retry_ingest_pending_ = false;
    sequence_ = next_sequence;
    last_upload_attempt_ms_ = now_ms;
    recordSubmittedUploadForScheduling();
    if (urgent_upload_due) {
        recordUrgentUpload(now_ms);
    }
    saveState();
    scheduleNextUpload(now_ms, next_upload_delay_ms);
}

bool AuraLinkManager::claim(const char *pairing_code) {
    if (!base_url_configured()) {
        claim_status_ = ClaimStatus::NotConfigured;
        return false;
    }
    if (!started_ || !pairing_code || strlen(pairing_code) != 6) {
        claim_status_ = ClaimStatus::ServerUnreachable;
        return false;
    }

    WorkerCommand command;
    command.op = WorkerOp::Claim;
    copy_string(command.pairing_code, sizeof(command.pairing_code), pairing_code);
    if (!submitCommand(command)) {
        claim_status_ = ClaimStatus::ServerUnreachable;
        return false;
    }
    claim_status_ = ClaimStatus::Pending;
    return true;
}

void AuraLinkManager::reset() {
    char old_device_id[kDeviceIdMax] = {};
    char old_token[kDeviceTokenMax] = {};
    copy_string(old_device_id, sizeof(old_device_id), device_id_);
    copy_string(old_token, sizeof(old_token), device_token_);
    clearLocalState();

    if (!started_ || old_device_id[0] == '\0' || old_token[0] == '\0' || !base_url_configured()) {
        return;
    }

    WorkerCommand command;
    command.op = WorkerOp::Reset;
    copy_string(command.device_id, sizeof(command.device_id), old_device_id);
    copy_string(command.device_token, sizeof(command.device_token), old_token);
    submitCommand(command);
}

void AuraLinkManager::clearClaimStatus() {
    if (claim_status_ != ClaimStatus::Pending) {
        claim_status_ = ClaimStatus::Idle;
    }
}

AuraLinkManager::Snapshot AuraLinkManager::snapshot() const {
    Snapshot out;
    out.configured = base_url_configured();
    out.enabled = enabled_;
    out.linked = enabled_ && !device_id_.isEmpty() && !device_token_.isEmpty();
    out.upload_paused = upload_paused_;
    out.busy = worker_busy_;
    out.claim_status = claim_status_;
    out.upload_interval_seconds = upload_interval_seconds_;
    out.offline_after_seconds = offline_after_seconds_;
    out.last_upload_success_ms = last_upload_success_ms_;
    out.last_upload_attempt_ms = last_upload_attempt_ms_;
    out.sequence = sequence_;
    out.device_id = device_id_;
    out.firmware_channel = firmware_channel_;
    out.auto_ota_enabled = auto_ota_enabled_;
    return out;
}

void AuraLinkManager::workerTaskTrampoline(void *arg) {
    auto *self = static_cast<AuraLinkManager *>(arg);
    if (self) {
        self->workerTask();
    }
    vTaskDelete(nullptr);
}

void AuraLinkManager::workerTask() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        WorkerCommand command;
        bool has_command = false;
        portENTER_CRITICAL(&worker_mux_);
        if (worker_command_pending_) {
            command = worker_command_;
            worker_command_pending_ = false;
            has_command = true;
        }
        portEXIT_CRITICAL(&worker_mux_);

        if (!has_command) {
            continue;
        }

        WorkerResult result = executeCommand(command);
        portENTER_CRITICAL(&worker_mux_);
        worker_result_ = result;
        worker_result_ready_ = true;
        worker_busy_ = false;
        portEXIT_CRITICAL(&worker_mux_);
    }
}

AuraLinkManager::WorkerResult AuraLinkManager::executeCommand(const WorkerCommand &command) {
    WorkerResult result;
    result.op = command.op;
    result.sequence = command.sequence;

    if (command.op == WorkerOp::Claim) {
        JsonDocument request;
        request["pairing_code"] = command.pairing_code;
        request["firmware_version"] = AppVersion::fullVersion();
        add_capabilities(request.as<JsonObject>());

        String body;
        serializeJson(request, body);
        String response;
        const int status = post_json("/api/devices/claim", body, nullptr, response);
        if (status != 200) {
            result.claim_status = mapClaimError(status, response);
            return result;
        }

        JsonDocument doc;
        if (deserializeJson(doc, response)) {
            result.claim_status = ClaimStatus::ServerUnreachable;
            return result;
        }
        const char *device_id = doc["device_id"] | "";
        const char *device_token = doc["device_token"] | "";
        if (device_id[0] == '\0' || device_token[0] == '\0') {
            result.claim_status = ClaimStatus::ServerUnreachable;
            return result;
        }

        copy_string(result.device_id, sizeof(result.device_id), device_id);
        copy_string(result.device_token, sizeof(result.device_token), device_token);
        JsonObject config = doc["config"].as<JsonObject>();
        result.upload_interval_seconds =
            clamp_upload_interval(config["upload_interval_seconds"] |
                                  Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS);
        result.offline_after_seconds =
            config["offline_after_seconds"] | Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
        copy_string(result.firmware_channel,
                    sizeof(result.firmware_channel),
                    config["firmware_channel"] | "stable");
        result.auto_ota_enabled = config["auto_ota_enabled"] | false;
        result.ok = true;
        result.claim_status = ClaimStatus::Success;
        return result;
    }

    if (command.op == WorkerOp::Reset) {
        JsonDocument request;
        request["device_id"] = command.device_id;
        String body;
        serializeJson(request, body);
        String response;
        const int status = post_json("/api/devices/reset", body, command.device_token, response);
        result.ok = status >= 200 && status < 300;
        return result;
    }

    if (command.op == WorkerOp::Ingest) {
        JsonDocument request;
        request["device_id"] = command.device_id;
        request["sequence"] = command.sequence;
        request["firmware_version"] = AppVersion::fullVersion();
        request["upload_interval_seconds"] = command.upload_interval_seconds;
        const char *upload_reason =
            command.upload_reason == UploadReason::AlertTransition ? "alert_transition" : "scheduled";
        request["upload_reason"] = upload_reason;

        char recorded_at[32] = {};
        format_recorded_at(command.recorded_at, recorded_at, sizeof(recorded_at));
        request["recorded_at"] = recorded_at;

        JsonObject sensors = request["sensors"].to<JsonObject>();
        add_float(sensors, "temperature_c", command.data.temperature, command.data.temp_valid);
        add_float(sensors, "humidity_percent", command.data.humidity, command.data.hum_valid);
        if (command.data.temp_valid && command.data.hum_valid) {
            const float ah = MathUtils::compute_absolute_humidity_gm3(command.data.temperature,
                                                                      command.data.humidity);
            const float dp = MathUtils::compute_dew_point_c(command.data.temperature,
                                                            command.data.humidity);
            const int mr = MathUtils::compute_mold_risk_index(command.data.temperature,
                                                              command.data.humidity);
            add_float(sensors, "absolute_humidity_gm3", ah, isfinite(ah));
            add_float(sensors, "dew_point_c", dp, isfinite(dp));
            add_int(sensors, "mold_risk", mr, mr >= 0);
        }
        add_float(sensors, "pressure_hpa", command.data.pressure, command.data.pressure_valid);
        add_float(sensors,
                  "pressure_delta_3h_hpa",
                  command.data.pressure_delta_3h,
                  command.data.pressure_delta_3h_valid);
        add_float(sensors,
                  "pressure_delta_24h_hpa",
                  command.data.pressure_delta_24h,
                  command.data.pressure_delta_24h_valid);
        add_float(sensors, "pm05_count_cm3", command.data.pm05, command.data.pm05_valid);
        add_float(sensors, "pm1_ugm3", command.data.pm1, command.data.pm1_valid);
        add_float(sensors, "pm25_ugm3", command.data.pm25, command.data.pm25_valid);
        add_float(sensors, "pm4_ugm3", command.data.pm4, command.data.pm4_valid);
        add_float(sensors, "pm10_ugm3", command.data.pm10, command.data.pm10_valid);
        add_int(sensors, "co2_ppm", command.data.co2, command.data.co2_valid);
        add_int(sensors, "voc_index", command.data.voc_index, command.data.voc_valid);
        add_int(sensors, "nox_index", command.data.nox_index, command.data.nox_valid);
        add_float(sensors, "hcho_ppb", command.data.hcho, command.data.hcho_valid);
        add_float(sensors, "co_ppm", command.data.co_ppm, command.data.co_valid);
        add_float(sensors,
                  "optional_gas_ppm",
                  command.data.optional_gas_ppm,
                  command.data.optional_gas_valid);

        JsonObject wifi = request["wifi"].to<JsonObject>();
        wifi["rssi_dbm"] = command.wifi_rssi;

        JsonObject health = request["health"].to<JsonObject>();
        health["uptime_seconds"] = command.uptime_s;
        health["heap_free_bytes"] = command.free_heap;
        health["psram_free_bytes"] = command.free_psram;
        health["reset_reason"] = command.reset_reason;

        JsonObject sensor_status = request["sensor_status"].to<JsonObject>();
        sensor_status["co_present"] = command.data.co_sensor_present;
        sensor_status["co_warmup"] = command.data.co_sensor_present && command.data.co_warmup;
        sensor_status["hcho_present"] = command.data.hcho_sensor_present;
        sensor_status["hcho_warmup"] = command.data.hcho_sensor_present && command.data.hcho_warmup;

        const auto optional_gas_type =
            static_cast<DfrOptionalGasSensor::OptionalGasType>(command.data.optional_gas_type);
        const bool optional_gas_present =
            command.data.optional_gas_sensor_present &&
            optional_gas_type != DfrOptionalGasSensor::OptionalGasType::None;
        sensor_status["optional_gas_present"] = optional_gas_present;
        sensor_status["optional_gas_warmup"] =
            optional_gas_present && command.data.optional_gas_warmup;
        if (optional_gas_present) {
            sensor_status["optional_gas_type"] =
                DfrOptionalGasSensor::optionalGasLabel(optional_gas_type);
        }

        const char *alert_level = "normal";
        switch (command.alert_state.level) {
            case AlertUploadLevel::Alert:
                alert_level = "alert";
                break;
            case AlertUploadLevel::Watch:
                alert_level = "watch";
                break;
            case AlertUploadLevel::Normal:
            default:
                alert_level = "normal";
                break;
        }
        JsonObject alert_state = request["alert_state"].to<JsonObject>();
        alert_state["level"] = alert_level;
        alert_state["upload_reason"] = upload_reason;
        JsonArray active_alerts = alert_state["active"].to<JsonArray>();
        for (const AlertMetricName &metric : kAlertMetricNames) {
            if ((command.alert_state.active_mask & metric.mask) != 0) {
                active_alerts.add(metric.name);
            }
        }

        JsonObject alert_debug = alert_state["debug"].to<JsonObject>();
        alert_debug["urgent_block_reason"] =
            urgentBlockReasonName(command.urgent_diagnostics.block_reason);
        alert_debug["regular_upload_due"] = command.urgent_diagnostics.regular_upload_due;
        alert_debug["urgent_upload_due"] = command.urgent_diagnostics.urgent_upload_due;
        alert_debug["alert_state_changed"] = command.urgent_diagnostics.alert_state_changed;
        alert_debug["candidate_level"] =
            alertLevelName(command.urgent_diagnostics.candidate_state.level);
        JsonArray candidate_active = alert_debug["candidate_active"].to<JsonArray>();
        for (const AlertMetricName &metric : kAlertMetricNames) {
            if ((command.urgent_diagnostics.candidate_state.active_mask & metric.mask) != 0) {
                candidate_active.add(metric.name);
            }
        }
        alert_debug["stable_level"] =
            alertLevelName(command.urgent_diagnostics.stable_state.level);
        JsonArray stable_active = alert_debug["stable_active"].to<JsonArray>();
        for (const AlertMetricName &metric : kAlertMetricNames) {
            if ((command.urgent_diagnostics.stable_state.active_mask & metric.mask) != 0) {
                stable_active.add(metric.name);
            }
        }
        alert_debug["reported_level"] =
            alertLevelName(command.urgent_diagnostics.reported_state.level);
        JsonArray reported_active = alert_debug["reported_active"].to<JsonArray>();
        for (const AlertMetricName &metric : kAlertMetricNames) {
            if ((command.urgent_diagnostics.reported_state.active_mask & metric.mask) != 0) {
                reported_active.add(metric.name);
            }
        }
        alert_debug["candidate_age_ms"] = command.urgent_diagnostics.candidate_age_ms;
        alert_debug["candidate_sustain_ms"] = command.urgent_diagnostics.candidate_sustain_ms;
        alert_debug["boot_grace_remaining_ms"] =
            command.urgent_diagnostics.boot_grace_remaining_ms;
        alert_debug["urgent_cooldown_remaining_ms"] =
            command.urgent_diagnostics.urgent_cooldown_remaining_ms;
        alert_debug["urgent_quota_remaining"] =
            command.urgent_diagnostics.urgent_quota_remaining;
        alert_debug["next_regular_upload_in_ms"] =
            command.urgent_diagnostics.next_regular_upload_in_ms;
        alert_debug["last_success_age_ms"] = command.urgent_diagnostics.last_success_age_ms;
        alert_debug["active_alert_age_ms"] = command.urgent_diagnostics.active_alert_age_ms;
        alert_debug["planned_next_upload_delay_ms"] =
            command.urgent_diagnostics.planned_next_upload_delay_ms;
        alert_debug["startup_fast_uploads_remaining"] =
            command.urgent_diagnostics.startup_fast_uploads_remaining;

        String body;
        serializeJson(request, body);
        String response;
        const int status = post_json("/api/devices/ingest", body, command.device_token, response);
        if (status >= 200 && status < 300) {
            result.ok = true;
            JsonDocument doc;
            if (!deserializeJson(doc, response)) {
                result.duplicate = doc["duplicate"] | false;
                const uint32_t next_interval =
                    doc["next_upload_interval_seconds"] | command.upload_interval_seconds;
                result.upload_interval_seconds = clamp_upload_interval(next_interval);
            } else {
                result.upload_interval_seconds = command.upload_interval_seconds;
            }
            return result;
        }

        const String error = response_error_code(response);
        result.upload_paused = error == "account_upload_paused";
        return result;
    }

    return result;
}

bool AuraLinkManager::submitCommand(const WorkerCommand &command) {
    if (!worker_task_) {
        return false;
    }
    portENTER_CRITICAL(&worker_mux_);
    if (worker_busy_ || worker_command_pending_) {
        portEXIT_CRITICAL(&worker_mux_);
        return false;
    }
    worker_command_ = command;
    worker_command_pending_ = true;
    worker_busy_ = true;
    portEXIT_CRITICAL(&worker_mux_);
    xTaskNotifyGive(worker_task_);
    return true;
}

void AuraLinkManager::consumeWorkerResult(uint32_t now_ms) {
    WorkerResult result;
    bool ready = false;
    portENTER_CRITICAL(&worker_mux_);
    if (worker_result_ready_) {
        result = worker_result_;
        worker_result_ready_ = false;
        ready = true;
    }
    portEXIT_CRITICAL(&worker_mux_);

    if (!ready) {
        return;
    }

    if (result.op == WorkerOp::Claim) {
        claim_status_ = result.claim_status;
        if (!result.ok || result.claim_status != ClaimStatus::Success) {
            return;
        }
        enabled_ = true;
        upload_paused_ = false;
        device_id_ = result.device_id;
        device_token_ = result.device_token;
        upload_interval_seconds_ = result.upload_interval_seconds;
        offline_after_seconds_ = result.offline_after_seconds;
        firmware_channel_ = result.firmware_channel;
        auto_ota_enabled_ = result.auto_ota_enabled;
        sequence_ = 0;
        last_upload_success_ms_ = 0;
        last_upload_attempt_ms_ = 0;
        alert_candidate_state_ = {};
        stable_alert_state_ = {};
        reported_alert_state_ = {};
        alert_candidate_since_ms_ = now_ms;
        last_urgent_upload_ms_ = 0;
        urgent_upload_window_started_ms_ = 0;
        urgent_uploads_in_window_ = 0;
        saveState();
        beginFastUploadPhase(now_ms);
        LOGI("AuraLink", "device linked");
        return;
    }

    if (result.op == WorkerOp::Ingest) {
        if (!enabled_ || device_id_.isEmpty() || device_token_.isEmpty()) {
            inflight_ingest_valid_ = false;
            retry_ingest_pending_ = false;
            return;
        }
        if (result.ok) {
            upload_paused_ = false;
            if (inflight_ingest_valid_) {
                reported_alert_state_ = inflight_ingest_command_.alert_state;
            }
            inflight_ingest_valid_ = false;
            retry_ingest_pending_ = false;
            upload_interval_seconds_ = result.upload_interval_seconds;
            last_upload_success_ms_ = now_ms;
            saveState();
            return;
        }
        if (result.upload_paused) {
            upload_paused_ = true;
            inflight_ingest_valid_ = false;
            retry_ingest_pending_ = false;
            saveState();
            LOGW("AuraLink", "upload paused by account state");
            return;
        }
        if (inflight_ingest_valid_) {
            retry_ingest_command_ = inflight_ingest_command_;
            retry_ingest_pending_ = true;
        }
        scheduleNextUpload(now_ms, Config::AURA_LINK_UPLOAD_RETRY_MS);
        return;
    }
}

bool AuraLinkManager::loadState() {
    enabled_ = false;
    upload_paused_ = false;
    device_id_ = "";
    device_token_ = "";
    upload_interval_seconds_ = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
    offline_after_seconds_ = Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
    firmware_channel_ = "stable";
    auto_ota_enabled_ = false;
    sequence_ = 0;
    if (!storage_) {
        return false;
    }

    String json;
    if (!storage_->loadText(StorageManager::kAuraLinkPath, json) || json.isEmpty()) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        LOGW("AuraLink", "stored Link state is invalid; resetting local Link state");
        storage_->removeBlob(StorageManager::kAuraLinkPath);
        return false;
    }

    enabled_ = doc["enabled"] | false;
    device_id_ = doc["device_id"] | "";
    device_token_ = doc["device_token"] | "";
    upload_interval_seconds_ =
        clamp_upload_interval(doc["upload_interval_seconds"] |
                              Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS);
    offline_after_seconds_ =
        doc["offline_after_seconds"] | Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
    firmware_channel_ = doc["firmware_channel"] | "stable";
    auto_ota_enabled_ = doc["auto_ota_enabled"] | false;
    sequence_ = doc["sequence"] | 0;
    upload_paused_ = doc["upload_paused"] | false;
    if (device_id_.isEmpty() || device_token_.isEmpty()) {
        enabled_ = false;
        upload_paused_ = false;
    }
    return enabled_;
}

bool AuraLinkManager::saveState() {
    if (!storage_) {
        return false;
    }
    JsonDocument doc;
    doc["enabled"] = enabled_;
    doc["device_id"] = device_id_;
    doc["device_token"] = device_token_;
    doc["upload_interval_seconds"] = upload_interval_seconds_;
    doc["offline_after_seconds"] = offline_after_seconds_;
    doc["firmware_channel"] = firmware_channel_;
    doc["auto_ota_enabled"] = auto_ota_enabled_;
    doc["sequence"] = sequence_;
    doc["upload_paused"] = upload_paused_;

    String json;
    serializeJson(doc, json);
    if (!storage_->saveTextAtomic(StorageManager::kAuraLinkPath, json)) {
        LOGW("AuraLink", "failed to persist Link state");
        return false;
    }
    return true;
}

void AuraLinkManager::clearLocalState() {
    enabled_ = false;
    upload_paused_ = false;
    device_id_ = "";
    device_token_ = "";
    upload_interval_seconds_ = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
    offline_after_seconds_ = Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
    firmware_channel_ = "stable";
    auto_ota_enabled_ = false;
    sequence_ = 0;
    next_upload_due_ms_ = 0;
    last_upload_success_ms_ = 0;
    last_upload_attempt_ms_ = 0;
    inflight_ingest_valid_ = false;
    retry_ingest_pending_ = false;
    claim_status_ = ClaimStatus::Idle;
    alert_candidate_state_ = {};
    stable_alert_state_ = {};
    reported_alert_state_ = {};
    alert_candidate_since_ms_ = 0;
    last_urgent_upload_ms_ = 0;
    urgent_upload_window_started_ms_ = 0;
    urgent_uploads_in_window_ = 0;
    startup_fast_uploads_remaining_ = 0;
    active_alert_started_ms_ = 0;
    if (storage_) {
        storage_->removeBlob(StorageManager::kAuraLinkPath);
    }
}

void AuraLinkManager::scheduleNextUpload(uint32_t now_ms, uint32_t delay_ms) {
    next_upload_due_ms_ = now_ms + delay_ms;
}

void AuraLinkManager::beginFastUploadPhase(uint32_t now_ms) {
    startup_fast_uploads_remaining_ = kInitialFastUploadCount;
    active_alert_started_ms_ = 0;
    scheduleNextUpload(now_ms, kInitialUploadDelayMs);
}

void AuraLinkManager::updateActiveAlertWindow(uint32_t now_ms, const AlertSnapshot &alert_state) {
    if (alert_state.level == AlertUploadLevel::Alert) {
        if (active_alert_started_ms_ == 0) {
            active_alert_started_ms_ = now_ms;
        }
        return;
    }

    active_alert_started_ms_ = 0;
}

uint32_t AuraLinkManager::plannedNextUploadDelay(uint32_t now_ms,
                                                 const AlertSnapshot &alert_state) const {
    if (startup_fast_uploads_remaining_ > 1) {
        return kFastUploadIntervalMs;
    }

    if (alert_state.level == AlertUploadLevel::Alert && active_alert_started_ms_ != 0) {
        const uint32_t active_alert_age_ms =
            static_cast<uint32_t>(now_ms - active_alert_started_ms_);
        if (active_alert_age_ms < kActiveAlertFastWindowMs) {
            return kActiveAlertFollowupIntervalMs;
        }
    }

    return upload_interval_seconds_ * 1000UL;
}

void AuraLinkManager::recordSubmittedUploadForScheduling() {
    if (startup_fast_uploads_remaining_ > 0) {
        --startup_fast_uploads_remaining_;
    }
}

bool AuraLinkManager::shouldUpload(uint32_t now_ms, const SensorData &data) const {
    if (!enabled_ || upload_paused_ || device_id_.isEmpty() || device_token_.isEmpty()) {
        return false;
    }
    if (!base_url_configured()) {
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (!hasAnyValidSensor(data)) {
        return false;
    }
    return next_upload_due_ms_ == 0 ||
           static_cast<int32_t>(now_ms - next_upload_due_ms_) >= 0;
}

bool AuraLinkManager::shouldRetryUpload(uint32_t now_ms) const {
    if (!retry_ingest_pending_) {
        return false;
    }
    if (!enabled_ || upload_paused_ || device_id_.isEmpty() || device_token_.isEmpty()) {
        return false;
    }
    if (!base_url_configured() || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    return next_upload_due_ms_ == 0 ||
           static_cast<int32_t>(now_ms - next_upload_due_ms_) >= 0;
}

bool AuraLinkManager::shouldTriggerUrgentUpload(uint32_t now_ms,
                                                const SensorData &data,
                                                const AlertSnapshot &alert_state) const {
    if (!enabled_ || upload_paused_ || device_id_.isEmpty() || device_token_.isEmpty()) {
        return false;
    }
    if (!base_url_configured() || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (!hasAnyValidSensor(data) || last_upload_success_ms_ == 0) {
        return false;
    }
    if (now_ms < kUrgentUploadBootGraceMs) {
        return false;
    }
    if (sameAlertSnapshot(alert_state, reported_alert_state_)) {
        return false;
    }
    if (last_urgent_upload_ms_ != 0 &&
        static_cast<uint32_t>(now_ms - last_urgent_upload_ms_) < kUrgentUploadMinIntervalMs) {
        return false;
    }
    return urgent_quota_available(now_ms,
                                  urgent_upload_window_started_ms_,
                                  urgent_uploads_in_window_);
}

AuraLinkManager::UrgentBlockReason AuraLinkManager::evaluateUrgentBlockReason(
    uint32_t now_ms,
    const SensorData &data,
    const AlertSnapshot &alert_state,
    bool regular_upload_due,
    bool urgent_upload_due) const {
    if (urgent_upload_due) {
        return UrgentBlockReason::None;
    }
    if (regular_upload_due) {
        return UrgentBlockReason::RegularDue;
    }
    if (!enabled_) {
        return UrgentBlockReason::Disabled;
    }
    if (upload_paused_) {
        return UrgentBlockReason::UploadPaused;
    }
    if (device_id_.isEmpty() || device_token_.isEmpty()) {
        return UrgentBlockReason::NotLinked;
    }
    if (!base_url_configured()) {
        return UrgentBlockReason::NotConfigured;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return UrgentBlockReason::NoWifi;
    }
    if (!hasAnyValidSensor(data)) {
        return UrgentBlockReason::NoValidSensor;
    }
    if (last_upload_success_ms_ == 0) {
        return UrgentBlockReason::NoSuccessfulUpload;
    }
    if (now_ms < kUrgentUploadBootGraceMs) {
        return UrgentBlockReason::BootGrace;
    }
    if (sameAlertSnapshot(alert_state, reported_alert_state_)) {
        if (!sameAlertSnapshot(alert_candidate_state_, reported_alert_state_)) {
            return UrgentBlockReason::NotStable;
        }
        return UrgentBlockReason::SameState;
    }
    if (last_urgent_upload_ms_ != 0 &&
        static_cast<uint32_t>(now_ms - last_urgent_upload_ms_) < kUrgentUploadMinIntervalMs) {
        return UrgentBlockReason::Cooldown;
    }
    if (!urgent_quota_available(now_ms,
                                urgent_upload_window_started_ms_,
                                urgent_uploads_in_window_)) {
        return UrgentBlockReason::Quota;
    }
    return UrgentBlockReason::None;
}

AuraLinkManager::UrgentDiagnostics AuraLinkManager::buildUrgentDiagnostics(
    uint32_t now_ms,
    const SensorData &data,
    const AlertSnapshot &alert_state,
    bool regular_upload_due,
    bool urgent_upload_due,
    bool alert_state_changed) const {
    UrgentDiagnostics diagnostics{};
    diagnostics.block_reason =
        evaluateUrgentBlockReason(now_ms, data, alert_state, regular_upload_due, urgent_upload_due);
    diagnostics.regular_upload_due = regular_upload_due;
    diagnostics.urgent_upload_due = urgent_upload_due;
    diagnostics.alert_state_changed = alert_state_changed;
    diagnostics.candidate_state = alert_candidate_state_;
    diagnostics.stable_state = stable_alert_state_;
    diagnostics.reported_state = reported_alert_state_;
    diagnostics.candidate_age_ms =
        alert_candidate_since_ms_ == 0 ? 0 : static_cast<uint32_t>(now_ms - alert_candidate_since_ms_);
    diagnostics.candidate_sustain_ms =
        alert_candidate_state_.level == AlertUploadLevel::Alert
            ? kUrgentAlertSustainMs
            : kUrgentWatchSustainMs;
    diagnostics.boot_grace_remaining_ms =
        now_ms < kUrgentUploadBootGraceMs ? kUrgentUploadBootGraceMs - now_ms : 0;
    diagnostics.urgent_cooldown_remaining_ms = 0;
    if (last_urgent_upload_ms_ != 0) {
        const uint32_t urgent_age_ms =
            static_cast<uint32_t>(now_ms - last_urgent_upload_ms_);
        if (urgent_age_ms < kUrgentUploadMinIntervalMs) {
            diagnostics.urgent_cooldown_remaining_ms =
                kUrgentUploadMinIntervalMs - urgent_age_ms;
        }
    }
    diagnostics.urgent_quota_remaining = kUrgentUploadMaxPerWindow;
    if (urgent_upload_window_started_ms_ != 0 &&
        static_cast<uint32_t>(now_ms - urgent_upload_window_started_ms_) <
            kUrgentUploadQuotaWindowMs) {
        diagnostics.urgent_quota_remaining =
            urgent_uploads_in_window_ >= kUrgentUploadMaxPerWindow
                ? 0
                : kUrgentUploadMaxPerWindow - urgent_uploads_in_window_;
    }
    diagnostics.next_regular_upload_in_ms = 0;
    if (next_upload_due_ms_ != 0 && static_cast<int32_t>(now_ms - next_upload_due_ms_) < 0) {
        diagnostics.next_regular_upload_in_ms =
            static_cast<uint32_t>(next_upload_due_ms_ - now_ms);
    }
    diagnostics.last_success_age_ms =
        last_upload_success_ms_ == 0 ? 0 : static_cast<uint32_t>(now_ms - last_upload_success_ms_);
    diagnostics.active_alert_age_ms =
        active_alert_started_ms_ == 0 ? 0 : static_cast<uint32_t>(now_ms - active_alert_started_ms_);
    diagnostics.startup_fast_uploads_remaining = startup_fast_uploads_remaining_;
    return diagnostics;
}

bool AuraLinkManager::buildIngestCommand(WorkerCommand &command,
                                         const SensorData &data,
                                         bool sensor_warmup_active,
                                         const TimeManager &time_manager,
                                         const AlertSnapshot &alert_state,
                                         UploadReason upload_reason,
                                         const UrgentDiagnostics &urgent_diagnostics) {
    if (device_id_.isEmpty() || device_token_.isEmpty()) {
        return false;
    }
    command.op = WorkerOp::Ingest;
    copy_string(command.device_id, sizeof(command.device_id), device_id_);
    copy_string(command.device_token, sizeof(command.device_token), device_token_);
    command.upload_interval_seconds = upload_interval_seconds_;
    command.recorded_at = time_manager.isSystemTimeValid() ? time(nullptr) : 0;
    command.data = data;
    command.sensor_warmup_active = sensor_warmup_active;
    command.wifi_rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    command.uptime_s = millis() / 1000UL;
    command.free_heap = ESP.getFreeHeap();
    command.free_psram = ESP.getPsramSize() > 0 ? ESP.getFreePsram() : 0;
    command.reset_reason = static_cast<uint32_t>(esp_reset_reason());
    command.alert_state = alert_state;
    command.upload_reason = upload_reason;
    command.urgent_diagnostics = urgent_diagnostics;
    return true;
}

AuraLinkManager::AlertSnapshot AuraLinkManager::updateAlertState(
    uint32_t now_ms,
    const SensorData &data,
    bool sensor_warmup_active,
    const DisplayThresholds::Config &thresholds) {
    const AlertSnapshot evaluated = evaluateAlertState(data, sensor_warmup_active, thresholds);
    if (!sameAlertSnapshot(evaluated, alert_candidate_state_)) {
        alert_candidate_state_ = evaluated;
        alert_candidate_since_ms_ = now_ms;
        return stable_alert_state_;
    }

    const uint32_t sustain_ms = evaluated.level == AlertUploadLevel::Alert
                                    ? kUrgentAlertSustainMs
                                    : kUrgentWatchSustainMs;
    if (alert_candidate_since_ms_ == 0) {
        alert_candidate_since_ms_ = now_ms;
    }
    if (static_cast<uint32_t>(now_ms - alert_candidate_since_ms_) >= sustain_ms) {
        stable_alert_state_ = evaluated;
    }
    return stable_alert_state_;
}

void AuraLinkManager::recordUrgentUpload(uint32_t now_ms) {
    last_urgent_upload_ms_ = now_ms;
    if (urgent_upload_window_started_ms_ == 0 ||
        static_cast<uint32_t>(now_ms - urgent_upload_window_started_ms_) >=
            kUrgentUploadQuotaWindowMs) {
        urgent_upload_window_started_ms_ = now_ms;
        urgent_uploads_in_window_ = 0;
    }
    if (urgent_uploads_in_window_ < UINT8_MAX) {
        ++urgent_uploads_in_window_;
    }
}

AuraLinkManager::AlertSnapshot AuraLinkManager::evaluateAlertState(
    const SensorData &data,
    bool sensor_warmup_active,
    const DisplayThresholds::Config &thresholds) {
    AlertSnapshot snapshot{};

    auto include_level = [&snapshot](uint32_t mask, uint8_t raw_level) {
        if (raw_level == 0) {
            return;
        }
        snapshot.active_mask |= mask;
        const AlertUploadLevel level =
            raw_level >= 2 ? AlertUploadLevel::Alert : AlertUploadLevel::Watch;
        if (static_cast<uint8_t>(level) > static_cast<uint8_t>(snapshot.level)) {
            snapshot.level = level;
        }
    };

    if (data.temp_valid && isfinite(data.temperature)) {
        include_level(kAlertMetricTemp,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyRange(data.temperature, thresholds.temp)));
    }
    if (data.hum_valid && isfinite(data.humidity)) {
        include_level(kAlertMetricHumidity,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyRange(data.humidity, thresholds.rh)));
    }
    if (data.temp_valid && data.hum_valid) {
        const float dew_point =
            MathUtils::compute_dew_point_c(data.temperature, data.humidity);
        const float absolute_humidity =
            MathUtils::compute_absolute_humidity_gm3(data.temperature, data.humidity);
        include_level(kAlertMetricDewPoint,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyRange(dew_point, thresholds.dew_point)));
        include_level(kAlertMetricAbsoluteHumidity,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyRange(absolute_humidity, thresholds.ah)));
    }
    if (data.co2_valid && data.co2 > 0) {
        include_level(kAlertMetricCo2,
                      alert_level_from_display_band(DisplayThresholds::classifyHigh(
                          static_cast<float>(data.co2), thresholds.co2)));
    }
    if (data.pm05_valid && isfinite(data.pm05)) {
        include_level(kAlertMetricPm05,
                      alert_level_from_high(data.pm05,
                                            Config::AQ_PM05_YELLOW_MAX_PPCM3,
                                            Config::AQ_PM05_ORANGE_MAX_PPCM3));
    }
    if (data.pm1_valid && isfinite(data.pm1)) {
        include_level(kAlertMetricPm1,
                      alert_level_from_high(data.pm1,
                                            Config::AQ_PM1_YELLOW_MAX_UGM3,
                                            Config::AQ_PM1_ORANGE_MAX_UGM3));
    }
    if (data.pm25_valid && isfinite(data.pm25)) {
        include_level(kAlertMetricPm25,
                      alert_level_from_high(data.pm25,
                                            Config::AQ_PM25_YELLOW_MAX_UGM3,
                                            Config::AQ_PM25_ORANGE_MAX_UGM3));
    }
    if (data.pm4_valid && isfinite(data.pm4)) {
        include_level(kAlertMetricPm4,
                      alert_level_from_high(data.pm4,
                                            Config::AQ_PM4_YELLOW_MAX_UGM3,
                                            Config::AQ_PM4_ORANGE_MAX_UGM3));
    }
    if (data.pm10_valid && isfinite(data.pm10)) {
        include_level(kAlertMetricPm10,
                      alert_level_from_high(data.pm10,
                                            Config::AQ_PM10_YELLOW_MAX_UGM3,
                                            Config::AQ_PM10_ORANGE_MAX_UGM3));
    }
    if (!sensor_warmup_active && data.voc_valid && data.voc_index >= 0) {
        include_level(kAlertMetricVoc,
                      alert_level_from_high(static_cast<float>(data.voc_index),
                                            static_cast<float>(Config::AQ_VOC_YELLOW_MAX_INDEX),
                                            static_cast<float>(Config::AQ_VOC_ORANGE_MAX_INDEX)));
    }
    if (!sensor_warmup_active && data.nox_valid && data.nox_index >= 0) {
        include_level(kAlertMetricNox,
                      alert_level_from_high(static_cast<float>(data.nox_index),
                                            static_cast<float>(Config::AQ_NOX_YELLOW_MAX_INDEX),
                                            static_cast<float>(Config::AQ_NOX_ORANGE_MAX_INDEX)));
    }
    if (data.hcho_valid && !data.hcho_warmup && isfinite(data.hcho)) {
        include_level(kAlertMetricHcho,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyHigh(data.hcho, thresholds.hcho)));
    }
    if (data.co_sensor_present && data.co_valid && !data.co_warmup && isfinite(data.co_ppm)) {
        include_level(kAlertMetricCo,
                      alert_level_from_display_band(
                          DisplayThresholds::classifyHigh(data.co_ppm, thresholds.co)));
    }
    if (data.optional_gas_sensor_present &&
        data.optional_gas_valid &&
        !data.optional_gas_warmup &&
        isfinite(data.optional_gas_ppm)) {
        const auto optional_type =
            static_cast<DfrOptionalGasSensor::OptionalGasType>(data.optional_gas_type);
        if (UiOptionalGasProfile::isKnown(optional_type)) {
            const auto &profile = UiOptionalGasProfile::forType(optional_type);
            include_level(kAlertMetricOptionalGas,
                          alert_level_from_high(data.optional_gas_ppm,
                                                profile.yellow_max_ppm,
                                                profile.orange_max_ppm));
        }
    }

    return snapshot;
}

bool AuraLinkManager::sameAlertSnapshot(const AlertSnapshot &left, const AlertSnapshot &right) {
    return left.level == right.level && left.active_mask == right.active_mask;
}

const char *AuraLinkManager::alertLevelName(AlertUploadLevel level) {
    switch (level) {
        case AlertUploadLevel::Alert:
            return "alert";
        case AlertUploadLevel::Watch:
            return "watch";
        case AlertUploadLevel::Normal:
        default:
            return "normal";
    }
}

const char *AuraLinkManager::urgentBlockReasonName(UrgentBlockReason reason) {
    switch (reason) {
        case UrgentBlockReason::RegularDue:
            return "regular_due";
        case UrgentBlockReason::Disabled:
            return "disabled";
        case UrgentBlockReason::UploadPaused:
            return "upload_paused";
        case UrgentBlockReason::NotLinked:
            return "not_linked";
        case UrgentBlockReason::NotConfigured:
            return "not_configured";
        case UrgentBlockReason::NoWifi:
            return "no_wifi";
        case UrgentBlockReason::NoValidSensor:
            return "no_valid_sensor";
        case UrgentBlockReason::NoSuccessfulUpload:
            return "no_successful_upload";
        case UrgentBlockReason::BootGrace:
            return "boot_grace";
        case UrgentBlockReason::NotStable:
            return "not_stable";
        case UrgentBlockReason::SameState:
            return "same_state";
        case UrgentBlockReason::Cooldown:
            return "cooldown";
        case UrgentBlockReason::Quota:
            return "quota";
        case UrgentBlockReason::None:
        default:
            return "none";
    }
}

bool AuraLinkManager::hasAnyValidSensor(const SensorData &data) {
    return data.temp_valid ||
           data.hum_valid ||
           data.pressure_valid ||
           data.pressure_delta_3h_valid ||
           data.pressure_delta_24h_valid ||
           data.pm05_valid ||
           data.pm1_valid ||
           data.pm25_valid ||
           data.pm4_valid ||
           data.pm10_valid ||
           data.co2_valid ||
           data.voc_valid ||
           data.nox_valid ||
           data.hcho_valid ||
           data.co_valid ||
           data.optional_gas_valid;
}

AuraLinkManager::ClaimStatus AuraLinkManager::mapClaimError(int http_status, const String &body) {
    if (http_status <= 0 || http_status >= 500) {
        return ClaimStatus::ServerUnreachable;
    }
    const String error = response_error_code(body);
    if (error == "pairing_code_expired") {
        return ClaimStatus::Expired;
    }
    if (error == "pairing_code_already_used") {
        return ClaimStatus::AlreadyUsed;
    }
    if (error == "pairing_code_not_found" ||
        error == "invalid_pairing_code" ||
        error == "invalid_claim_request" ||
        error == "missing_firmware_version") {
        return ClaimStatus::InvalidCode;
    }
    return http_status >= 400 && http_status < 500
               ? ClaimStatus::InvalidCode
               : ClaimStatus::ServerUnreachable;
}
