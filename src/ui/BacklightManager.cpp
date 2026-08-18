// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "BacklightManager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_display_panel.hpp>
#include "modules/StorageManager.h"
#include "core/BacklightWakeBreadcrumbs.h"
#include "core/I2cBusRecovery.h"
#include "core/Logger.h"
#include "core/WakePowerGuard.h"
#include "lvgl_v8_port.h"
#include "ui/ui.h"

namespace {

void safe_label_set_text(lv_obj_t *obj, const char *new_text) {
    if (!obj) {
        return;
    }
    const char *current = lv_label_get_text(obj);
    if (current && strcmp(current, new_text) == 0) {
        return;
    }
    lv_label_set_text(obj, new_text);
}

int wrap_value(int value, int modulo) {
    if (modulo == 0) {
        return value;
    }
    value %= modulo;
    if (value < 0) {
        value += modulo;
    }
    return value;
}

bool is_sleep_window(int sleep_hour, int sleep_minute, int wake_hour, int wake_minute,
                     const tm &local_tm) {
    int now_min = local_tm.tm_hour * 60 + local_tm.tm_min;
    int sleep_min = sleep_hour * 60 + sleep_minute;
    int wake_min = wake_hour * 60 + wake_minute;
    if (sleep_min == wake_min) {
        return false;
    }
    if (sleep_min < wake_min) {
        return now_min >= sleep_min && now_min < wake_min;
    }
    return now_min >= sleep_min || now_min < wake_min;
}

BacklightWakeBreadcrumbs::LineState sample_shared_i2c_lines() {
    const I2cBusRecovery::LineState lines = I2cBusRecovery::sample(
        static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
        static_cast<gpio_num_t>(Config::I2C_SCL_PIN));
    return BacklightWakeBreadcrumbs::LineState{
        true,
        lines.sda_high,
        lines.scl_high,
    };
}

uint8_t wake_event_priority(BacklightWakeBreadcrumbs::Event event) {
    switch (event) {
        case BacklightWakeBreadcrumbs::Event::AlarmWake: return 5;
        case BacklightWakeBreadcrumbs::Event::TouchWake: return 4;
        case BacklightWakeBreadcrumbs::Event::ScheduleWake: return 3;
        case BacklightWakeBreadcrumbs::Event::MqttWake: return 2;
        case BacklightWakeBreadcrumbs::Event::WebWake: return 1;
        case BacklightWakeBreadcrumbs::Event::None: return 0;
    }
    return 0;
}

} // namespace

void BacklightManager::loadFromPrefs(StorageManager &storage) {
    const auto &cfg = storage.config();
    uint32_t timeout_s = cfg.backlight_timeout_s;
    backlight_timeout_ms_ = normalizeTimeoutMs(timeout_s * 1000UL);
    schedule_enabled_ = cfg.backlight_schedule_enabled;
    alarm_wake_enabled_ = cfg.backlight_alarm_wake;
    sleep_hour_ = cfg.backlight_sleep_hour;
    sleep_minute_ = cfg.backlight_sleep_minute;
    wake_hour_ = cfg.backlight_wake_hour;
    wake_minute_ = cfg.backlight_wake_minute;

    if (sleep_hour_ < 0 || sleep_hour_ > 23) sleep_hour_ = 23;
    if (wake_hour_ < 0 || wake_hour_ > 23) wake_hour_ = 6;
    if (sleep_minute_ < 0 || sleep_minute_ > 59) sleep_minute_ = 0;
    if (wake_minute_ < 0 || wake_minute_ > 59) wake_minute_ = 0;

    prefs_dirty_ = false;
    ui_dirty_ = true;
}

void BacklightManager::attachBacklight(esp_panel::drivers::Backlight *backlight) {
    auto bus_access = runtime_gate_.acquire();
    if (!bus_access) {
        return;
    }
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    cancelGuardedWake();
    panel_backlight_ = backlight;
    backlight_on_ = panel_backlight_ != nullptr;
    schedule_boot_grace_until_ms_ = millis() + Config::BACKLIGHT_BOOT_GRACE_MS;
    lvgl_port_set_wake_touch_probe(!backlight_on_);
}

uint32_t BacklightManager::normalizeTimeoutMs(uint32_t timeout_ms) const {
    if (timeout_ms == Config::BACKLIGHT_TIMEOUT_30S ||
        timeout_ms == Config::BACKLIGHT_TIMEOUT_1M) {
        return timeout_ms;
    }
    if (timeout_ms > 0) {
        return Config::BACKLIGHT_TIMEOUT_1M;
    }
    return 0;
}

void BacklightManager::setTimeoutMs(uint32_t timeout_ms) {
    timeout_ms = normalizeTimeoutMs(timeout_ms);
    if (backlight_on_) {
        // Start a newly selected preset from this interaction instead of
        // inheriting an older LVGL inactivity interval.
        lv_disp_trig_activity(nullptr);
        last_inactive_ms_ = 0;
    }
    if (timeout_ms == backlight_timeout_ms_) {
        return;
    }
    backlight_timeout_ms_ = timeout_ms;
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

BacklightManager::RequestResult BacklightManager::requestState(
    bool on,
    BacklightWakeBreadcrumbs::Event source,
    uint32_t now_ms) {
    RequestResult result{};
    const RuntimeSnapshot published = runtimeSnapshot();
    result.actual_on = published.actual_on;
    result.target_on = published.target_on;
    auto bus_access = runtime_gate_.acquire();
    if (!bus_access || !panel_backlight_) {
        return result;
    }
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    result.actual_on = backlight_on_;
    result.target_on = targetOnInternal();

    if (guarded_wake_settle_pending_ || guarded_wake_render_pending_) {
        if (on && targetOnInternal()) {
            result.status = RequestStatus::Queued;
            result.target_on = true;
        }
        return result;
    }

    const BacklightStatePolicy::CommandRoute command_route =
        BacklightStatePolicy::route(backlight_on_, on);
    if (command_route == BacklightStatePolicy::CommandRoute::GuardedOn) {
        if (!requestGuardedWake(source, now_ms)) {
            return result;
        }
        result.status = RequestStatus::Queued;
        result.target_on = true;
        return result;
    }

    if (!on) {
        cancelGuardedWake();
    }
    const bool accepted = setOnWithGateHeld(on);
    result.actual_on = backlight_on_;
    result.target_on = targetOnInternal();
    if (accepted) {
        result.status = RequestStatus::Applied;
    } else if (pending_command_.active() &&
               pending_command_.targetOn() == on) {
        result.status = RequestStatus::Queued;
    } else {
        result.status = RequestStatus::Rejected;
    }
    return result;
}

BacklightManager::RuntimeSnapshot BacklightManager::runtimeSnapshot() const {
    const uint8_t bits = runtime_snapshot_bits_.load(std::memory_order_acquire);
    return RuntimeSnapshot{
        (bits & RUNTIME_ACTUAL_ON_BIT) != 0U,
        (bits & RUNTIME_TRANSITION_PENDING_BIT) != 0U,
        (bits & RUNTIME_TARGET_ON_BIT) != 0U,
        (bits & RUNTIME_WAKE_CRITICAL_BIT) != 0U,
    };
}

bool BacklightManager::isTransitionPending() const {
    return runtimeSnapshot().transition_pending;
}

bool BacklightManager::isTransitionPendingInternal() const {
    return guarded_wake_pending_ || guarded_wake_settle_pending_ ||
           guarded_wake_render_pending_ || pending_command_.active();
}

bool BacklightManager::isWakeCriticalSectionPending() const {
    return runtimeSnapshot().wake_critical_section_pending;
}

bool BacklightManager::targetOn() const {
    return runtimeSnapshot().target_on;
}

bool BacklightManager::targetOnInternal() const {
    if (guarded_wake_pending_ || guarded_wake_settle_pending_ ||
        guarded_wake_render_pending_) {
        return true;
    }
    return pending_command_.active() ? pending_command_.targetOn()
                                     : backlight_on_;
}

void BacklightManager::publishRuntimeSnapshot() {
    uint8_t bits = backlight_on_ ? RUNTIME_ACTUAL_ON_BIT : 0U;
    if (isTransitionPendingInternal()) {
        bits |= RUNTIME_TRANSITION_PENDING_BIT;
    }
    if (targetOnInternal()) {
        bits |= RUNTIME_TARGET_ON_BIT;
    }
    if (guarded_wake_settle_pending_ || guarded_wake_render_pending_) {
        bits |= RUNTIME_WAKE_CRITICAL_BIT;
    }
    runtime_snapshot_bits_.store(bits, std::memory_order_release);
}

bool BacklightManager::requestGuardedWake(
    BacklightWakeBreadcrumbs::Event event,
    uint32_t now_ms) {
    if (backlight_on_) {
        return true;
    }
    if (!panel_backlight_ || !runtime_gate_.available() ||
        event == BacklightWakeBreadcrumbs::Event::None) {
        LOGE("Backlight", "guarded wake rejected: source or driver unavailable");
        return false;
    }

    if (!guarded_wake_pending_) {
        if (guarded_wake_settle_pending_) {
            return true;
        }
        guarded_wake_pending_ = true;
        guarded_wake_wait_exceeded_ = false;
        guarded_wake_started_ms_ = now_ms;
        guarded_wake_event_ = event;
        BacklightWakeBreadcrumbs::beginPreQuietWake(
            event,
            now_ms,
            static_cast<uint32_t>(time(nullptr)),
            true,
            backlight_on_,
            sample_shared_i2c_lines());
        if (!WakePowerGuard::request(now_ms)) {
            guarded_wake_pending_ = false;
            guarded_wake_started_ms_ = 0;
            guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
            BacklightWakeBreadcrumbs::markAborted();
            return false;
        }
        return true;
    }

    if (wake_event_priority(event) > wake_event_priority(guarded_wake_event_)) {
        guarded_wake_event_ = event;
        BacklightWakeBreadcrumbs::updateWakeEvent(event);
    }
    return true;
}

bool BacklightManager::processGuardedWake(uint32_t now_ms) {
    if (!guarded_wake_pending_) {
        return false;
    }
    if (backlight_on_) {
        BacklightWakeBreadcrumbs::markAborted();
        guarded_wake_pending_ = false;
        guarded_wake_wait_exceeded_ = false;
        guarded_wake_started_ms_ = 0;
        guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
        WakePowerGuard::cancel();
        return false;
    }

    if (WakePowerGuard::phase(now_ms) == WakePowerGuard::Phase::Idle) {
        // The guard has a defensive failsafe so a lost UI owner cannot pause
        // background work forever. A live owner rearms the quiet window and
        // still waits for tracked work to drain before touching CH422G.
        if (!WakePowerGuard::request(now_ms)) {
            return true;
        }
    }
    const WakePowerGuard::SwitchDecision switch_decision =
        WakePowerGuard::evaluateSwitch(
            now_ms,
            Config::BACKLIGHT_WAKE_PRE_QUIET_MIN_MS,
            Config::BACKLIGHT_WAKE_PRE_QUIET_WARN_MS);
    const uint32_t total_pre_quiet_elapsed_ms =
        static_cast<uint32_t>(now_ms - guarded_wake_started_ms_);
    if (switch_decision.wait_exceeded && !guarded_wake_wait_exceeded_) {
        guarded_wake_wait_exceeded_ = true;
        BacklightWakeBreadcrumbs::markPreQuietWaitExceeded(
            total_pre_quiet_elapsed_ms,
            switch_decision.active_operations);
        LOGW("Backlight",
             "guarded wake still waiting after %lu ms (%lu tracked operations active)",
             static_cast<unsigned long>(switch_decision.elapsed_ms),
             static_cast<unsigned long>(switch_decision.active_operations));
    }
    if (!switch_decision.ready) {
        return true;
    }
    if (!WakePowerGuard::beginSwitch()) {
        return true;
    }

    const BacklightWakeBreadcrumbs::Event wake_event = guarded_wake_event_;
    BacklightWakeBreadcrumbs::markPreQuietReady(
        total_pre_quiet_elapsed_ms, sample_shared_i2c_lines());
    guarded_wake_pending_ = false;
    bool driver_attempted = false;
    const bool wake_succeeded = setOnWithGateHeld(
        true,
        wake_event,
        &driver_attempted);
    guarded_wake_wait_exceeded_ = false;
    guarded_wake_started_ms_ = 0;

    if (driver_attempted) {
        BacklightWakeBreadcrumbs::markCommandReturnedPendingSettle(
            wake_succeeded
                ? BacklightWakeBreadcrumbs::CommandResult::Succeeded
                : BacklightWakeBreadcrumbs::CommandResult::Failed);
        BacklightWakeBreadcrumbs::markGuardSettleBegin();
        const uint32_t switched_ms = millis();
        WakePowerGuard::beginSettle(switched_ms,
                                    Config::BACKLIGHT_WAKE_SETTLE_MS);
        guarded_wake_settle_pending_ = true;
        guarded_wake_settle_succeeded_ = wake_succeeded;
        block_input_until_ms_ = switched_ms + Config::BACKLIGHT_WAKE_BLOCK_MS;
        lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
        consumeInput();
        pending_wake_event_ =
            !wake_succeeded && pending_command_.active() &&
                    pending_command_.targetOn()
                ? wake_event
                : BacklightWakeBreadcrumbs::Event::None;
    } else {
        BacklightWakeBreadcrumbs::markCommandReturned(
            wake_succeeded
                ? BacklightWakeBreadcrumbs::CommandResult::Succeeded
                : BacklightWakeBreadcrumbs::CommandResult::Failed);
        WakePowerGuard::cancel();
        pending_wake_event_ =
            !wake_succeeded && pending_command_.active() &&
                    pending_command_.targetOn()
                ? wake_event
                : BacklightWakeBreadcrumbs::Event::None;
        if (wake_succeeded) {
            BacklightWakeBreadcrumbs::markCompleted();
        }
    }

    guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    return true;
}

void BacklightManager::finalizeGuardedWake(uint32_t now_ms) {
    if (!guarded_wake_settle_pending_ ||
        !WakePowerGuard::beginRenderWait(now_ms)) {
        return;
    }

    BacklightWakeBreadcrumbs::markGuardSettleReturned();
    if (backlight_on_) {
        // A successful CH422G write can physically enable the display even if
        // the later touch-probe synchronization fails. Keep background work
        // closed through the first post-wake render whenever the actual load
        // is on, while preserving the full command result for diagnostics.
        guarded_wake_render_pending_ = true;
        guarded_wake_render_command_succeeded_ =
            guarded_wake_settle_succeeded_;
        guarded_wake_render_armed_ = false;
    } else {
        BacklightWakeBreadcrumbs::markFailed();
        (void)WakePowerGuard::completeRenderWait();
    }
    guarded_wake_settle_pending_ = false;
    guarded_wake_settle_succeeded_ = false;
}

void BacklightManager::armWakeCompletionAfterRender() {
    if (!guarded_wake_render_pending_ || guarded_wake_render_armed_) {
        return;
    }
    lvgl_port_diagnostics_t diagnostics{};
    if (!lvgl_port_get_diagnostics(&diagnostics)) {
        return;
    }
    guarded_wake_render_handler_baseline_ = diagnostics.timer_handler_count;
    guarded_wake_render_armed_ = true;
}

bool BacklightManager::completeWakeAfterRender() {
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    if (!guarded_wake_render_pending_ || !guarded_wake_render_armed_) {
        return false;
    }
    lvgl_port_diagnostics_t diagnostics{};
    if (!lvgl_port_get_diagnostics(&diagnostics) ||
        diagnostics.timer_handler_count ==
            guarded_wake_render_handler_baseline_) {
        return false;
    }
    // UiController calls this while holding the LVGL mutex. A changed handler
    // counter plus ownership of the mutex proves that the first post-wake
    // lv_timer_handler (including any synchronous flush) returned cleanly.
    if (guarded_wake_render_command_succeeded_) {
        BacklightWakeBreadcrumbs::markCompleted();
    } else {
        BacklightWakeBreadcrumbs::markFailed();
    }
    guarded_wake_render_pending_ = false;
    guarded_wake_render_command_succeeded_ = false;
    guarded_wake_render_armed_ = false;
    guarded_wake_render_handler_baseline_ = 0;
    // Keep background admission closed until UiController has published this
    // final actual/pending/target tuple to both runtime snapshots. Otherwise
    // Core 0 can resume from RenderWait and retain one stale OFF state.
    guarded_wake_release_pending_ = true;
    guarded_wake_release_requires_cancel_ = false;
    guarded_wake_release_permitted_ = true;
    return true;
}

bool BacklightManager::releaseWakeAfterRuntimePublish() {
    if (!guarded_wake_release_pending_ ||
        !guarded_wake_release_permitted_) {
        return false;
    }
    if (guarded_wake_release_requires_cancel_) {
        WakePowerGuard::cancel();
    } else {
        if (!WakePowerGuard::completeRenderWait()) {
            return false;
        }
    }
    guarded_wake_release_pending_ = false;
    guarded_wake_release_requires_cancel_ = false;
    guarded_wake_release_permitted_ = false;
    return true;
}

void BacklightManager::cancelGuardedWake(bool defer_guard_release) {
    if (guarded_wake_settle_pending_) {
        // Once the CH422G write was attempted, the settle interval is a safety
        // boundary. Callers may defer their state change, but cannot reopen
        // background work early.
        return;
    }
    if (guarded_wake_pending_) {
        BacklightWakeBreadcrumbs::markAborted();
    } else if (guarded_wake_render_pending_) {
        if (guarded_wake_render_command_succeeded_) {
            BacklightWakeBreadcrumbs::markAborted();
        } else {
            BacklightWakeBreadcrumbs::markFailed();
        }
    }
    guarded_wake_pending_ = false;
    guarded_wake_wait_exceeded_ = false;
    guarded_wake_render_pending_ = false;
    guarded_wake_render_command_succeeded_ = false;
    guarded_wake_render_armed_ = false;
    guarded_wake_release_pending_ = defer_guard_release;
    guarded_wake_release_requires_cancel_ = defer_guard_release;
    // Suppressed recovery must explicitly prove the OFF outcome and pending
    // drain before the public release step is allowed to cancel FailClosed.
    guarded_wake_release_permitted_ = false;
    guarded_wake_render_handler_baseline_ = 0;
    guarded_wake_started_ms_ = 0;
    guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    pending_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    if (!defer_guard_release) {
        WakePowerGuard::cancel();
    }
}

BacklightStatePolicy::SuppressedAbortOffOutcome
BacklightManager::abortWakeAfterWriterQuiescence(
    bool defer_guard_release,
    bool turn_backlight_off) {
    using OffOutcome = BacklightStatePolicy::SuppressedAbortOffOutcome;
    OffOutcome off_outcome = OffOutcome::NotRequested;
    if (guarded_wake_settle_pending_) {
        // A failed display runtime cannot acknowledge the first post-wake
        // render. Still honour the electrical settle interval before opening
        // background admission, then terminate the retained trace explicitly.
        while (!WakePowerGuard::settleReady(millis())) {
            delay(1);
        }
        BacklightWakeBreadcrumbs::markGuardSettleReturned();
        if (guarded_wake_settle_succeeded_) {
            BacklightWakeBreadcrumbs::markAborted();
        } else {
            BacklightWakeBreadcrumbs::markFailed();
        }
        guarded_wake_settle_pending_ = false;
        guarded_wake_settle_succeeded_ = false;
    }
    if (WakePowerGuard::phase(millis()) == WakePowerGuard::Phase::Switching) {
        // This phase is not expected here because UiController owns the whole
        // synchronous switch. If a future caller changes that ownership,
        // preserve a conservative settle window before touching CH422G again.
        delay(Config::BACKLIGHT_WAKE_SETTLE_MS);
        BacklightWakeBreadcrumbs::markAborted();
    }
    if (turn_backlight_off) {
        if (panel_backlight_ != nullptr) {
            const bool off_succeeded = panel_backlight_->off();
            if (off_succeeded) {
                backlight_on_ = false;
                off_outcome = OffOutcome::OffConfirmed;
            } else {
                off_outcome = OffOutcome::DriverFailed;
                ++command_failure_count_;
                LOGE("Backlight",
                     "failed to turn backlight off while isolating unavailable LVGL");
            }
        } else {
            off_outcome = OffOutcome::DriverUnavailable;
            ++command_failure_count_;
            LOGE("Backlight",
                 "cannot confirm backlight off without an attached driver");
        }
    }
    cancelGuardedWake(defer_guard_release);
    return off_outcome;
}

bool BacklightManager::setOnWithGateHeld(
    bool on,
    BacklightWakeBreadcrumbs::Event trace_event,
    bool *driver_attempted) {
    if (driver_attempted) {
        *driver_attempted = false;
    }
    const bool trace_wake = trace_event != BacklightWakeBreadcrumbs::Event::None;

    if (!panel_backlight_) {
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markDriverCallBegin();
            BacklightWakeBreadcrumbs::markDriverCallReturned(
                false, true, 0, sample_shared_i2c_lines());
        }
        LOGE("Backlight", "cannot turn backlight %s: driver unavailable", on ? "on" : "off");
        return false;
    }
    if (on == backlight_on_) {
        const bool cancelled_retry = pending_command_.active();
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markDriverCallBegin();
            BacklightWakeBreadcrumbs::markDriverCallReturned(
                true, true, 0, sample_shared_i2c_lines());
            BacklightWakeBreadcrumbs::markWakeProbeUpdateBegin();
        }
        const bool wake_probe_updated = lvgl_port_set_wake_touch_probe(!on);
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markWakeProbeUpdateReturned(
                sample_shared_i2c_lines());
        }
        if (!wake_probe_updated) {
            ++command_failure_count_;
            pending_command_.recordFailure(on, millis());
            LOGE("Backlight", "failed to synchronize touch IRQ for unchanged %s state",
                 on ? "on" : "off");
            return false;
        }
        pending_command_.clear();
        if (on && cancelled_retry) {
            if (trace_wake) {
                BacklightWakeBreadcrumbs::markLvglActivityBegin();
            }
            lv_disp_trig_activity(nullptr);
            if (trace_wake) {
                BacklightWakeBreadcrumbs::markLvglActivityReturned();
            }
            last_inactive_ms_ = 0;
        }
        return true;
    }

    const bool previous_on = backlight_on_;
    // beginSwitch() is the admission barrier: it can enter Switching only after
    // observing zero admitted activity. A racing tryAcquireActivity() may
    // provisionally increment the counter, but its second phase check rejects
    // that lease before any work begins. Rechecking the counter here would turn
    // that harmless provisional increment into a false wake failure.
    if (!previous_on && on &&
        WakePowerGuard::phase(millis()) != WakePowerGuard::Phase::Switching) {
        ++command_failure_count_;
        pending_command_.recordFailure(on, millis());
        LOGE("Backlight",
             "blocked unsafe OFF-to-ON transition outside an idle pre-quiet guard");
        return false;
    }
    const BacklightStatePolicy::WakeProbePlan wake_probe_plan =
        BacklightStatePolicy::planWakeProbe(previous_on, on);
    if (wake_probe_plan.mask_before_driver) {
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markTouchIrqMaskBegin();
        }
        const bool touch_irq_masked = lvgl_port_set_wake_touch_probe(false);
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markTouchIrqMaskReturned();
        }
        if (!touch_irq_masked) {
            if (trace_wake) {
                BacklightWakeBreadcrumbs::markDriverCallReturned(
                    false, true, 0, sample_shared_i2c_lines());
            }
            ++command_failure_count_;
            pending_command_.recordFailure(on, millis());
            const uint32_t retry_delay_ms = BacklightStatePolicy::retryDelayMs(
                pending_command_.consecutiveFailures());
            LOGE("Backlight",
                 "touch IRQ could not be safely masked before backlight %s; driver skipped, retry in %lu ms",
                 on ? "on" : "off",
                 static_cast<unsigned long>(retry_delay_ms));
            return false;
        }
    }

    if (trace_wake) {
        BacklightWakeBreadcrumbs::markDriverCallBegin();
    }
    if (driver_attempted) {
        *driver_attempted = true;
    }
    const uint32_t driver_started_us = micros();
    const bool driver_succeeded = on ? panel_backlight_->on() : panel_backlight_->off();
    const uint32_t driver_duration_us = micros() - driver_started_us;
    if (trace_wake) {
        BacklightWakeBreadcrumbs::markDriverCallReturned(
            driver_succeeded, false, driver_duration_us,
            sample_shared_i2c_lines());
    }
    const BacklightStatePolicy::Transition transition =
        BacklightStatePolicy::resolve(previous_on, on, driver_succeeded);
    if (BacklightStatePolicy::needsPostDriverSettle(
            previous_on, on, true)) {
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markPowerSettleBegin();
        }
        // CH422G applies the complete backlight load in one step. Keep the
        // cross-core quiet gate closed before touch-I2C or LVGL work so the
        // display rail and cable-mounted bulk capacitor can settle.
        delay(Config::BACKLIGHT_WAKE_DRIVER_SETTLE_MS);
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markPowerSettleReturned();
        }
    }
    if (trace_wake) {
        BacklightWakeBreadcrumbs::markWakeProbeUpdateBegin();
    }
    backlight_on_ = transition.actual_on;
    const bool wake_probe_updated =
        lvgl_port_set_wake_touch_probe(transition.wake_probe_enabled);
    if (trace_wake) {
        BacklightWakeBreadcrumbs::markWakeProbeUpdateReturned(
            sample_shared_i2c_lines());
    }

    if (!transition.command_succeeded || !wake_probe_updated) {
        ++command_failure_count_;
        pending_command_.recordFailure(on, millis());
        const uint32_t retry_delay_ms =
            BacklightStatePolicy::retryDelayMs(pending_command_.consecutiveFailures());
        LOGE("Backlight",
             "failed to complete backlight %s transition (driver=%s touch_irq=%s failures=%lu); keeping state %s, retry in %lu ms",
             on ? "on" : "off",
             transition.command_succeeded ? "ok" : "failed",
             wake_probe_updated ? "ok" : "failed",
             static_cast<unsigned long>(command_failure_count_),
             backlight_on_ ? "on" : "off",
             static_cast<unsigned long>(retry_delay_ms));
        return false;
    }

    pending_command_.clear();
    if (on) {
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markLvglActivityBegin();
        }
        lv_disp_trig_activity(nullptr);
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markLvglActivityReturned();
        }
        last_inactive_ms_ = 0;
    }
    return true;
}

void BacklightManager::disableSharedBus() {
    if (!runtime_gate_.disable()) {
        return;
    }
    // Only close admission here. The LVGL task may still be finishing a
    // callback which already owns the runtime gate, so touching the guarded
    // wake or pending-command fields at this point would race that writer.
    // waitForSharedBusIdle() performs finalization after the caller has
    // quiesced LVGL and every admitted operation has released its lease.
    LOGW("Backlight", "shared I2C bus closing; new backlight commands disabled");
}

bool BacklightManager::waitForSharedBusIdle(uint32_t timeout_ms) {
    if (!waitForSharedBusWriterIdle(timeout_ms)) {
        return false;
    }
    return finalizeDisabledSharedBus(false, false);
}

bool BacklightManager::waitForSharedBusWriterIdle(uint32_t timeout_ms) {
    if (runtime_gate_.available()) {
        return false;
    }
    const uint32_t start_ms = millis();
    uint32_t now_ms = start_ms;
    while (!runtime_gate_.idle() &&
           !BacklightStatePolicy::drainWaitExpired(start_ms, now_ms, timeout_ms)) {
        delay(1);
        now_ms = millis();
    }

    if (!runtime_gate_.idle()) {
        return false;
    }
    return true;
}

bool BacklightManager::prepareSuppressedLvglAbortAfterDrain() {
    return finalizeDisabledSharedBus(true, true);
}

bool BacklightManager::finalizeDisabledSharedBus(
    bool defer_guard_release,
    bool turn_backlight_off) {
    if (runtime_gate_.available() || !runtime_gate_.idle()) {
        return false;
    }
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    const BacklightStatePolicy::SuppressedAbortOffOutcome off_outcome =
        abortWakeAfterWriterQuiescence(
            defer_guard_release, turn_backlight_off);
    const bool pending_cleared = BacklightStatePolicy::clearPendingAfterDrain(
        runtime_gate_, pending_command_);
    const bool release_permitted =
        BacklightStatePolicy::mayReleaseAfterSuppressedAbort(
            off_outcome, pending_cleared);
    if (defer_guard_release) {
        guarded_wake_release_permitted_ = release_permitted;
    }
    block_input_until_ms_ = 0;
    lvgl_port_set_wake_touch_probe(false);
    LOGW("Backlight", "shared I2C bus offline; backlight operations drained");
    return release_permitted;
}

void BacklightManager::storeSchedulePrefs() {
    prefs_dirty_ = true;
}

void BacklightManager::savePrefs(StorageManager &storage) {
    if (!prefs_dirty_) {
        return;
    }
    auto &cfg = storage.config();
    cfg.backlight_timeout_s = backlight_timeout_ms_ / 1000;
    cfg.backlight_schedule_enabled = schedule_enabled_;
    cfg.backlight_alarm_wake = alarm_wake_enabled_;
    cfg.backlight_sleep_hour = sleep_hour_;
    cfg.backlight_sleep_minute = sleep_minute_;
    cfg.backlight_wake_hour = wake_hour_;
    cfg.backlight_wake_minute = wake_minute_;
    if (storage.saveConfig(true)) {
        prefs_dirty_ = false;
    } else {
        storage.requestSave();
        LOGE("Backlight", "failed to persist backlight settings");
    }
}

void BacklightManager::setScheduleEnabled(bool enabled) {
    if (enabled == schedule_enabled_) {
        return;
    }
    schedule_enabled_ = enabled;
    prefs_dirty_ = true;
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::setAlarmWakeEnabled(bool enabled) {
    if (enabled == alarm_wake_enabled_) {
        return;
    }
    alarm_wake_enabled_ = enabled;
    prefs_dirty_ = true;
    ui_dirty_ = true;
}

void BacklightManager::setAlarmWakeActive(bool active) {
    alarm_wake_active_ = active;
}

void BacklightManager::adjustSleepHour(int delta) {
    sleep_hour_ = wrap_value(sleep_hour_ + delta, 24);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustSleepMinute(int delta) {
    sleep_minute_ = wrap_value(sleep_minute_ + delta, 60);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustWakeHour(int delta) {
    wake_hour_ = wrap_value(wake_hour_ + delta, 24);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::adjustWakeMinute(int delta) {
    wake_minute_ = wrap_value(wake_minute_ + delta, 60);
    storeSchedulePrefs();
    refreshSchedule();
    ui_dirty_ = true;
}

void BacklightManager::refreshSchedule() {
    auto bus_access = runtime_gate_.acquire();
    if (!bus_access) {
        return;
    }
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    refreshScheduleWithGateHeld();
}

void BacklightManager::refreshScheduleWithGateHeld(bool trace_clock_transition) {
    if (guarded_wake_settle_pending_ || guarded_wake_render_pending_) {
        return;
    }
    if (schedule_enabled_ && schedule_boot_grace_until_ms_ != 0) {
        if (static_cast<int32_t>(millis() - schedule_boot_grace_until_ms_) < 0) {
            return;
        }
        schedule_boot_grace_until_ms_ = 0;
    }

    bool active = false;
    if (schedule_enabled_) {
        time_t now = time(nullptr);
        if (now > Config::TIME_VALID_EPOCH) {
            tm local_tm = {};
            localtime_r(&now, &local_tm);
            active = is_sleep_window(sleep_hour_, sleep_minute_, wake_hour_, wake_minute_, local_tm);
        }
    }
    if (active != schedule_active_) {
        (void)trace_clock_transition;
        schedule_active_ = active;
        if (!active && !backlight_on_) {
            // A schedule-related OFF-to-ON transition always uses the same
            // guarded path, including a manual schedule edit or disable.
            requestGuardedWake(
                BacklightWakeBreadcrumbs::Event::ScheduleWake, millis());
            return;
        }
        if (active) {
            cancelGuardedWake();
        }
        (void)setOnWithGateHeld(!active);
    }
}

void BacklightManager::updateUi() {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", sleep_hour_);
    if (objects.label_backlight_sleep_hours_value) {
        safe_label_set_text(objects.label_backlight_sleep_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", sleep_minute_);
    if (objects.label_backlight_sleep_minutes_value) {
        safe_label_set_text(objects.label_backlight_sleep_minutes_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", wake_hour_);
    if (objects.label_backlight_wake_hours_value) {
        safe_label_set_text(objects.label_backlight_wake_hours_value, buf);
    }
    snprintf(buf, sizeof(buf), "%02d", wake_minute_);
    if (objects.label_backlight_wake_minutes_value) {
        safe_label_set_text(objects.label_backlight_wake_minutes_value, buf);
    }

    schedule_syncing_ = true;
    if (objects.btn_backlight_schedule_toggle) {
        if (schedule_enabled_) {
            lv_obj_add_state(objects.btn_backlight_schedule_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_backlight_schedule_toggle, LV_STATE_CHECKED);
        }
    }
    schedule_syncing_ = false;

    alarm_wake_syncing_ = true;
    if (objects.btn_backlight_alarm_wake) {
        if (alarm_wake_enabled_) {
            lv_obj_add_state(objects.btn_backlight_alarm_wake, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_backlight_alarm_wake, LV_STATE_CHECKED);
        }
    }
    alarm_wake_syncing_ = false;

    preset_syncing_ = true;
    bool always_on = backlight_timeout_ms_ == 0;
    if (objects.btn_backlight_always_on) {
        if (always_on) lv_obj_add_state(objects.btn_backlight_always_on, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_always_on, LV_STATE_CHECKED);
    }
    if (objects.btn_backlight_30s) {
        if (backlight_timeout_ms_ == Config::BACKLIGHT_TIMEOUT_30S) lv_obj_add_state(objects.btn_backlight_30s, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_30s, LV_STATE_CHECKED);
    }
    if (objects.btn_backlight_1m) {
        if (backlight_timeout_ms_ == Config::BACKLIGHT_TIMEOUT_1M) lv_obj_add_state(objects.btn_backlight_1m, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_backlight_1m, LV_STATE_CHECKED);
    }
    preset_syncing_ = false;
    ui_dirty_ = false;
}

void BacklightManager::consumeInput() {
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_reset(indev, nullptr);
        }
        indev = lv_indev_get_next(indev);
    }
}

void BacklightManager::poll(bool lvgl_ready) {
    auto bus_access = runtime_gate_.acquire();
    if (!bus_access || !panel_backlight_) {
        return;
    }
    RuntimeSnapshotPublishGuard snapshot_guard(*this);
    const uint32_t now_ms = millis();
    finalizeGuardedWake(now_ms);
    if (guarded_wake_settle_pending_ || guarded_wake_render_pending_ ||
        !lvgl_ready) {
        return;
    }
    if (guarded_wake_pending_) {
        // PreQuiet is intentionally owner-only. Once a guarded wake exists,
        // do not sample LVGL inactivity, refresh schedules, consume input or
        // retry unrelated commands before evaluating the quiet deadline.
        (void)processGuardedWake(now_ms);
        return;
    }
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }
    uint32_t inactive_ms = lv_disp_get_inactive_time(disp);
    const bool input_activity =
        BacklightStatePolicy::inputActivitySince(last_inactive_ms_, inactive_ms);
    last_inactive_ms_ = inactive_ms;

    refreshScheduleWithGateHeld(true);

    if (!pending_command_.active()) {
        pending_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    }

    const bool alarm_wake_requested = alarm_wake_enabled_ && alarm_wake_active_;
    if (backlight_on_ && pending_command_.active() && !pending_command_.targetOn() &&
        (input_activity || alarm_wake_requested)) {
        // Touch activity and an active alarm are newer ON requests. They cancel
        // a failed OFF command before its retry can blank an active display.
        setOnWithGateHeld(true);
    }

    if (pending_command_.ready(now_ms)) {
        const bool retry_target_on = pending_command_.targetOn();
        if (retry_target_on && !backlight_on_) {
            if (pending_wake_event_ != BacklightWakeBreadcrumbs::Event::None) {
                requestGuardedWake(pending_wake_event_, now_ms);
            } else {
                // This should be unreachable once every OFF-to-ON caller uses
                // requestGuardedWake(). Back off instead of bypassing safety.
                pending_command_.recordFailure(true, now_ms);
                LOGE("Backlight",
                     "ON retry has no guarded wake source; retry deferred");
            }
        } else {
            const bool was_on = backlight_on_;
            const BacklightWakeBreadcrumbs::Event recovery_event =
                retry_target_on && backlight_on_
                    ? pending_wake_event_
                    : BacklightWakeBreadcrumbs::Event::None;
            const bool trace_probe_recovery =
                recovery_event != BacklightWakeBreadcrumbs::Event::None;
            if (trace_probe_recovery) {
                // A previous driver attempt may have physically enabled the
                // backlight while touch-probe synchronization failed. Record
                // this later probe-only retry as a distinct attempt so the
                // retained status reflects a successful recovery.
                BacklightWakeBreadcrumbs::beginWake(
                    recovery_event,
                    now_ms,
                    static_cast<uint32_t>(time(nullptr)),
                    true,
                    true,
                    sample_shared_i2c_lines());
            }
            const bool retry_succeeded = setOnWithGateHeld(
                retry_target_on,
                trace_probe_recovery
                    ? recovery_event
                    : BacklightWakeBreadcrumbs::Event::None);
            if (trace_probe_recovery) {
                BacklightWakeBreadcrumbs::markCommandReturned(
                    retry_succeeded
                        ? BacklightWakeBreadcrumbs::CommandResult::Succeeded
                        : BacklightWakeBreadcrumbs::CommandResult::Failed);
                if (retry_succeeded) {
                    BacklightWakeBreadcrumbs::markCompleted();
                }
            }
            if (retry_succeeded) {
                pending_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
            }
            if (retry_succeeded && retry_target_on && !was_on) {
                block_input_until_ms_ = now_ms + Config::BACKLIGHT_WAKE_BLOCK_MS;
                lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
                consumeInput();
            }
        }
    }

    if (!backlight_on_) {
        const bool wake_touch = lvgl_port_take_wake_touch_pending();
        const BacklightWakeBreadcrumbs::Event wake_event =
            BacklightWakeBreadcrumbs::selectDarkWakeEvent(
                wake_touch, alarm_wake_requested);
        if (wake_event != BacklightWakeBreadcrumbs::Event::None) {
            if (pending_command_.active() && pending_command_.targetOn() &&
                !pending_command_.ready(now_ms)) {
                if (wake_event_priority(wake_event) >
                    wake_event_priority(pending_wake_event_)) {
                    pending_wake_event_ = wake_event;
                }
            } else {
                requestGuardedWake(wake_event, now_ms);
            }
        }
        if (processGuardedWake(now_ms)) {
            return;
        }
        return;
    }

    if (BacklightStatePolicy::beforeDeadline(now_ms, block_input_until_ms_)) {
        consumeInput();
    } else {
        block_input_until_ms_ = 0;
    }

    uint32_t effective_timeout_ms = backlight_timeout_ms_;
    if (schedule_active_ && effective_timeout_ms == 0) {
        effective_timeout_ms = Config::BACKLIGHT_SCHEDULE_WAKE_MS;
    }
    if (alarm_wake_requested) {
        return;
    }
    // A successful scheduled or retried ON command triggers LVGL activity.
    // Re-read the timer so the stale pre-command value cannot switch the
    // display straight back off in this same poll.
    inactive_ms = lv_disp_get_inactive_time(disp);
    last_inactive_ms_ = inactive_ms;
    if (effective_timeout_ms > 0 && inactive_ms >= effective_timeout_ms &&
        !pending_command_.active()) {
        setOnWithGateHeld(false);
    }
}
