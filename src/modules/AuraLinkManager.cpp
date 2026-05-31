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
#include "modules/StorageManager.h"
#include "modules/TimeManager.h"

namespace {

constexpr uint32_t kAuraLinkTaskStackSize = 12288;
constexpr UBaseType_t kAuraLinkTaskPriority = 1;
constexpr BaseType_t kAuraLinkTaskCore = 0;
constexpr uint32_t kInitialUploadDelayMs = 5000;
constexpr uint32_t kMinUploadIntervalSeconds = 30;
constexpr uint32_t kMaxUploadIntervalSeconds = 24UL * 60UL * 60UL;

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

} // namespace

void AuraLinkManager::begin(StorageManager &storage) {
    storage_ = &storage;
    loadState();
    if (enabled_ && !device_token_.isEmpty()) {
        scheduleNextUpload(millis(), kInitialUploadDelayMs);
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
                           const TimeManager &time_manager) {
    consumeWorkerResult(now_ms);
    if (!started_) {
        return;
    }

    if (shouldRetryUpload(now_ms)) {
        if (submitCommand(retry_ingest_command_)) {
            inflight_ingest_command_ = retry_ingest_command_;
            inflight_ingest_valid_ = true;
            last_upload_attempt_ms_ = now_ms;
            scheduleNextUpload(now_ms, Config::AURA_LINK_UPLOAD_RETRY_MS);
        }
        return;
    }

    if (!shouldUpload(now_ms, data)) {
        return;
    }

    WorkerCommand command;
    if (!buildIngestCommand(command, data, sensor_warmup_active, time_manager)) {
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
    saveState();
    scheduleNextUpload(now_ms, upload_interval_seconds_ * 1000UL);
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
        saveState();
        scheduleNextUpload(now_ms, kInitialUploadDelayMs);
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
    if (storage_) {
        storage_->removeBlob(StorageManager::kAuraLinkPath);
    }
}

void AuraLinkManager::scheduleNextUpload(uint32_t now_ms, uint32_t delay_ms) {
    next_upload_due_ms_ = now_ms + delay_ms;
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

bool AuraLinkManager::buildIngestCommand(WorkerCommand &command,
                                         const SensorData &data,
                                         bool sensor_warmup_active,
                                         const TimeManager &time_manager) {
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
    return true;
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
