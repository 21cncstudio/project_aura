// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Sfa40.h"

#include <math.h>

#include "config/AppConfig.h"
#include "core/BootState.h"
#include "core/I2CHelper.h"
#include "core/Logger.h"

namespace {

constexpr uint8_t SFA40_STATUS_NOT_READY = 0x01U;
constexpr uint8_t SFA40_STATUS_NOT_WITHIN_SPEC = 0x02U;

bool sfa40StateUnknownAfterBoot() {
    return !boot_peripherals_cold_start;
}

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

float decodeHumidityPercent(uint16_t raw) {
    float humidity = 125.0f * (static_cast<float>(raw) / 65535.0f) - 6.0f;
    if (humidity < 0.0f) {
        humidity = 0.0f;
    } else if (humidity > 100.0f) {
        humidity = 100.0f;
    }
    return humidity;
}

float decodeTemperatureC(uint16_t raw) {
    return 175.0f * (static_cast<float>(raw) / 65535.0f) - 45.0f;
}

} // namespace

bool Sfa40::begin() {
    ok_ = true;
    measuring_ = false;
    measurement_state_unknown_ = sfa40StateUnknownAfterBoot();
    data_valid_ = false;
    has_new_data_ = false;
    last_hcho_ppb_ = 0.0f;
    last_poll_ms_ = 0;
    next_measurement_read_ms_ = 0;
    last_data_ms_ = 0;
    fail_count_ = 0;
    status_ = Status::Absent;
    last_error_cause_ = ErrorCause::None;
    warmup_active_ = false;
    selftest_active_ = false;
    serial_valid_ = false;
    for (uint16_t &word : serial_words_) {
        word = 0;
    }
    start_ms_ = 0;
    first_ready_ms_ = 0;
    first_within_spec_ms_ = 0;
    last_measurement_ms_ = 0;
    measurement_reads_ = 0;
    measurement_frames_ok_ = 0;
    measurement_data_ok_ = 0;
    measurement_read_failures_ = 0;
    read_command_errors_ = 0;
    read_bytes_errors_ = 0;
    read_crc_errors_ = 0;
    status_not_ready_count_ = 0;
    status_not_within_spec_count_ = 0;
    status_invalid_count_ = 0;
    last_status_valid_ = false;
    last_raw_hcho_ = 0;
    last_raw_humidity_ = 0;
    last_raw_temperature_ = 0;
    last_status_byte_ = 0;
    last_status_reserved_ = 0;
    last_humidity_percent_ = 0.0f;
    last_temperature_c_ = 0.0f;
    late_start_phase_ = LateStartPhase::Idle;
    late_start_due_ms_ = 0;
    late_start_command_ms_ = 0;
    return true;
}

void Sfa40::start() {
    // A failed stop can leave the sensor state unknown. In that case we still
    // want start() to re-enter the recovery path instead of latching forever.
    if (measuring_ && !measurement_state_unknown_) {
        return;
    }
    if (!pingAddress()) {
        ok_ = false;
        status_ = Status::Absent;
        last_error_cause_ = ErrorCause::None;
        warmup_active_ = false;
        return;
    }

    if (!ensureIdleBeforeDetect()) {
        ok_ = false;
        status_ = Status::Fault;
        // A warm-restart stop failure can be an expected probe rejection when
        // the device on 0x5D is actually SFA30. SensorManager decides whether
        // to fall back, so avoid surfacing this as a user-facing warning here.
        if (!shouldFallbackToSfa30()) {
            LOGW(label(), "detect pre-stop failed (%s)", errorCauseLabel());
        }
        return;
    }

    if (!detectSensor()) {
        ok_ = false;
        status_ = Status::Fault;
        warmup_active_ = false;
        // Read/CRC/detect mismatches are expected while probing SFA40 on a
        // board that actually has SFA30 installed. Keep the fallback path
        // silent and let SensorManager report the final selected sensor.
        if (!shouldFallbackToSfa30()) {
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
    if (!writeCmd(Config::SFA40_CMD_START)) {
        ok_ = false;
        last_error_cause_ = ErrorCause::StartCommand;
        LOGW(label(), "start failed (%s)", errorCauseLabel());
        return;
    }
    const uint32_t start_command_ms = millis();
    delay(Config::SFA3X_START_DELAY_MS);
    measuring_ = true;
    measurement_state_unknown_ = false;
    warmup_active_ = true;
    selftest_active_ = false;
    ok_ = true;
    status_ = Status::Ok;
    last_error_cause_ = ErrorCause::None;
    last_poll_ms_ = start_command_ms;
    next_measurement_read_ms_ = start_command_ms + Config::SFA40_FIRST_READ_DELAY_MS;
    start_ms_ = start_command_ms;
    first_ready_ms_ = 0;
    first_within_spec_ms_ = 0;
}

void Sfa40::beginLateStart() {
    ok_ = false;
    status_ = Status::Absent;
    data_valid_ = false;
    has_new_data_ = false;
    warmup_active_ = false;
    serial_valid_ = false;
    late_start_due_ms_ = 0;
    late_start_command_ms_ = 0;
    late_start_phase_ = LateStartPhase::Ping;
}

CooperativeStart::Result Sfa40::finishLateStart(bool success, uint32_t now_ms) {
    (void)now_ms;
    late_start_phase_ = LateStartPhase::Idle;
    if (!success) {
        ok_ = false;
        return CooperativeStart::Result::Failed;
    }
    measuring_ = true;
    measurement_state_unknown_ = false;
    warmup_active_ = true;
    selftest_active_ = false;
    ok_ = true;
    status_ = Status::Ok;
    last_error_cause_ = ErrorCause::None;
    last_poll_ms_ = late_start_command_ms_;
    next_measurement_read_ms_ =
        late_start_command_ms_ + Config::SFA40_FIRST_READ_DELAY_MS;
    start_ms_ = late_start_command_ms_;
    first_ready_ms_ = 0;
    first_within_spec_ms_ = 0;
    return CooperativeStart::Result::Success;
}

CooperativeStart::Result Sfa40::pollLateStart(uint32_t now_ms) {
    switch (late_start_phase_) {
        case LateStartPhase::Idle:
            return CooperativeStart::Result::Idle;
        case LateStartPhase::Ping:
            if (!pingAddress()) {
                status_ = Status::Absent;
                last_error_cause_ = ErrorCause::None;
                return finishLateStart(false, now_ms);
            }
            late_start_phase_ = measurement_state_unknown_
                ? LateStartPhase::StopBeforeDetect
                : LateStartPhase::WriteId;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::StopBeforeDetect:
            LOGI(label(), "forcing idle before detect after warm restart");
            if (!writeCmd(Config::SFA40_CMD_STOP)) {
                status_ = Status::Fault;
                last_error_cause_ = ErrorCause::WarmRestartStop;
                return finishLateStart(false, now_ms);
            }
            late_start_due_ms_ = millis() + Config::SFA3X_STOP_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitStopBeforeDetect;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitStopBeforeDetect:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                measuring_ = false;
                measurement_state_unknown_ = false;
                late_start_phase_ = LateStartPhase::WriteId;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteId:
            if (!writeCmd(Config::SFA40_CMD_ID)) {
                ++read_command_errors_;
                status_ = Status::Fault;
                last_error_cause_ = ErrorCause::ReadCommand;
                return finishLateStart(false, now_ms);
            }
            late_start_phase_ = LateStartPhase::ReadId;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::ReadId: {
            uint8_t buf[9] = {};
            if (!readBytes(buf, sizeof(buf))) {
                ++read_bytes_errors_;
                status_ = Status::Fault;
                last_error_cause_ = ErrorCause::ReadBytes;
                return finishLateStart(false, now_ms);
            }
            bool nonzero = false;
            for (size_t i = 0; i < 3U; ++i) {
                const uint8_t *word = &buf[i * 3U];
                if (I2C::crc8(word, 2) != word[2]) {
                    ++read_crc_errors_;
                    status_ = Status::Fault;
                    last_error_cause_ = ErrorCause::ReadCrc;
                    return finishLateStart(false, now_ms);
                }
                serial_words_[i] =
                    (static_cast<uint16_t>(word[0]) << 8) | word[1];
                nonzero = nonzero || serial_words_[i] != 0U;
            }
            if (!nonzero) {
                status_ = Status::Fault;
                serial_valid_ = false;
                last_error_cause_ = ErrorCause::DetectSensor;
                return finishLateStart(false, now_ms);
            }
            serial_valid_ = true;
            last_error_cause_ = ErrorCause::None;
            status_ = Status::Fault;
            late_start_phase_ = measurement_state_unknown_
                ? LateStartPhase::StopBeforeStart
                : LateStartPhase::WriteStart;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::StopBeforeStart:
            if (!writeCmd(Config::SFA40_CMD_STOP)) {
                last_error_cause_ = ErrorCause::WarmRestartStop;
                return finishLateStart(false, now_ms);
            }
            late_start_due_ms_ = millis() + Config::SFA3X_STOP_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitStopBeforeStart;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitStopBeforeStart:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                measuring_ = false;
                measurement_state_unknown_ = false;
                late_start_phase_ = LateStartPhase::WriteStart;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WriteStart:
            if (!writeCmd(Config::SFA40_CMD_START)) {
                last_error_cause_ = ErrorCause::StartCommand;
                return finishLateStart(false, now_ms);
            }
            late_start_command_ms_ = millis();
            late_start_due_ms_ = late_start_command_ms_ + Config::SFA3X_START_DELAY_MS;
            late_start_phase_ = LateStartPhase::WaitStart;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::WaitStart:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            return finishLateStart(true, now_ms);
        default:
            return finishLateStart(false, now_ms);
    }
}

void Sfa40::stop() {
    if (!measuring_ && !measurement_state_unknown_) {
        return;
    }
    if (!writeCmd(Config::SFA40_CMD_STOP)) {
        measurement_state_unknown_ = true;
        return;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
    warmup_active_ = false;
    selftest_active_ = false;
    next_measurement_read_ms_ = 0;
}

bool Sfa40::readMeasurement(MeasurementReadResult &result) {
    ++measurement_reads_;
    uint16_t words[4];
    if (!readWords(Config::SFA40_CMD_READ_VALUES, words, 4, 0)) {
        ++measurement_read_failures_;
        return false;
    }

    ++measurement_frames_ok_;
    const uint32_t now = millis();
    const uint8_t status = static_cast<uint8_t>((words[3] >> 8) & 0xFFU);
    const bool not_ready = (status & SFA40_STATUS_NOT_READY) != 0U;
    const bool not_within_spec = (status & SFA40_STATUS_NOT_WITHIN_SPEC) != 0U;
    last_status_valid_ = true;
    last_raw_hcho_ = words[0];
    last_raw_humidity_ = words[1];
    last_raw_temperature_ = words[2];
    last_status_byte_ = status;
    last_status_reserved_ = static_cast<uint8_t>(words[3] & 0xFFU);
    last_humidity_percent_ = decodeHumidityPercent(words[1]);
    last_temperature_c_ = decodeTemperatureC(words[2]);
    last_measurement_ms_ = now;
    if (!not_ready && first_ready_ms_ == 0) {
        first_ready_ms_ = now;
    }
    if (!not_ready && !not_within_spec && first_within_spec_ms_ == 0) {
        first_within_spec_ms_ = now;
    }
    result.status_valid = true;
    // The datasheet documents only 11 (not ready), 10 (sub-spec warmup) and
    // 00 (within spec). Treat the undocumented 01 combination as invalid.
    if (not_ready && !not_within_spec) {
        ++status_invalid_count_;
        last_error_cause_ = ErrorCause::ReadStatus;
        return false;
    }
    result.warmup_active = not_ready || not_within_spec;
    if (not_ready) {
        ++status_not_ready_count_;
        last_error_cause_ = ErrorCause::ReadStatus;
        return false;
    }
    if (not_within_spec) {
        ++status_not_within_spec_count_;
    }

    result.hcho_ppb = static_cast<float>(words[0]) / 10.0f;
    ++measurement_data_ok_;
    return true;
}

bool Sfa40::readData(float &hcho_ppb) {
    MeasurementReadResult result;
    if (!readMeasurement(result)) {
        if (result.status_valid) {
            warmup_active_ = result.warmup_active;
        }
        return false;
    }
    warmup_active_ = result.warmup_active;
    hcho_ppb = result.hcho_ppb;
    return true;
}

bool Sfa40::startSelfTest() {
    if (status_ != Status::Ok || selftest_active_) {
        return false;
    }
    if (measurement_state_unknown_ && !ensureIdleBeforeStart()) {
        ok_ = false;
        status_ = Status::Fault;
        LOGW(label(), "self-test aborted (%s)", errorCauseLabel());
        return false;
    }
    if (measuring_) {
        stop();
        if (measuring_) {
            ok_ = false;
            status_ = Status::Fault;
            last_error_cause_ = ErrorCause::WarmRestartStop;
            LOGW(label(), "self-test start aborted (%s)", errorCauseLabel());
            return false;
        }
    }
    if (!writeCmd(Config::SFA40_CMD_START_SELFTEST)) {
        ok_ = false;
        status_ = Status::Fault;
        last_error_cause_ = ErrorCause::StartCommand;
        LOGW(label(), "self-test start failed (%s)", errorCauseLabel());
        return false;
    }

    invalidate();
    ok_ = true;
    status_ = Status::Ok;
    warmup_active_ = false;
    measuring_ = false;
    selftest_active_ = true;
    measurement_state_unknown_ = true;
    next_measurement_read_ms_ = 0;
    last_error_cause_ = ErrorCause::None;
    return true;
}

Sfa40::SelfTestStatus Sfa40::readSelfTestStatus(uint16_t &raw_result) {
    raw_result = 0;
    if (!selftest_active_) {
        return SelfTestStatus::Idle;
    }

    uint16_t words[1];
    if (!readWords(Config::SFA40_CMD_READ_SELFTEST, words, 1, 0)) {
        ok_ = false;
        status_ = Status::Fault;
        selftest_active_ = false;
        measuring_ = false;
        measurement_state_unknown_ = true;
        warmup_active_ = false;
        return SelfTestStatus::ReadError;
    }

    raw_result = words[0];
    if (raw_result == Config::SFA40_SELFTEST_RUNNING_RAW) {
        return SelfTestStatus::Running;
    }

    selftest_active_ = false;
    measurement_state_unknown_ = true;
    if (raw_result == 0U) {
        ok_ = true;
        status_ = Status::Ok;
        last_error_cause_ = ErrorCause::None;
        return SelfTestStatus::Passed;
    }

    ok_ = false;
    status_ = Status::Fault;
    return SelfTestStatus::Failed;
}

void Sfa40::poll() {
    if (!ok_ || !measuring_ || selftest_active_) {
        return;
    }
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - next_measurement_read_ms_) < 0) {
        return;
    }
    if (now - last_poll_ms_ < Config::SFA40_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    float hcho_ppb = 0.0f;
    if (!readData(hcho_ppb)) {
        if (last_error_cause_ == ErrorCause::ReadStatus) {
            if (warmup_active_) {
                status_ = Status::Ok;
                return;
            }
            const bool was_fault = (status_ == Status::Fault);
            status_ = Status::Fault;
            fail_count_ = 0;
            if (!was_fault) {
                LOGW(label(), "read values failed (%s)", errorCauseLabel());
            }
            return;
        }
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

bool Sfa40::takeNewData(float &hcho_ppb) {
    if (!has_new_data_ || !data_valid_) {
        return false;
    }
    hcho_ppb = last_hcho_ppb_;
    has_new_data_ = false;
    return true;
}

bool Sfa40::shouldFallbackToSfa30() const {
    if (status_ != Status::Fault) {
        return false;
    }
    switch (last_error_cause_) {
        case ErrorCause::DetectSensor:
        case ErrorCause::WarmRestartStop:
        case ErrorCause::ReadCommand:
        case ErrorCause::ReadBytes:
        case ErrorCause::ReadCrc:
            return true;
        default:
            return false;
    }
}

void Sfa40::invalidate() {
    data_valid_ = false;
    has_new_data_ = false;
}

bool Sfa40::detectSensor() {
    uint16_t words[3];
    if (!readWords(Config::SFA40_CMD_ID, words, 3, 0)) {
        return false;
    }
    for (uint16_t word : words) {
        if (word != 0) {
            for (size_t i = 0; i < 3; ++i) {
                serial_words_[i] = words[i];
            }
            serial_valid_ = true;
            last_error_cause_ = ErrorCause::None;
            return true;
        }
    }
    serial_valid_ = false;
    last_error_cause_ = ErrorCause::DetectSensor;
    return false;
}

bool Sfa40::readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms) {
    if (!writeCmd(cmd)) {
        last_error_cause_ = ErrorCause::ReadCommand;
        ++read_command_errors_;
        return false;
    }
    delay(delay_ms);
    const size_t bytes = words * 3;
    uint8_t buf[12];
    if (bytes > sizeof(buf)) {
        last_error_cause_ = ErrorCause::ReadBytes;
        ++read_bytes_errors_;
        return false;
    }
    if (!readBytes(buf, bytes)) {
        last_error_cause_ = ErrorCause::ReadBytes;
        ++read_bytes_errors_;
        return false;
    }
    for (size_t i = 0; i < words; ++i) {
        const uint8_t *p = &buf[i * 3];
        if (I2C::crc8(p, 2) != p[2]) {
            last_error_cause_ = ErrorCause::ReadCrc;
            ++read_crc_errors_;
            return false;
        }
        out[i] = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    }
    return true;
}

Sfa40::Diagnostics Sfa40::diagnostics() const {
    Diagnostics out;
    out.measuring = measuring_;
    out.measurement_state_unknown = measurement_state_unknown_;
    out.data_valid = data_valid_;
    out.has_new_data = has_new_data_;
    out.warmup_active = warmup_active_;
    out.selftest_active = selftest_active_;
    out.status = status_;
    out.last_error = errorCauseLabel();
    out.serial_valid = serial_valid_;
    for (size_t i = 0; i < 3; ++i) {
        out.serial_words[i] = serial_words_[i];
    }
    out.start_ms = start_ms_;
    out.first_ready_ms = first_ready_ms_;
    out.first_within_spec_ms = first_within_spec_ms_;
    out.last_measurement_ms = last_measurement_ms_;
    out.last_data_ms = last_data_ms_;
    out.measurement_reads = measurement_reads_;
    out.measurement_frames_ok = measurement_frames_ok_;
    out.measurement_data_ok = measurement_data_ok_;
    out.measurement_read_failures = measurement_read_failures_;
    out.read_command_errors = read_command_errors_;
    out.read_bytes_errors = read_bytes_errors_;
    out.read_crc_errors = read_crc_errors_;
    out.status_not_ready_count = status_not_ready_count_;
    out.status_not_within_spec_count = status_not_within_spec_count_;
    out.status_invalid_count = status_invalid_count_;
    out.last_status_valid = last_status_valid_;
    out.raw_hcho = last_raw_hcho_;
    out.raw_humidity = last_raw_humidity_;
    out.raw_temperature = last_raw_temperature_;
    out.status_byte = last_status_byte_;
    out.status_reserved = last_status_reserved_;
    out.hcho_ppb = last_hcho_ppb_;
    out.humidity_percent = last_humidity_percent_;
    out.temperature_c = last_temperature_c_;
    return out;
}

bool Sfa40::ensureIdleBeforeDetect() {
    if (!measurement_state_unknown_) {
        return true;
    }

    LOGI(label(), "forcing idle before detect after warm restart");
    if (!writeCmd(Config::SFA40_CMD_STOP)) {
        last_error_cause_ = ErrorCause::WarmRestartStop;
        return false;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
    last_error_cause_ = ErrorCause::None;
    return true;
}

bool Sfa40::ensureIdleBeforeStart() {
    if (!measurement_state_unknown_) {
        return true;
    }

    LOGI(label(), "forcing idle after warm restart");
    if (!writeCmd(Config::SFA40_CMD_STOP)) {
        last_error_cause_ = ErrorCause::WarmRestartStop;
        return false;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
    last_error_cause_ = ErrorCause::None;
    return true;
}

bool Sfa40::pingAddress() {
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

bool Sfa40::writeCmd(uint16_t cmd) {
    return I2C::write_cmd(Config::SFA3X_ADDR, cmd, nullptr, 0) == ESP_OK;
}

bool Sfa40::readBytes(uint8_t *buf, size_t len) {
    return I2C::read_bytes(Config::SFA3X_ADDR, buf, len) == ESP_OK;
}

const char *Sfa40::errorCauseLabel() const {
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
        case ErrorCause::ReadStatus:
            return "status";
        default:
            return "unknown";
    }
}
