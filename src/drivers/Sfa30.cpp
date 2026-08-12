// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Sfa30.h"

#include <math.h>

#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/I2CHelper.h"
#include "core/Logger.h"

namespace {

bool sfa30StateUnknownAfterBoot() {
    return !boot_peripherals_cold_start;
}

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

} // namespace

bool Sfa30::begin() {
    ok_ = true;
    measuring_ = false;
    measurement_state_unknown_ = sfa30StateUnknownAfterBoot();
    warmup_active_ = boot_peripherals_cold_start;
    data_valid_ = false;
    has_new_data_ = false;
    last_hcho_ppb_ = 0.0f;
    last_poll_ms_ = 0;
    warmup_deadline_ms_ = warmup_active_ ? Config::SFA30_POWERUP_SUPPRESS_MS : 0;
    last_data_ms_ = 0;
    fail_count_ = 0;
    status_ = Status::Absent;
    last_error_cause_ = ErrorCause::None;
    late_start_phase_ = LateStartPhase::Idle;
    late_start_due_ms_ = 0;
    late_start_identified_ = false;
    return true;
}

bool Sfa30::probe() {
    if (!pingAddress()) {
        ok_ = false;
        status_ = Status::Absent;
        last_error_cause_ = ErrorCause::None;
        return false;
    }

    if (!detectSensor()) {
        ok_ = false;
        status_ = Status::Fault;
        return false;
    }

    return true;
}

void Sfa30::start() {
    if (measuring_ && !measurement_state_unknown_) {
        return;
    }
    if (!probe()) {
        ok_ = false;
        if (status_ != Status::Absent) {
            LOGW(label(), "detect failed (%s)", errorCauseLabel());
        }
        return;
    }

    status_ = Status::Fault;
    if (!ensureIdleBeforeStart()) {
        ok_ = false;
        LOGW(label(), "start aborted (%s)", errorCauseLabel());
        return;
    }
    if (!writeCmd(Config::SFA3X_CMD_START)) {
        ok_ = false;
        last_error_cause_ = ErrorCause::StartCommand;
        LOGW(label(), "start failed (%s)", errorCauseLabel());
        return;
    }
    delay(Config::SFA3X_START_DELAY_MS);
    measuring_ = true;
    measurement_state_unknown_ = false;
    ok_ = true;
    status_ = Status::Ok;
    last_error_cause_ = ErrorCause::None;
}

void Sfa30::beginLateStart() {
    ok_ = false;
    status_ = Status::Absent;
    data_valid_ = false;
    has_new_data_ = false;
    late_start_identified_ = false;
    late_start_due_ms_ = 0;
    late_start_phase_ = LateStartPhase::Ping;
}

CooperativeStart::Result Sfa30::finishLateStart(bool success) {
    late_start_phase_ = LateStartPhase::Idle;
    if (!success) {
        ok_ = false;
        return CooperativeStart::Result::Failed;
    }
    measuring_ = true;
    measurement_state_unknown_ = false;
    ok_ = true;
    status_ = Status::Ok;
    last_error_cause_ = ErrorCause::None;
    return CooperativeStart::Result::Success;
}

CooperativeStart::Result Sfa30::pollLateStart(uint32_t now_ms) {
    switch (late_start_phase_) {
        case LateStartPhase::Idle:
            return CooperativeStart::Result::Idle;
        case LateStartPhase::Ping:
            if (!pingAddress()) {
                status_ = Status::Absent;
                last_error_cause_ = ErrorCause::None;
                return finishLateStart(false);
            }
            late_start_phase_ = LateStartPhase::WriteDetect;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteDetect:
            if (!writeCmd(Config::SFA30_CMD_GET_DEVICE_MARKING)) {
                status_ = Status::Fault;
                last_error_cause_ = ErrorCause::ReadCommand;
                return finishLateStart(false);
            }
            late_start_due_ms_ = millis() + Config::SFA3X_READ_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitDetect;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitDetect:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::ReadDetect;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::ReadDetect: {
            uint8_t buf[48] = {};
            if (!readBytes(buf, sizeof(buf))) {
                status_ = Status::Fault;
                last_error_cause_ = ErrorCause::ReadBytes;
                return finishLateStart(false);
            }
            if (!parseDeviceMarking(buf, sizeof(buf))) {
                status_ = Status::Fault;
                return finishLateStart(false);
            }
            late_start_identified_ = true;
            status_ = Status::Fault;
            late_start_phase_ = measurement_state_unknown_
                ? LateStartPhase::StopUnknown
                : LateStartPhase::WriteStart;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::StopUnknown:
            LOGI(label(), "forcing idle after warm restart");
            if (!writeCmd(Config::SFA3X_CMD_STOP)) {
                last_error_cause_ = ErrorCause::WarmRestartStop;
                return finishLateStart(false);
            }
            late_start_due_ms_ = millis() + Config::SFA3X_STOP_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitStop;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitStop:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                measuring_ = false;
                measurement_state_unknown_ = false;
                late_start_phase_ = LateStartPhase::WriteStart;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteStart:
            if (!writeCmd(Config::SFA3X_CMD_START)) {
                last_error_cause_ = ErrorCause::StartCommand;
                return finishLateStart(false);
            }
            late_start_due_ms_ = millis() + Config::SFA3X_START_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitStart;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitStart:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            return finishLateStart(true);
        default:
            return finishLateStart(false);
    }
}

bool Sfa30::isWarmupActive() const {
    if (!warmup_active_) {
        return false;
    }
    return static_cast<int32_t>(millis() - warmup_deadline_ms_) < 0;
}

void Sfa30::stop() {
    if (!measuring_ && !measurement_state_unknown_) {
        return;
    }
    if (!writeCmd(Config::SFA3X_CMD_STOP)) {
        measurement_state_unknown_ = true;
        return;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
}

bool Sfa30::readData(float &hcho_ppb) {
    uint16_t words[3];
    if (!readWords(Config::SFA3X_CMD_READ_VALUES, words, 3, Config::SFA3X_READ_DELAY_MS)) {
        return false;
    }
    const int16_t hcho_raw = static_cast<int16_t>(words[0]);
    hcho_ppb = hcho_raw / 5.0f;
    return true;
}

void Sfa30::poll() {
    if (!ok_ || !measuring_) {
        return;
    }
    const uint32_t now = millis();
    if (now - last_poll_ms_ < Config::SFA3X_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    float hcho_ppb = 0.0f;
    if (!readData(hcho_ppb)) {
        if (++fail_count_ == 3) {
            if (status_ != Status::Absent) {
                status_ = Status::Fault;
                LOGW(label(), "read values failed (%s)", errorCauseLabel());
            }
            fail_count_ = 0;
        }
        return;
    }

    fail_count_ = 0;
    status_ = Status::Ok;
    last_error_cause_ = ErrorCause::None;
    if (isfinite(hcho_ppb) && hcho_ppb >= 0.0f) {
        last_hcho_ppb_ = hcho_ppb;
        data_valid_ = true;
        has_new_data_ = true;
        last_data_ms_ = now;
    }
}

bool Sfa30::takeNewData(float &hcho_ppb) {
    if (!has_new_data_ || !data_valid_) {
        return false;
    }
    hcho_ppb = last_hcho_ppb_;
    has_new_data_ = false;
    return true;
}

void Sfa30::invalidate() {
    data_valid_ = false;
    has_new_data_ = false;
}

bool Sfa30::detectSensor() {
    uint16_t words[16];
    if (!readWords(
            Config::SFA30_CMD_GET_DEVICE_MARKING,
            words,
            sizeof(words) / sizeof(words[0]),
            Config::SFA3X_READ_DELAY_MS
        )) {
        return false;
    }

    bool seen_terminator = false;
    bool seen_printable = false;
    for (uint16_t word : words) {
        const uint8_t bytes[2] = {
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word & 0xFF),
        };
        for (uint8_t byte : bytes) {
            if (seen_terminator) {
                if (byte != 0U) {
                    last_error_cause_ = ErrorCause::DetectSensor;
                    return false;
                }
                continue;
            }
            if (byte == 0U) {
                seen_terminator = true;
                continue;
            }
            if (byte < 0x20U || byte > 0x7EU) {
                last_error_cause_ = ErrorCause::DetectSensor;
                return false;
            }
            seen_printable = true;
        }
    }

    if (!seen_printable) {
        last_error_cause_ = ErrorCause::DetectSensor;
        return false;
    }

    last_error_cause_ = ErrorCause::None;
    return true;
}

bool Sfa30::parseDeviceMarking(const uint8_t *buf, size_t len) {
    if (!buf || len != 48U) {
        last_error_cause_ = ErrorCause::DetectSensor;
        return false;
    }

    bool seen_terminator = false;
    bool seen_printable = false;
    for (size_t offset = 0; offset < len; offset += 3U) {
        if (I2C::crc8(&buf[offset], 2) != buf[offset + 2]) {
            last_error_cause_ = ErrorCause::ReadCrc;
            return false;
        }
        for (size_t byte_index = 0; byte_index < 2U; ++byte_index) {
            const uint8_t byte = buf[offset + byte_index];
            if (seen_terminator) {
                if (byte != 0U) {
                    last_error_cause_ = ErrorCause::DetectSensor;
                    return false;
                }
            } else if (byte == 0U) {
                seen_terminator = true;
            } else if (byte < 0x20U || byte > 0x7EU) {
                last_error_cause_ = ErrorCause::DetectSensor;
                return false;
            } else {
                seen_printable = true;
            }
        }
    }
    if (!seen_printable) {
        last_error_cause_ = ErrorCause::DetectSensor;
        return false;
    }
    last_error_cause_ = ErrorCause::None;
    return true;
}

bool Sfa30::readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms) {
    if (!writeCmd(cmd)) {
        last_error_cause_ = ErrorCause::ReadCommand;
        return false;
    }
    delay(delay_ms);
    const size_t bytes = words * 3;
    uint8_t buf[48];
    if (bytes > sizeof(buf)) {
        last_error_cause_ = ErrorCause::ReadBytes;
        return false;
    }
    if (!readBytes(buf, bytes)) {
        last_error_cause_ = ErrorCause::ReadBytes;
        return false;
    }
    for (size_t i = 0; i < words; ++i) {
        const uint8_t *p = &buf[i * 3];
        if (I2C::crc8(p, 2) != p[2]) {
            last_error_cause_ = ErrorCause::ReadCrc;
            return false;
        }
        out[i] = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    }
    return true;
}

bool Sfa30::ensureIdleBeforeStart() {
    if (!measurement_state_unknown_) {
        return true;
    }

    LOGI(label(), "forcing idle after warm restart");
    if (!writeCmd(Config::SFA3X_CMD_STOP)) {
        last_error_cause_ = ErrorCause::WarmRestartStop;
        return false;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
    last_error_cause_ = ErrorCause::None;
    return true;
}

bool Sfa30::pingAddress() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return false;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (Config::SFA3X_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    const esp_err_t err = i2c_master_cmd_begin(
        Config::I2C_PORT,
        cmd,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

bool Sfa30::writeCmd(uint16_t cmd) {
    return I2C::write_cmd(Config::SFA3X_ADDR, cmd, nullptr, 0) == ESP_OK;
}

bool Sfa30::readBytes(uint8_t *buf, size_t len) {
    return I2C::read_bytes(Config::SFA3X_ADDR, buf, len) == ESP_OK;
}

const char *Sfa30::errorCauseLabel() const {
    switch (last_error_cause_) {
        case ErrorCause::DetectSensor:
            return "detect";
        case ErrorCause::WarmRestartStop:
            return "warm-restart-stop";
        case ErrorCause::StartCommand:
            return "start-cmd";
        case ErrorCause::ReadCommand:
            return "read-cmd";
        case ErrorCause::ReadBytes:
            return "read-bytes";
        case ErrorCause::ReadCrc:
            return "crc";
        default:
            return "unknown";
    }
}
