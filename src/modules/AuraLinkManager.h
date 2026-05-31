// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>

#include "config/AppData.h"
#include "config/AppConfig.h"

class StorageManager;
class TimeManager;
namespace DisplayThresholds {
struct Config;
}

class AuraLinkManager {
public:
    enum class ClaimStatus : uint8_t {
        Idle = 0,
        Pending,
        Success,
        InvalidCode,
        Expired,
        AlreadyUsed,
        ServerUnreachable,
        NotConfigured,
    };

    struct Snapshot {
        bool configured = false;
        bool enabled = false;
        bool linked = false;
        bool upload_paused = false;
        bool busy = false;
        ClaimStatus claim_status = ClaimStatus::Idle;
        uint32_t upload_interval_seconds = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
        uint32_t offline_after_seconds = Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
        uint32_t last_upload_success_ms = 0;
        uint32_t last_upload_attempt_ms = 0;
        uint32_t sequence = 0;
        String device_id;
        String firmware_channel = "stable";
        bool auto_ota_enabled = false;
    };

    void begin(StorageManager &storage);
    void poll(uint32_t now_ms,
              const SensorData &data,
              bool sensor_warmup_active,
              const TimeManager &time_manager,
              const DisplayThresholds::Config &thresholds);

    bool claim(const char *pairing_code);
    void reset();
    void clearClaimStatus();

    Snapshot snapshot() const;

private:
    static constexpr size_t kDeviceIdMax = 96;
    static constexpr size_t kDeviceTokenMax = 192;

    enum class WorkerOp : uint8_t {
        None = 0,
        Claim,
        Reset,
        Ingest,
    };

    enum class AlertUploadLevel : uint8_t {
        Normal = 0,
        Watch,
        Alert,
    };

    struct AlertSnapshot {
        AlertUploadLevel level = AlertUploadLevel::Normal;
        uint32_t active_mask = 0;
    };

    struct WorkerCommand {
        WorkerOp op = WorkerOp::None;
        char pairing_code[7] = {};
        char device_id[kDeviceIdMax] = {};
        char device_token[kDeviceTokenMax] = {};
        uint32_t sequence = 0;
        uint32_t upload_interval_seconds = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
        time_t recorded_at = 0;
        SensorData data{};
        bool sensor_warmup_active = false;
        int32_t wifi_rssi = 0;
        uint32_t uptime_s = 0;
        uint32_t free_heap = 0;
        uint32_t free_psram = 0;
        uint32_t reset_reason = 0;
        AlertSnapshot alert_state{};
    };

    struct WorkerResult {
        WorkerOp op = WorkerOp::None;
        ClaimStatus claim_status = ClaimStatus::Idle;
        bool ok = false;
        bool upload_paused = false;
        bool duplicate = false;
        char device_id[kDeviceIdMax] = {};
        char device_token[kDeviceTokenMax] = {};
        uint32_t upload_interval_seconds = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
        uint32_t offline_after_seconds = Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
        char firmware_channel[24] = "stable";
        bool auto_ota_enabled = false;
        uint32_t sequence = 0;
    };

    static void workerTaskTrampoline(void *arg);
    void workerTask();
    WorkerResult executeCommand(const WorkerCommand &command);

    bool submitCommand(const WorkerCommand &command);
    void consumeWorkerResult(uint32_t now_ms);
    bool loadState();
    bool saveState();
    void clearLocalState();
    void scheduleNextUpload(uint32_t now_ms, uint32_t delay_ms);
    bool shouldUpload(uint32_t now_ms, const SensorData &data) const;
    bool shouldRetryUpload(uint32_t now_ms) const;
    bool shouldTriggerUrgentUpload(uint32_t now_ms,
                                   const SensorData &data,
                                   const AlertSnapshot &alert_state) const;
    bool buildIngestCommand(WorkerCommand &command,
                            const SensorData &data,
                            bool sensor_warmup_active,
                            const TimeManager &time_manager,
                            const AlertSnapshot &alert_state);
    AlertSnapshot updateAlertState(uint32_t now_ms,
                                   const SensorData &data,
                                   bool sensor_warmup_active,
                                   const DisplayThresholds::Config &thresholds);
    void recordUrgentUpload(uint32_t now_ms);

    static bool hasAnyValidSensor(const SensorData &data);
    static AlertSnapshot evaluateAlertState(const SensorData &data,
                                            bool sensor_warmup_active,
                                            const DisplayThresholds::Config &thresholds);
    static bool sameAlertSnapshot(const AlertSnapshot &left, const AlertSnapshot &right);
    static ClaimStatus mapClaimError(int http_status, const String &body);

    StorageManager *storage_ = nullptr;
    bool started_ = false;
    bool enabled_ = false;
    bool upload_paused_ = false;
    String device_id_;
    String device_token_;
    uint32_t upload_interval_seconds_ = Config::AURA_LINK_DEFAULT_UPLOAD_INTERVAL_SECONDS;
    uint32_t offline_after_seconds_ = Config::AURA_LINK_DEFAULT_OFFLINE_AFTER_SECONDS;
    String firmware_channel_ = "stable";
    bool auto_ota_enabled_ = false;
    uint32_t sequence_ = 0;
    uint32_t next_upload_due_ms_ = 0;
    uint32_t last_upload_success_ms_ = 0;
    uint32_t last_upload_attempt_ms_ = 0;
    ClaimStatus claim_status_ = ClaimStatus::Idle;

    TaskHandle_t worker_task_ = nullptr;
    portMUX_TYPE worker_mux_ = portMUX_INITIALIZER_UNLOCKED;
    WorkerCommand worker_command_{};
    WorkerResult worker_result_{};
    WorkerCommand inflight_ingest_command_{};
    WorkerCommand retry_ingest_command_{};
    bool worker_command_pending_ = false;
    bool worker_result_ready_ = false;
    bool worker_busy_ = false;
    bool inflight_ingest_valid_ = false;
    bool retry_ingest_pending_ = false;
    AlertSnapshot alert_candidate_state_{};
    AlertSnapshot stable_alert_state_{};
    AlertSnapshot reported_alert_state_{};
    uint32_t alert_candidate_since_ms_ = 0;
    uint32_t last_urgent_upload_ms_ = 0;
    uint32_t urgent_upload_window_started_ms_ = 0;
    uint8_t urgent_uploads_in_window_ = 0;
};
