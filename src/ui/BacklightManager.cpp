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

bool BacklightManager::setOn(bool on) {
    if (!on) {
        cancelGuardedWake();
    }
    auto bus_access = runtime_gate_.acquire();
    if (!bus_access) {
        return false;
    }
    return setOnWithGateHeld(on);
}

void BacklightManager::requestGuardedWake(
    BacklightWakeBreadcrumbs::Event event,
    uint32_t now_ms) {
    if (backlight_on_) {
        return;
    }

    const auto priority = [](BacklightWakeBreadcrumbs::Event value) {
        switch (value) {
            case BacklightWakeBreadcrumbs::Event::AlarmWake: return 3;
            case BacklightWakeBreadcrumbs::Event::TouchWake: return 2;
            case BacklightWakeBreadcrumbs::Event::ScheduleWake: return 1;
            case BacklightWakeBreadcrumbs::Event::None: return 0;
        }
        return 0;
    };

    if (!guarded_wake_pending_) {
        guarded_wake_pending_ = true;
        guarded_wake_event_ = event;
        (void)WakePowerGuard::request(now_ms);
        return;
    }

    if (priority(event) > priority(guarded_wake_event_)) {
        guarded_wake_event_ = event;
    }
}

bool BacklightManager::processGuardedWake(uint32_t now_ms) {
    if (!guarded_wake_pending_) {
        return false;
    }
    if (backlight_on_) {
        guarded_wake_pending_ = false;
        guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
        WakePowerGuard::cancel();
        return false;
    }

    if (WakePowerGuard::phase(now_ms) == WakePowerGuard::Phase::Idle) {
        (void)WakePowerGuard::request(now_ms);
    }
    if (!WakePowerGuard::readyToSwitch(
            now_ms,
            Config::BACKLIGHT_WAKE_PRE_QUIET_MIN_MS,
            Config::BACKLIGHT_WAKE_PRE_QUIET_MAX_MS)) {
        return true;
    }

    const BacklightWakeBreadcrumbs::Event wake_event = guarded_wake_event_;
    guarded_wake_pending_ = false;
    guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    const bool wake_succeeded = setOnWithGateHeld(true, wake_event);
    if (wake_event != BacklightWakeBreadcrumbs::Event::None) {
        BacklightWakeBreadcrumbs::markCommandReturned();
    }

    if (wake_succeeded) {
        const uint32_t switched_ms = millis();
        WakePowerGuard::beginSettle(switched_ms,
                                    Config::BACKLIGHT_WAKE_SETTLE_MS);
        block_input_until_ms_ = switched_ms + Config::BACKLIGHT_WAKE_BLOCK_MS;
        lvgl_port_block_touch_read(Config::BACKLIGHT_WAKE_BLOCK_MS);
        consumeInput();
        pending_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    } else {
        WakePowerGuard::cancel();
        pending_wake_event_ =
            pending_command_.active() && pending_command_.targetOn()
                ? wake_event
                : BacklightWakeBreadcrumbs::Event::None;
    }

    if (wake_event != BacklightWakeBreadcrumbs::Event::None) {
        BacklightWakeBreadcrumbs::markCompleted();
    }
    return true;
}

void BacklightManager::cancelGuardedWake() {
    guarded_wake_pending_ = false;
    guarded_wake_event_ = BacklightWakeBreadcrumbs::Event::None;
    WakePowerGuard::cancel();
}

bool BacklightManager::setOnWithGateHeld(
    bool on,
    BacklightWakeBreadcrumbs::Event trace_event) {
    const bool trace_wake = trace_event != BacklightWakeBreadcrumbs::Event::None;
    const auto sample_bus = []() {
        const I2cBusRecovery::LineState lines = I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::I2C_SCL_PIN));
        return BacklightWakeBreadcrumbs::LineState{
            true,
            lines.sda_high,
            lines.scl_high,
        };
    };

    if (trace_wake) {
        BacklightWakeBreadcrumbs::beginWake(
            trace_event,
            millis(),
            static_cast<uint32_t>(time(nullptr)),
            on,
            backlight_on_,
            sample_bus());
    }

    if (!panel_backlight_) {
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markDriverCallBegin();
            BacklightWakeBreadcrumbs::markDriverCallReturned(false, false, 0, sample_bus());
        }
        LOGE("Backlight", "cannot turn backlight %s: driver unavailable", on ? "on" : "off");
        return false;
    }
    if (on == backlight_on_) {
        const bool cancelled_retry = pending_command_.active();
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markDriverCallBegin();
            BacklightWakeBreadcrumbs::markDriverCallReturned(true, true, 0, sample_bus());
            BacklightWakeBreadcrumbs::markWakeProbeUpdateBegin();
        }
        const bool wake_probe_updated = lvgl_port_set_wake_touch_probe(!on);
        if (trace_wake) {
            BacklightWakeBreadcrumbs::markWakeProbeUpdateReturned(sample_bus());
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
    const uint32_t driver_started_us = micros();
    const bool driver_succeeded = on ? panel_backlight_->on() : panel_backlight_->off();
    const uint32_t driver_duration_us = micros() - driver_started_us;
    if (trace_wake) {
        BacklightWakeBreadcrumbs::markDriverCallReturned(
            driver_succeeded, false, driver_duration_us, sample_bus());
        BacklightWakeBreadcrumbs::markWakeProbeUpdateBegin();
    }
    const BacklightStatePolicy::Transition transition =
        BacklightStatePolicy::resolve(previous_on, on, driver_succeeded);
    backlight_on_ = transition.actual_on;
    const bool wake_probe_updated =
        lvgl_port_set_wake_touch_probe(transition.wake_probe_enabled);
    if (trace_wake) {
        BacklightWakeBreadcrumbs::markWakeProbeUpdateReturned(sample_bus());
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
    cancelGuardedWake();
    if (!runtime_gate_.disable()) {
        return;
    }
    LOGW("Backlight", "shared I2C bus closing; new backlight commands disabled");
}

bool BacklightManager::waitForSharedBusIdle(uint32_t timeout_ms) {
    const uint32_t start_ms = millis();
    uint32_t now_ms = start_ms;
    while (!runtime_gate_.idle() &&
           !BacklightStatePolicy::drainWaitExpired(start_ms, now_ms, timeout_ms)) {
        delay(1);
        now_ms = millis();
    }

    if (!BacklightStatePolicy::clearPendingAfterDrain(runtime_gate_,
                                                      pending_command_)) {
        return false;
    }
    block_input_until_ms_ = 0;
    lvgl_port_set_wake_touch_probe(false);
    LOGW("Backlight", "shared I2C bus offline; backlight operations drained");
    return true;
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
    refreshScheduleWithGateHeld();
}

void BacklightManager::refreshScheduleWithGateHeld(bool trace_clock_transition) {
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
        const bool schedule_wake = BacklightWakeBreadcrumbs::shouldTraceScheduleWake(
            trace_clock_transition, schedule_active_, active);
        schedule_active_ = active;
        const BacklightWakeBreadcrumbs::Event wake_event = schedule_wake
            ? BacklightWakeBreadcrumbs::Event::ScheduleWake
            : BacklightWakeBreadcrumbs::Event::None;
        if (!active && !backlight_on_) {
            requestGuardedWake(wake_event, millis());
            return;
        }
        if (active) {
            cancelGuardedWake();
        }
        const bool command_succeeded = setOnWithGateHeld(!active, wake_event);
        if (schedule_wake) {
            BacklightWakeBreadcrumbs::markCommandReturned();
            BacklightWakeBreadcrumbs::markCompleted();
            pending_wake_event_ =
                !command_succeeded && pending_command_.active() && pending_command_.targetOn()
                    ? wake_event
                    : BacklightWakeBreadcrumbs::Event::None;
        }
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
    if (!bus_access || !panel_backlight_ || !lvgl_ready) {
        return;
    }
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }
    uint32_t now_ms = millis();
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
        const BacklightWakeBreadcrumbs::Event retry_event = retry_target_on
            ? pending_wake_event_
            : BacklightWakeBreadcrumbs::Event::None;
        if (retry_event != BacklightWakeBreadcrumbs::Event::None) {
            requestGuardedWake(retry_event, now_ms);
        } else {
            const bool was_on = backlight_on_;
            const bool retry_succeeded = setOnWithGateHeld(retry_target_on);
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
        if (wake_event != BacklightWakeBreadcrumbs::Event::None &&
            !pending_command_.active()) {
            requestGuardedWake(wake_event, now_ms);
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
