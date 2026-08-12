// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/FanControl.h"

#include <math.h>

#include "config/AppConfig.h"
#include "config/AppData.h"
#include "core/Logger.h"
#include "modules/DacAutoDemand.h"

namespace {

constexpr uint32_t kDacWriteRetryDelayMs = 2;

bool timeReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

} // namespace

void FanControl::ensureSyncPrimitives() {
    if (sync_mutex_ == nullptr) {
        sync_mutex_ = xSemaphoreCreateMutex();
    }
}

bool FanControl::lockSync() const {
    if (sync_mutex_ == nullptr) {
        return true;
    }
    return xSemaphoreTake(sync_mutex_, portMAX_DELAY) == pdTRUE;
}

void FanControl::unlockSync() const {
    if (sync_mutex_ != nullptr) {
        xSemaphoreGive(sync_mutex_);
    }
}

void FanControl::drainPendingCommands(PendingCommands &out) {
    out = PendingCommands{};
    if (!lockSync()) {
        return;
    }
    out = pending_commands_;
    pending_commands_ = PendingCommands{};
    unlockSync();
}

void FanControl::publishSnapshot() {
    if (!lockSync()) {
        return;
    }
    snapshot_.present = present_;
    snapshot_.available = available_;
    snapshot_.running = running_;
    snapshot_.faulted = faulted_;
    snapshot_.output_known = output_known_;
    snapshot_.manual_override_active = manual_override_active_;
    snapshot_.auto_resume_blocked = auto_resume_blocked_;
    snapshot_.mode = mode_;
    snapshot_.manual_step = manual_step_;
    snapshot_.selected_timer_s = selected_timer_s_;
    snapshot_.output_mv = output_mv_;
    snapshot_.stop_at_ms = stop_at_ms_;
    snapshot_.auto_config = auto_config_;
    unlockSync();
}

bool FanControl::isAvailable() const {
    bool value = snapshot_.available;
    if (lockSync()) {
        value = snapshot_.available;
        unlockSync();
    }
    return value;
}

bool FanControl::isRunning() const {
    bool value = snapshot_.running;
    if (lockSync()) {
        value = snapshot_.running;
        unlockSync();
    }
    return value;
}

bool FanControl::isFaulted() const {
    bool value = snapshot_.faulted;
    if (lockSync()) {
        value = snapshot_.faulted;
        unlockSync();
    }
    return value;
}

bool FanControl::isOutputKnown() const {
    bool value = snapshot_.output_known;
    if (lockSync()) {
        value = snapshot_.output_known;
        unlockSync();
    }
    return value;
}

bool FanControl::isManualOverrideActive() const {
    bool value = snapshot_.manual_override_active;
    if (lockSync()) {
        value = snapshot_.manual_override_active;
        unlockSync();
    }
    return value;
}

bool FanControl::isAutoResumeBlocked() const {
    bool value = snapshot_.auto_resume_blocked;
    if (lockSync()) {
        value = snapshot_.auto_resume_blocked;
        unlockSync();
    }
    return value;
}

FanControl::Mode FanControl::mode() const {
    Mode value = snapshot_.mode;
    if (lockSync()) {
        value = snapshot_.mode;
        unlockSync();
    }
    return value;
}

uint8_t FanControl::manualStep() const {
    uint8_t value = snapshot_.manual_step;
    if (lockSync()) {
        value = snapshot_.manual_step;
        unlockSync();
    }
    return value;
}

uint32_t FanControl::selectedTimerSeconds() const {
    uint32_t value = snapshot_.selected_timer_s;
    if (lockSync()) {
        value = snapshot_.selected_timer_s;
        unlockSync();
    }
    return value;
}

uint16_t FanControl::outputMillivolts() const {
    uint16_t value = snapshot_.output_mv;
    if (lockSync()) {
        value = snapshot_.output_mv;
        unlockSync();
    }
    return value;
}

DacAutoConfig FanControl::autoConfig() const {
    DacAutoConfig value = snapshot_.auto_config;
    if (lockSync()) {
        value = snapshot_.auto_config;
        unlockSync();
    }
    return value;
}

uint8_t FanControl::outputPercent() const {
    if (Config::DAC_VOUT_FULL_SCALE_MV == 0) {
        return 0;
    }
    uint16_t output_mv = snapshot_.output_mv;
    if (lockSync()) {
        output_mv = snapshot_.output_mv;
        unlockSync();
    }
    uint32_t percent = static_cast<uint32_t>(output_mv) * 100u;
    percent = (percent + (Config::DAC_VOUT_FULL_SCALE_MV / 2u)) / Config::DAC_VOUT_FULL_SCALE_MV;
    if (percent > 100u) {
        percent = 100u;
    }
    return static_cast<uint8_t>(percent);
}

uint32_t FanControl::remainingSeconds(uint32_t now_ms) const {
    bool running = snapshot_.running;
    uint32_t stop_at_ms = snapshot_.stop_at_ms;
    if (lockSync()) {
        running = snapshot_.running;
        stop_at_ms = snapshot_.stop_at_ms;
        unlockSync();
    }
    if (!running || stop_at_ms == 0 || timeReached(now_ms, stop_at_ms)) {
        return 0;
    }
    return (stop_at_ms - now_ms + 999UL) / 1000UL;
}

FanControl::Snapshot FanControl::snapshot() const {
    Snapshot value = snapshot_;
    if (lockSync()) {
        value = snapshot_;
        unlockSync();
    }
    return value;
}

void FanControl::begin(bool auto_mode_preference, bool auto_armed_preference) {
    ensureSyncPrimitives();
    initialized_ = true;

    mode_ = auto_mode_preference ? Mode::Auto : Mode::Manual;
    manual_step_ = 1;
    selected_timer_s_ = 0;
    start_requested_ = false;
    stop_requested_ = false;
    present_ = false;
    available_ = false;
    faulted_ = false;
    applyStopState(true);
    manual_step_update_pending_ = false;
    timer_update_pending_ = false;
    last_recover_attempt_ms_ = 0;
    last_health_check_ms_ = 0;
    health_probe_fail_count_ = 0;
    dac_ever_ready_ = false;
    boot_auto_resume_pending_ = false;
    boot_auto_resume_due_ms_ = 0;
    auto_resume_blocked_ = false;
    if (mode_ == Mode::Auto) {
        auto_resume_blocked_ = true;
        if (auto_armed_preference) {
            boot_auto_resume_pending_ = true;
            boot_auto_resume_due_ms_ = millis() + Config::DAC_AUTO_BOOT_RESUME_DELAY_MS;
            LOGI("FanControl", "auto resume scheduled in %lu ms after boot",
                 static_cast<unsigned long>(Config::DAC_AUTO_BOOT_RESUME_DELAY_MS));
        }
    }
    pending_commands_ = PendingCommands{};
    snapshot_ = Snapshot{};

    if (!Config::DAC_FEATURE_ENABLED) {
        LOGI("FanControl", "DAC feature disabled");
        publishSnapshot();
        return;
    }

    const uint32_t now_ms = millis();
    startup_probe_.reset(now_ms);
    const char *init_failure_reason = nullptr;
    const InitStatus init_status = tryInitialize(now_ms, init_failure_reason);
    startup_probe_.recordAttempt(init_status == InitStatus::Ok);
    switch (init_status) {
        case InitStatus::Ok:
            dac_ever_ready_ = true;
            LOGI("FanControl", "DAC ready at 0x%02X", Config::DAC_I2C_ADDR_DEFAULT);
            break;
        case InitStatus::Absent:
            LOGI("FanControl", "DAC not detected; bounded startup retries scheduled");
            applyStopState(false);
            break;
        case InitStatus::Fault:
            Logger::log(Logger::Warn, "FanControl", "DAC init failed: %s",
                        init_failure_reason ? init_failure_reason : "unknown");
            break;
    }
    publishSnapshot();
}

bool FanControl::prepareForRestart() {
    return prepareSafeShutdown("safe restart write failed");
}

bool FanControl::prepareForI2cOffline() {
    return prepareSafeShutdown("safe I2C-offline write failed");
}

bool FanControl::prepareSafeShutdown(const char *failure_reason) {
    if (!initialized_ || !Config::DAC_FEATURE_ENABLED) {
        return true;
    }

    // Keep shutdown bounded: one best-effort I2C write, with no retry delay.
    // This is attempted even when startup detection failed because a GP8403
    // can retain an earlier non-zero output across an ESP-only restart. The
    // transaction itself is limited by I2C_TIMEOUT_MS.
    if (!dac_.writeChannelMillivolts(Config::DAC_CHANNEL_VOUT0,
                                     Config::DAC_SAFE_ERROR_MV)) {
        if (present_ || dac_ever_ready_) {
            handleDacFault(failure_reason);
            publishSnapshot();
        }
        return false;
    }

    applyStopState(true);
    publishSnapshot();
    return true;
}

void FanControl::poll(uint32_t now_ms,
                      const SensorData *sensor_data,
                      bool gas_warmup,
                      const DisplayThresholds::Config &thresholds) {
    if (!initialized_) {
        return;
    }
    ensureSyncPrimitives();

    PendingCommands pending;
    drainPendingCommands(pending);

    if (pending.has_auto_config) {
        applyAutoConfig(pending.auto_config);
    }
    if (pending.has_mode) {
        applyMode(pending.mode);
    }
    if (pending.has_manual_step) {
        applyManualStep(pending.manual_step);
    }
    if (pending.has_timer_seconds) {
        applyTimerSeconds(pending.timer_seconds);
    }
    switch (pending.start_stop_request) {
        case PendingCommands::StartStopRequest::Start:
            applyRequestStart();
            break;
        case PendingCommands::StartStopRequest::Stop:
            applyRequestStop();
            break;
        case PendingCommands::StartStopRequest::AutoStart:
            applyRequestAutoStart();
            break;
        case PendingCommands::StartStopRequest::None:
        default:
            break;
    }

    if (!Config::DAC_FEATURE_ENABLED) {
        present_ = false;
        available_ = false;
        faulted_ = false;
        applyStopState(true);
        publishSnapshot();
        return;
    }

    if (!available_) {
        if (!dac_ever_ready_ && startup_probe_.shouldAttempt(now_ms)) {
            const char *init_failure_reason = nullptr;
            const InitStatus init_status = tryInitialize(now_ms, init_failure_reason);
            const bool ready = init_status == InitStatus::Ok;
            startup_probe_.recordAttempt(ready);
            if (ready) {
                dac_ever_ready_ = true;
                LOGI("FanControl", "DAC detected during startup retry %u/%u",
                     static_cast<unsigned>(startup_probe_.attempts()),
                     static_cast<unsigned>(StartupProbePolicy::kMaxAttempts));
            } else if (startup_probe_.exhausted()) {
                LOGW("FanControl",
                     "DAC not detected after %u startup attempts; retries stopped until reboot",
                     static_cast<unsigned>(StartupProbePolicy::kMaxAttempts));
            } else if (init_status == InitStatus::Fault) {
                LOGD("FanControl", "DAC startup retry %u/%u failed: %s",
                     static_cast<unsigned>(startup_probe_.attempts()),
                     static_cast<unsigned>(StartupProbePolicy::kMaxAttempts),
                     init_failure_reason ? init_failure_reason : "unknown");
            }
        } else if (dac_ever_ready_ &&
                   now_ms - last_recover_attempt_ms_ >= Config::DAC_RECOVER_COOLDOWN_MS) {
            last_recover_attempt_ms_ = now_ms;
            const char *init_failure_reason = nullptr;
            if (tryInitialize(now_ms, init_failure_reason) == InitStatus::Ok) {
                LOGI("FanControl", "DAC recovered");
            }
        }
    } else if (!running_ &&
               now_ms - last_health_check_ms_ >= Config::DAC_HEALTH_CHECK_MS) {
        last_health_check_ms_ = now_ms;
        if (!dac_.probe()) {
            if (health_probe_fail_count_ < 0xFFu) {
                ++health_probe_fail_count_;
            }
            if (health_probe_fail_count_ >= Config::DAC_HEALTH_FAIL_THRESHOLD) {
                handleDacFault("probe failed");
            } else {
                // Shared-I2C DAC probes can miss transiently; keep intermediate
                // retries out of normal user-facing logs and only surface the
                // final fault transition.
                LOGD("FanControl",
                     "DAC probe failed (%u/%u)",
                     static_cast<unsigned>(health_probe_fail_count_),
                     static_cast<unsigned>(Config::DAC_HEALTH_FAIL_THRESHOLD));
            }
        } else {
            health_probe_fail_count_ = 0;
        }
    }

    if (stop_requested_) {
        stop_requested_ = false;
        if (available_ && !applyOutputMillivolts(Config::DAC_SAFE_ERROR_MV)) {
            handleDacFault("safe stop write failed");
            publishSnapshot();
            return;
        }
        applyStopState(available_);
        if (mode_ == Mode::Auto) {
            // Explicit STOP in auto mode pauses auto-demand until user arms auto again.
            auto_resume_blocked_ = true;
        }
    }

    if (start_requested_) {
        start_requested_ = false;
        if (mode_ != Mode::Manual || !available_) {
            publishSnapshot();
            return;
        }

        const uint16_t target_mv = stepToMillivolts(manual_step_);
        if (!applyOutputMillivolts(target_mv)) {
            handleDacFault("start write failed");
            publishSnapshot();
            return;
        }

        running_ = true;
        manual_override_active_ = true;
        output_mv_ = target_mv;
        manual_step_update_pending_ = false;
        if (selected_timer_s_ > 0) {
            stop_at_ms_ = now_ms + selected_timer_s_ * 1000UL;
        } else {
            stop_at_ms_ = 0;
        }
        timer_update_pending_ = false;
    }

    if (manual_step_update_pending_) {
        manual_step_update_pending_ = false;
        if (running_ && manual_override_active_ && available_) {
            const uint16_t target_mv = stepToMillivolts(manual_step_);
            if (!applyOutputMillivolts(target_mv)) {
                handleDacFault("manual level update failed");
                publishSnapshot();
                return;
            }
            output_mv_ = target_mv;
        }
    }

    if (timer_update_pending_) {
        timer_update_pending_ = false;
        if (running_ && manual_override_active_) {
            if (selected_timer_s_ > 0) {
                stop_at_ms_ = now_ms + selected_timer_s_ * 1000UL;
            } else {
                stop_at_ms_ = 0;
            }
        }
    }

    if (boot_auto_resume_pending_ && timeReached(now_ms, boot_auto_resume_due_ms_)) {
        boot_auto_resume_pending_ = false;
        boot_auto_resume_due_ms_ = 0;
        if (mode_ == Mode::Auto) {
            auto_resume_blocked_ = false;
            LOGI("FanControl", "auto resume armed after boot delay");
        }
    }

    if (mode_ == Mode::Auto && available_ && !manual_override_active_ && !auto_resume_blocked_) {
        uint8_t demand_percent = 0;
        if (auto_config_.enabled && sensor_data != nullptr) {
            demand_percent = evaluateAutoDemandPercent(*sensor_data, gas_warmup, thresholds);
        }
        const uint16_t target_mv = percentToMillivolts(demand_percent);

        if (target_mv == 0) {
            if (running_ || !output_known_ || output_mv_ != Config::DAC_SAFE_ERROR_MV) {
                if (!applyOutputMillivolts(Config::DAC_SAFE_ERROR_MV)) {
                    handleDacFault("auto stop write failed");
                    publishSnapshot();
                    return;
                }
                applyStopState(true);
            } else {
                output_known_ = true;
                output_mv_ = Config::DAC_SAFE_ERROR_MV;
            }
        } else {
            if (!running_ || output_mv_ != target_mv) {
                if (!applyOutputMillivolts(target_mv)) {
                    handleDacFault("auto level write failed");
                    publishSnapshot();
                    return;
                }
            }
            running_ = true;
            output_known_ = true;
            output_mv_ = target_mv;
            stop_at_ms_ = 0;
        }
    }

    if (running_ && stop_at_ms_ != 0 && timeReached(now_ms, stop_at_ms_)) {
        if (available_ && !applyOutputMillivolts(Config::DAC_SAFE_ERROR_MV)) {
            handleDacFault("timer stop write failed");
            publishSnapshot();
            return;
        }
        const bool auto_resume_on_timer_end = available_ &&
                                              auto_config_.enabled &&
                                              !auto_resume_blocked_;
        applyStopState(available_);
        if (auto_resume_on_timer_end) {
            mode_ = Mode::Auto;
        }
    }

    publishSnapshot();
}

void FanControl::setMode(Mode mode) {
    ensureSyncPrimitives();
    if (!lockSync()) {
        return;
    }
    pending_commands_.has_mode = true;
    pending_commands_.mode = mode;
    if (mode == Mode::Manual &&
        pending_commands_.start_stop_request == PendingCommands::StartStopRequest::AutoStart) {
        pending_commands_.start_stop_request = PendingCommands::StartStopRequest::None;
    }
    unlockSync();
}

void FanControl::setManualStep(uint8_t step) {
    ensureSyncPrimitives();
    if (step < 1) {
        step = 1;
    } else if (step > 10) {
        step = 10;
    }
    if (!lockSync()) {
        return;
    }
    pending_commands_.has_manual_step = true;
    pending_commands_.manual_step = step;
    unlockSync();
}

void FanControl::setTimerSeconds(uint32_t seconds) {
    ensureSyncPrimitives();
    if (!lockSync()) {
        return;
    }
    pending_commands_.has_timer_seconds = true;
    pending_commands_.timer_seconds = seconds;
    unlockSync();
}

void FanControl::requestStart() {
    ensureSyncPrimitives();
    if (!lockSync()) {
        return;
    }
    pending_commands_.start_stop_request = PendingCommands::StartStopRequest::Start;
    unlockSync();
}

void FanControl::requestStop() {
    ensureSyncPrimitives();
    if (!lockSync()) {
        return;
    }
    pending_commands_.start_stop_request = PendingCommands::StartStopRequest::Stop;
    unlockSync();
}

void FanControl::requestAutoStart() {
    ensureSyncPrimitives();
    if (!lockSync()) {
        return;
    }
    pending_commands_.start_stop_request = PendingCommands::StartStopRequest::AutoStart;
    pending_commands_.has_mode = true;
    pending_commands_.mode = Mode::Auto;
    unlockSync();
}

void FanControl::setAutoConfig(const DacAutoConfig &config) {
    ensureSyncPrimitives();
    DacAutoConfig sanitized = config;
    DacAutoConfigJson::sanitize(sanitized);
    if (!lockSync()) {
        return;
    }
    pending_commands_.has_auto_config = true;
    pending_commands_.auto_config = sanitized;
    unlockSync();
}

void FanControl::applyMode(Mode mode) {
    if (mode == Mode::Auto && mode_ != Mode::Auto) {
        // Selecting AUTO only switches mode/UI; START AUTO is the explicit re-arm action.
        auto_resume_blocked_ = true;
        boot_auto_resume_pending_ = false;
        boot_auto_resume_due_ms_ = 0;
    }
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    if (mode_ == Mode::Manual) {
        auto_resume_blocked_ = false;
        boot_auto_resume_pending_ = false;
        boot_auto_resume_due_ms_ = 0;
    }
    if (mode_ == Mode::Auto && !manual_override_active_) {
        manual_step_update_pending_ = false;
        timer_update_pending_ = false;
    }
}

void FanControl::applyManualStep(uint8_t step) {
    if (step < 1) {
        step = 1;
    } else if (step > 10) {
        step = 10;
    }
    if (manual_step_ != step) {
        manual_step_ = step;
        manual_step_update_pending_ = true;
    }
}

void FanControl::applyTimerSeconds(uint32_t seconds) {
    if (selected_timer_s_ != seconds) {
        selected_timer_s_ = seconds;
        timer_update_pending_ = true;
    }
}

void FanControl::applyRequestStart() {
    stop_requested_ = false;
    start_requested_ = true;
}

void FanControl::applyRequestStop() {
    start_requested_ = false;
    stop_requested_ = true;
    boot_auto_resume_pending_ = false;
    boot_auto_resume_due_ms_ = 0;
}

void FanControl::applyRequestAutoStart() {
    applyMode(Mode::Auto);
    start_requested_ = false;
    stop_requested_ = false;
    manual_override_active_ = false;
    stop_at_ms_ = 0;
    manual_step_update_pending_ = false;
    timer_update_pending_ = false;
    auto_resume_blocked_ = false;
    boot_auto_resume_pending_ = false;
    boot_auto_resume_due_ms_ = 0;
}

void FanControl::applyAutoConfig(const DacAutoConfig &config) {
    auto_config_ = config;
    DacAutoConfigJson::sanitize(auto_config_);
}

FanControl::InitStatus FanControl::tryInitialize(uint32_t now_ms, const char *&failure_reason) {
    failure_reason = nullptr;
    if (!dac_.begin(Config::DAC_I2C_ADDR_DEFAULT)) {
        present_ = false;
        available_ = false;
        faulted_ = false;
        return InitStatus::Absent;
    }
    present_ = true;
    if (!dac_.setOutputRange10V()) {
        available_ = false;
        faulted_ = true;
        applyStopState(false);
        health_probe_fail_count_ = 0;
        last_recover_attempt_ms_ = now_ms;
        failure_reason = "range write failed";
        return InitStatus::Fault;
    }
    if (!dac_.writeChannelMillivolts(Config::DAC_CHANNEL_VOUT0, Config::DAC_SAFE_DEFAULT_MV)) {
        available_ = false;
        faulted_ = true;
        applyStopState(false);
        health_probe_fail_count_ = 0;
        last_recover_attempt_ms_ = now_ms;
        failure_reason = "default output write failed";
        return InitStatus::Fault;
    }

    available_ = true;
    faulted_ = false;
    running_ = false;
    manual_override_active_ = false;
    output_known_ = true;
    output_mv_ = Config::DAC_SAFE_DEFAULT_MV;
    stop_at_ms_ = 0;
    manual_step_update_pending_ = false;
    timer_update_pending_ = false;
    last_health_check_ms_ = now_ms;
    health_probe_fail_count_ = 0;
    return InitStatus::Ok;
}

bool FanControl::applyOutputMillivolts(uint16_t millivolts) {
    if (dac_.writeChannelMillivolts(Config::DAC_CHANNEL_VOUT0, millivolts)) {
        return true;
    }

    LOGW("FanControl",
         "DAC write failed at %u mV, retrying once",
         static_cast<unsigned>(millivolts));
    vTaskDelay(pdMS_TO_TICKS(kDacWriteRetryDelayMs));
    return dac_.writeChannelMillivolts(Config::DAC_CHANNEL_VOUT0, millivolts);
}

void FanControl::handleDacFault(const char *reason) {
    LOGW("FanControl", "DAC error: %s", reason ? reason : "unknown");
    present_ = true;
    available_ = false;
    faulted_ = true;
    applyStopState(false);
    health_probe_fail_count_ = 0;
    last_recover_attempt_ms_ = millis();
}

void FanControl::applyStopState(bool output_known) {
    running_ = false;
    manual_override_active_ = false;
    output_known_ = output_known;
    if (output_known_) {
        output_mv_ = Config::DAC_SAFE_ERROR_MV;
    }
    stop_at_ms_ = 0;
    manual_step_update_pending_ = false;
    timer_update_pending_ = false;
}

uint16_t FanControl::stepToMillivolts(uint8_t step) const {
    if (step < 1) {
        step = 1;
    } else if (step > 10) {
        step = 10;
    }
    const uint16_t millivolts = static_cast<uint16_t>(step) * 1000u;
    if (millivolts > Config::DAC_VOUT_FULL_SCALE_MV) {
        return Config::DAC_VOUT_FULL_SCALE_MV;
    }
    return millivolts;
}

uint16_t FanControl::percentToMillivolts(uint8_t percent) const {
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t mv = static_cast<uint32_t>(percent) * Config::DAC_VOUT_FULL_SCALE_MV + 50u;
    return static_cast<uint16_t>(mv / 100u);
}

uint8_t FanControl::evaluateAutoDemandPercent(const SensorData &data,
                                              bool gas_warmup,
                                              const DisplayThresholds::Config &thresholds) const {
    return DacAutoDemand::evaluate(auto_config_, data, gas_warmup, thresholds).percent;
}
