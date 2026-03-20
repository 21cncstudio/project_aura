// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "modules/DacAutoConfig.h"
#include "drivers/Gp8403.h"

struct SensorData;

class FanControl {
public:
    enum class Mode : uint8_t {
        Manual = 0,
        Auto = 1,
    };

    enum class InitStatus : uint8_t {
        Ok = 0,
        Absent,
        Fault,
    };

    struct Snapshot {
        bool available = false;
        bool running = false;
        bool faulted = false;
        bool output_known = true;
        bool manual_override_active = false;
        bool auto_resume_blocked = false;
        Mode mode = Mode::Manual;
        uint8_t manual_step = 1;
        uint32_t selected_timer_s = 0;
        uint16_t output_mv = 0;
        uint32_t stop_at_ms = 0;
        DacAutoConfig auto_config{};
    };

    void begin(bool auto_mode_preference, bool auto_armed_preference);
    void poll(uint32_t now_ms, const SensorData *sensor_data, bool gas_warmup);

    void setMode(Mode mode);
    void setManualStep(uint8_t step);
    void setTimerSeconds(uint32_t seconds);
    void requestStart();
    void requestStop();
    void requestAutoStart();
    void setAutoConfig(const DacAutoConfig &config);
    DacAutoConfig autoConfig() const;

    bool isAvailable() const;
    bool isRunning() const;
    bool isFaulted() const;
    bool isOutputKnown() const;
    bool isManualOverrideActive() const;
    bool isAutoResumeBlocked() const;
    Mode mode() const;
    uint8_t manualStep() const;
    uint32_t selectedTimerSeconds() const;
    uint16_t outputMillivolts() const;
    uint8_t outputPercent() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    Snapshot snapshot() const;

private:
    struct PendingCommands {
        bool has_mode = false;
        Mode mode = Mode::Manual;
        bool has_manual_step = false;
        uint8_t manual_step = 1;
        bool has_timer_seconds = false;
        uint32_t timer_seconds = 0;
        enum class StartStopRequest : uint8_t {
            None = 0,
            Start,
            Stop,
            AutoStart,
        };
        StartStopRequest start_stop_request = StartStopRequest::None;
        bool has_auto_config = false;
        DacAutoConfig auto_config{};
    };

    void ensureSyncPrimitives();
    bool lockSync() const;
    void unlockSync() const;
    void drainPendingCommands(PendingCommands &out);
    void publishSnapshot();
    void applyMode(Mode mode);
    void applyManualStep(uint8_t step);
    void applyTimerSeconds(uint32_t seconds);
    void applyRequestStart();
    void applyRequestStop();
    void applyRequestAutoStart();
    void applyAutoConfig(const DacAutoConfig &config);
    InitStatus tryInitialize(uint32_t now_ms, const char *&failure_reason);
    bool applyOutputMillivolts(uint16_t millivolts);
    void handleDacFault(const char *reason);
    void applyStopState(bool output_known);
    uint16_t stepToMillivolts(uint8_t step) const;
    uint16_t percentToMillivolts(uint8_t percent) const;
    uint8_t evaluateAutoDemandPercent(const SensorData &data, bool gas_warmup) const;
    static uint8_t maxPercent(uint8_t a, uint8_t b) { return (a > b) ? a : b; }

    Gp8403 dac_;
    DacAutoConfig auto_config_{};
    Mode mode_ = Mode::Manual;
    uint8_t manual_step_ = 1;
    uint32_t selected_timer_s_ = 0;
    bool start_requested_ = false;
    bool stop_requested_ = false;
    bool available_ = false;
    bool running_ = false;
    bool faulted_ = false;
    bool output_known_ = true;
    bool manual_override_active_ = false;
    uint16_t output_mv_ = 0;
    uint32_t stop_at_ms_ = 0;
    bool manual_step_update_pending_ = false;
    bool timer_update_pending_ = false;
    uint32_t last_recover_attempt_ms_ = 0;
    uint32_t last_health_check_ms_ = 0;
    uint8_t health_probe_fail_count_ = 0;
    bool boot_missing_lockout_ = false;
    bool auto_resume_blocked_ = false;
    bool boot_auto_resume_pending_ = false;
    uint32_t boot_auto_resume_due_ms_ = 0;

    mutable SemaphoreHandle_t sync_mutex_ = nullptr;
    PendingCommands pending_commands_{};
    Snapshot snapshot_{};
};
