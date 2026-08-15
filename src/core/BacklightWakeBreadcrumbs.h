// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BacklightWakeBreadcrumbs {

enum class Event : uint8_t {
    None = 0,
    ScheduleWake = 1,
    TouchWake,
    AlarmWake,
};

enum class Stage : uint8_t {
    None = 0,
    Request,
    DriverCallBegin,
    DriverCallReturned,
    WakeProbeUpdateBegin,
    WakeProbeUpdateReturned,
    LvglActivityBegin,
    LvglActivityReturned,
    CommandReturned,
    Completed,
    // Appended to preserve the numeric meaning of retained v1 records.
    TouchIrqMaskBegin,
    TouchIrqMaskReturned,
    PowerSettleBegin,
    PowerSettleReturned,
};

enum class DriverResult : uint8_t {
    Unknown = 0,
    Skipped,
    Succeeded,
    Failed,
};

enum class CaptureStatus : uint8_t {
    PowerLost = 0,
    Empty,
    Active,
    Completed,
    Corrupt,
};

struct LineState {
    bool valid = false;
    bool sda_high = false;
    bool scl_high = false;
};

struct Trace {
    uint32_t sequence = 0;
    uint32_t uptime_ms = 0;
    uint32_t epoch_s = 0;
    uint32_t driver_duration_us = 0;
    uint32_t expected_network_manager_addr = 0;
    uint32_t post_backlight_network_manager_addr = 0;
    uint32_t pre_render_network_manager_addr = 0;
    uint32_t post_backlight_task_handle = 0;
    uint32_t pre_render_task_handle = 0;
    Event event = Event::None;
    Stage stage = Stage::None;
    DriverResult driver_result = DriverResult::Unknown;
    bool target_on = false;
    bool previous_on = false;
    LineState before{};
    LineState after_driver{};
    LineState after_wake_probe{};
};

struct BootSnapshot {
    CaptureStatus status = CaptureStatus::Empty;
    bool has_trace = false;
    Trace trace{};
};

inline bool shouldTraceScheduleWake(bool clock_evaluation,
                                    bool was_sleep_window_active,
                                    bool is_sleep_window_active) {
    return clock_evaluation && was_sleep_window_active &&
           !is_sleep_window_active;
}

inline Event selectDarkWakeEvent(bool touch_wake, bool alarm_wake) {
    if (alarm_wake) {
        return Event::AlarmWake;
    }
    return touch_wake ? Event::TouchWake : Event::None;
}

// Capture retained state before a new boot starts using the trace. A cold power
// start invalidates RTC slow-memory contents; warm resets preserve them.
void initializeAtBoot(bool cold_power_start);
const BootSnapshot &bootSnapshot();

// Acknowledge only after setup has completed without scheduling another
// recovery reboot. The RAM boot snapshot remains available to diagnostics.
void acknowledgeBootSnapshot();

void beginWake(Event event,
               uint32_t uptime_ms,
               uint32_t epoch_s,
               bool target_on,
               bool previous_on,
               LineState before);
void markTouchIrqMaskBegin();
void markTouchIrqMaskReturned();
void markDriverCallBegin();
void markDriverCallReturned(bool succeeded,
                            bool skipped,
                            uint32_t duration_us,
                            LineState after);
void markPowerSettleBegin();
void markPowerSettleReturned();
void markWakeProbeUpdateBegin();
void markWakeProbeUpdateReturned(LineState after);
void markLvglActivityBegin();
void markLvglActivityReturned();
void markCommandReturned();
void markCompleted();
void markUiPostBacklightContext(uint32_t expected_network_manager_addr,
                                uint32_t actual_network_manager_addr,
                                uint32_t task_handle);
void markUiPreRenderContext(uint32_t expected_network_manager_addr,
                            uint32_t actual_network_manager_addr,
                            uint32_t task_handle);

const char *statusText(CaptureStatus status);
const char *eventText(Event event);
const char *stageText(Stage stage);
const char *driverResultText(DriverResult result);

#if defined(UNIT_TEST)
namespace test {
void resetRetained();
void corruptRetained();
void corruptLatestRecord();
size_t retainedRecordSize();
}
#endif

} // namespace BacklightWakeBreadcrumbs
