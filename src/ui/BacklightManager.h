// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <atomic>
#include <Arduino.h>
#include "config/AppConfig.h"
#include "core/BacklightWakeBreadcrumbs.h"
#include "core/BacklightStatePolicy.h"

namespace esp_panel {
namespace drivers {
class Backlight;
} // namespace drivers
} // namespace esp_panel

class StorageManager;

class BacklightManager {
public:
    enum class RequestStatus : uint8_t {
        Applied = 0,
        Queued,
        Rejected,
    };

    struct RequestResult {
        RequestStatus status = RequestStatus::Rejected;
        bool actual_on = false;
        bool target_on = false;

        bool accepted() const { return status != RequestStatus::Rejected; }
        bool pending() const { return status == RequestStatus::Queued; }
    };

    struct RuntimeSnapshot {
        bool actual_on = false;
        bool transition_pending = false;
        bool target_on = false;
        bool wake_critical_section_pending = false;
    };

    void loadFromPrefs(StorageManager &storage);
    void attachBacklight(esp_panel::drivers::Backlight *backlight);
    // Startup only: called after the logo framebuffer's VSYNC acknowledgement.
    // The first owner poll queues the ordinary guarded wake, without I2C here.
    void markStartupFrameReady();
    void poll(bool lvgl_ready);
    void updateUi();
    void savePrefs(StorageManager &storage);

    RequestResult requestState(
        bool on,
        BacklightWakeBreadcrumbs::Event source,
        uint32_t now_ms);
    void armWakeCompletionAfterRender();
    bool completeWakeAfterRender();
    bool releaseWakeAfterRuntimePublish();
    void disableSharedBus();
    bool waitForSharedBusWriterIdle(uint32_t timeout_ms);
    bool prepareSuppressedLvglAbortAfterDrain();
    bool waitForSharedBusIdle(uint32_t timeout_ms);
    RuntimeSnapshot runtimeSnapshot() const;
    bool isOn() const { return runtimeSnapshot().actual_on; }
    bool isTransitionPending() const;
    bool isWakeCriticalSectionPending() const;
    bool targetOn() const;
    uint32_t commandFailureCount() const { return command_failure_count_; }

    void setTimeoutMs(uint32_t timeout_ms);
    void setScheduleEnabled(bool enabled);
    void setAlarmWakeEnabled(bool enabled);
    void setAlarmWakeActive(bool active);
    void adjustSleepHour(int delta);
    void adjustSleepMinute(int delta);
    void adjustWakeHour(int delta);
    void adjustWakeMinute(int delta);

    void markUiDirty() { ui_dirty_ = true; }
    bool isUiDirty() const { return ui_dirty_; }
    bool isPresetSyncing() const { return preset_syncing_; }
    bool isScheduleSyncing() const { return schedule_syncing_; }
    bool isAlarmWakeSyncing() const { return alarm_wake_syncing_; }
    bool isScheduleEnabled() const { return schedule_enabled_; }
    bool isAlarmWakeEnabled() const { return alarm_wake_enabled_; }
    bool hasPrefsDirty() const { return prefs_dirty_; }

private:
    uint32_t normalizeTimeoutMs(uint32_t timeout_ms) const;
    bool setOnWithGateHeld(
        bool on,
        BacklightWakeBreadcrumbs::Event trace_event =
            BacklightWakeBreadcrumbs::Event::None,
        bool *driver_attempted = nullptr);
    void storeSchedulePrefs();
    void refreshSchedule();
    void refreshScheduleWithGateHeld(bool trace_clock_transition = false);
    bool requestGuardedWake(BacklightWakeBreadcrumbs::Event event, uint32_t now_ms);
    bool processGuardedWake(uint32_t now_ms);
    void finalizeGuardedWake(uint32_t now_ms);
    void cancelGuardedWake(bool defer_guard_release = false);
    BacklightStatePolicy::SuppressedAbortOffOutcome
    abortWakeAfterWriterQuiescence(bool defer_guard_release,
                                   bool turn_backlight_off);
    bool finalizeDisabledSharedBus(bool defer_guard_release,
                                   bool turn_backlight_off);
    void consumeInput();
    bool isTransitionPendingInternal() const;
    bool targetOnInternal() const;
    void publishRuntimeSnapshot();

    class RuntimeSnapshotPublishGuard {
    public:
        explicit RuntimeSnapshotPublishGuard(BacklightManager &owner)
            : owner_(owner) {}
        ~RuntimeSnapshotPublishGuard() { owner_.publishRuntimeSnapshot(); }

        RuntimeSnapshotPublishGuard(const RuntimeSnapshotPublishGuard &) = delete;
        RuntimeSnapshotPublishGuard &operator=(
            const RuntimeSnapshotPublishGuard &) = delete;

    private:
        BacklightManager &owner_;
    };

    static constexpr uint8_t RUNTIME_ACTUAL_ON_BIT = 1U << 0;
    static constexpr uint8_t RUNTIME_TRANSITION_PENDING_BIT = 1U << 1;
    static constexpr uint8_t RUNTIME_TARGET_ON_BIT = 1U << 2;
    static constexpr uint8_t RUNTIME_WAKE_CRITICAL_BIT = 1U << 3;

    esp_panel::drivers::Backlight *panel_backlight_ = nullptr;
    bool backlight_on_ = false;
    bool startup_pending_ = false;
    std::atomic<bool> startup_frame_ready_{false};
    std::atomic<uint8_t> runtime_snapshot_bits_{0};
    uint32_t command_failure_count_ = 0;
    BacklightStatePolicy::PendingCommand pending_command_;
    BacklightStatePolicy::RuntimeGate runtime_gate_;
    uint32_t backlight_timeout_ms_ = 0;
    bool schedule_enabled_ = false;
    bool alarm_wake_enabled_ = false;
    bool alarm_wake_active_ = false;
    bool schedule_active_ = false;
    BacklightWakeBreadcrumbs::Event pending_wake_event_ =
        BacklightWakeBreadcrumbs::Event::None;
    BacklightWakeBreadcrumbs::Event guarded_wake_event_ =
        BacklightWakeBreadcrumbs::Event::None;
    bool guarded_wake_pending_ = false;
    bool guarded_wake_wait_exceeded_ = false;
    bool guarded_wake_settle_pending_ = false;
    bool guarded_wake_settle_succeeded_ = false;
    bool guarded_wake_render_pending_ = false;
    bool guarded_wake_render_command_succeeded_ = false;
    bool guarded_wake_render_armed_ = false;
    bool guarded_wake_release_pending_ = false;
    bool guarded_wake_release_requires_cancel_ = false;
    bool guarded_wake_release_permitted_ = false;
    uint32_t guarded_wake_render_handler_baseline_ = 0;
    uint32_t guarded_wake_started_ms_ = 0;
    int sleep_hour_ = 23;
    int sleep_minute_ = 0;
    int wake_hour_ = 6;
    int wake_minute_ = 0;
    uint32_t schedule_boot_grace_until_ms_ = 0;
    uint32_t last_inactive_ms_ = 0;
    uint32_t block_input_until_ms_ = 0;
    bool ui_dirty_ = true;
    bool preset_syncing_ = false;
    bool schedule_syncing_ = false;
    bool alarm_wake_syncing_ = false;
    bool prefs_dirty_ = false;
};
