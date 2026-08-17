// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include "core/CooperativeStart.h"

class Sfa40 {
public:
    enum class Status : uint8_t {
        Absent = 0,
        Ok,
        Fault,
    };

    enum class SelfTestStatus : uint8_t {
        Idle = 0,
        Running,
        Passed,
        Failed,
        ReadError,
    };

    struct Diagnostics {
        bool measuring = false;
        bool measurement_state_unknown = false;
        bool data_valid = false;
        bool has_new_data = false;
        bool warmup_active = false;
        bool selftest_active = false;
        Status status = Status::Absent;
        const char *last_error = "unknown";

        bool serial_valid = false;
        uint16_t serial_words[3] = {};

        uint32_t start_ms = 0;
        uint32_t first_ready_ms = 0;
        uint32_t first_within_spec_ms = 0;
        uint32_t last_measurement_ms = 0;
        uint32_t last_data_ms = 0;

        uint32_t measurement_reads = 0;
        uint32_t measurement_frames_ok = 0;
        uint32_t measurement_data_ok = 0;
        uint32_t measurement_read_failures = 0;
        uint32_t read_command_errors = 0;
        uint32_t read_bytes_errors = 0;
        uint32_t read_crc_errors = 0;
        uint32_t status_not_ready_count = 0;
        uint32_t status_not_within_spec_count = 0;
        uint32_t status_invalid_count = 0;

        bool last_status_valid = false;
        uint16_t raw_hcho = 0;
        uint16_t raw_humidity = 0;
        uint16_t raw_temperature = 0;
        uint8_t status_byte = 0;
        uint8_t status_reserved = 0;
        float hcho_ppb = 0.0f;
        float humidity_percent = 0.0f;
        float temperature_c = 0.0f;
    };

    bool begin();
    void start();
    void beginLateStart();
    CooperativeStart::Result pollLateStart(uint32_t now_ms);
    bool isLateStartActive() const { return late_start_phase_ != LateStartPhase::Idle; }
    bool stop();
    bool readData(float &hcho_ppb);
    bool startSelfTest();
    SelfTestStatus readSelfTestStatus(uint16_t &raw_result);
    void poll();
    bool isDataValid() const { return data_valid_; }
    bool isOk() const { return status_ == Status::Ok; }
    bool isPresent() const { return status_ != Status::Absent; }
    bool hasFault() const { return status_ == Status::Fault; }
    Status status() const { return status_; }
    bool isWarmupActive() const { return warmup_active_; }
    bool shouldFallbackToSfa30() const;
    const char *label() const { return "SFA40"; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    bool takeNewData(float &hcho_ppb);
    void invalidate();
    Diagnostics diagnostics() const;

private:
    enum class LateStartPhase : uint8_t {
        Idle = 0,
        Ping,
        StopBeforeDetect,
        WaitStopBeforeDetect,
        WriteId,
        ReadId,
        StopBeforeStart,
        WaitStopBeforeStart,
        WriteStart,
        WaitStart,
    };

    CooperativeStart::Result finishLateStart(bool success, uint32_t now_ms);
    enum class ErrorCause : uint8_t {
        None = 0,
        DetectSensor,
        WarmRestartStop,
        StartCommand,
        ReadCommand,
        ReadBytes,
        ReadCrc,
        ReadStatus,
    };

    struct MeasurementReadResult {
        bool status_valid = false;
        bool warmup_active = false;
        float hcho_ppb = 0.0f;
    };

    bool readMeasurement(MeasurementReadResult &result);
    bool detectSensor();
    bool readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms);
    bool ensureIdleBeforeDetect();
    bool ensureIdleBeforeStart();
    bool pingAddress();
    bool writeCmd(uint16_t cmd);
    bool readBytes(uint8_t *buf, size_t len);
    const char *errorCauseLabel() const;

    bool ok_ = false;
    bool measuring_ = false;
    bool measurement_state_unknown_ = false;
    bool data_valid_ = false;
    bool has_new_data_ = false;
    float last_hcho_ppb_ = 0.0f;
    uint32_t last_poll_ms_ = 0;
    uint32_t next_measurement_read_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint8_t fail_count_ = 0;
    Status status_ = Status::Absent;
    ErrorCause last_error_cause_ = ErrorCause::None;
    bool warmup_active_ = false;
    bool selftest_active_ = false;
    bool serial_valid_ = false;
    uint16_t serial_words_[3] = {};
    uint32_t start_ms_ = 0;
    uint32_t first_ready_ms_ = 0;
    uint32_t first_within_spec_ms_ = 0;
    uint32_t last_measurement_ms_ = 0;
    uint32_t measurement_reads_ = 0;
    uint32_t measurement_frames_ok_ = 0;
    uint32_t measurement_data_ok_ = 0;
    uint32_t measurement_read_failures_ = 0;
    uint32_t read_command_errors_ = 0;
    uint32_t read_bytes_errors_ = 0;
    uint32_t read_crc_errors_ = 0;
    uint32_t status_not_ready_count_ = 0;
    uint32_t status_not_within_spec_count_ = 0;
    uint32_t status_invalid_count_ = 0;
    bool last_status_valid_ = false;
    uint16_t last_raw_hcho_ = 0;
    uint16_t last_raw_humidity_ = 0;
    uint16_t last_raw_temperature_ = 0;
    uint8_t last_status_byte_ = 0;
    uint8_t last_status_reserved_ = 0;
    float last_humidity_percent_ = 0.0f;
    float last_temperature_c_ = 0.0f;
    LateStartPhase late_start_phase_ = LateStartPhase::Idle;
    uint32_t late_start_due_ms_ = 0;
    uint32_t late_start_command_ms_ = 0;
};
