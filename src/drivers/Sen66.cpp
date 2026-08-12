// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Sen66.h"
#include <math.h>
#include <string.h>
#include "core/BootState.h"
#include "core/Logger.h"
#include "core/Sen66Status.h"
#include "config/AppConfig.h"
#include "core/I2CHelper.h"
#include "modules/StorageManager.h"

namespace {

bool sen66AscDefaultsKnownAfterReset() {
    return boot_peripherals_cold_start;
}

bool sen66StateUnknownAfterBoot() {
    return !boot_peripherals_cold_start;
}

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

} // namespace

bool Sen66::begin() {
    ok_ = false;
    busy_ = false;
    measuring_ = false;
    last_poll_ms_ = 0;
    last_status_ms_ = 0;
    fail_count_ = 0;
    status_last_ = 0;
    measure_start_ms_ = 0;
    last_pressure_ms_ = 0;
    last_pressure_hpa_ = 0;
    pressure_fail_count_ = 0;
    last_data_ms_ = 0;
    last_voc_state_save_ms_ = 0;
    voc_state_valid_ = false;
    temp_offset_hw_active_ = false;
    temp_offset_hw_value_ = 0.0f;
    co2_invalid_logged_ = false;
    co2_invalid_since_ms_ = 0;
    resetCo2Smoother();
    asc_default_known_ = sen66AscDefaultsKnownAfterReset();
    measurement_state_unknown_ = sen66StateUnknownAfterBoot();
    late_start_phase_ = LateStartPhase::Idle;
    late_start_due_ms_ = 0;
    late_start_asc_enabled_ = true;
    late_start_stop_attempt_ = 0;
    late_start_asc_write_attempt_ = 0;
    late_start_asc_verify_attempt_ = 0;
    return true;
}

void Sen66::setOffsets(float temp_offset, float hum_offset) {
    temp_offset_ = temp_offset;
    hum_offset_ = hum_offset;
    if (ok_ && !busy_) {
        if (!applyTempOffsetParams()) {
            LOGW("SEN66", "temp offset set failed");
        }
    }
}

void Sen66::loadVocState(StorageManager &storage) {
    if (storage.loadVocState(voc_state_, sizeof(voc_state_))) {
        voc_state_valid_ = true;
    } else {
        voc_state_valid_ = false;
    }
}

void Sen66::saveVocState(StorageManager &storage) {
    if (!ok_ || busy_ || !measuring_) {
        return;
    }
    uint32_t now = millis();
    if (now - last_voc_state_save_ms_ < Config::SEN66_VOC_STATE_SAVE_MS) {
        return;
    }
    uint8_t state[Config::SEN66_VOC_STATE_LEN] = {};
    if (!getVocState(state, sizeof(state))) {
        LOGW("SEN66", "VOC state read failed");
        last_voc_state_save_ms_ = now;
        return;
    }
    memcpy(voc_state_, state, sizeof(voc_state_));
    voc_state_valid_ = true;
    storage.saveVocState(voc_state_, sizeof(voc_state_));
    last_voc_state_save_ms_ = now;
    LOGD("SEN66", "VOC state saved");
}

void Sen66::clearVocState(StorageManager &storage) {
    storage.clearVocState();
    voc_state_valid_ = false;
    memset(voc_state_, 0, sizeof(voc_state_));
}

bool Sen66::writeCmdWithWord(uint16_t cmd, uint16_t word) {
    uint8_t params[3] = {
        static_cast<uint8_t>(word >> 8),
        static_cast<uint8_t>(word & 0xFF),
        0
    };
    params[2] = I2C::crc8(params, 2);
    return I2C::write_cmd(Config::SEN66_ADDR, cmd, params, sizeof(params)) == ESP_OK;
}

bool Sen66::writeCmdWithWords(uint16_t cmd, const uint16_t *words, size_t count) {
    if (!words || count == 0 || count > 8) {
        return false;
    }
    uint8_t params[8 * 3] = {};
    for (size_t i = 0; i < count; ++i) {
        const size_t off = i * 3;
        params[off] = static_cast<uint8_t>(words[i] >> 8);
        params[off + 1] = static_cast<uint8_t>(words[i] & 0xFF);
        params[off + 2] = I2C::crc8(&params[off], 2);
    }
    return I2C::write_cmd(Config::SEN66_ADDR, cmd, params, count * 3) == ESP_OK;
}

bool Sen66::setAmbientPressure(uint16_t hpa) {
    if (!writeCmdWithWord(Config::SEN66_CMD_AMBIENT_PRESSURE, hpa)) {
        return false;
    }
    delay(Config::SEN66_CMD_DELAY_MS);
    return true;
}

bool Sen66::setTemperatureOffsetParams(float offset_c, float slope, uint16_t time_constant_s, uint16_t slot) {
    const int16_t offset_scaled = static_cast<int16_t>(lroundf(offset_c * 200.0f));
    const int16_t slope_scaled = static_cast<int16_t>(lroundf(slope * 10000.0f));
    const uint16_t words[4] = {
        static_cast<uint16_t>(offset_scaled),
        static_cast<uint16_t>(slope_scaled),
        time_constant_s,
        slot
    };
    if (!writeCmdWithWords(Config::SEN66_CMD_TEMP_OFFSET, words, 4)) {
        return false;
    }
    delay(Config::SEN66_CMD_DELAY_MS);
    return true;
}

bool Sen66::applyTempOffsetParams() {
    const float desired_correction = desiredTempCorrectionC();
    if (!setTemperatureOffsetParams(
            desired_correction,
            Config::SEN66_TEMP_OFFSET_SLOPE,
            Config::SEN66_TEMP_OFFSET_TIME_S,
            Config::SEN66_TEMP_OFFSET_SLOT)) {
        return false;
    }
    temp_offset_hw_active_ = true;
    temp_offset_hw_value_ = desired_correction;
    return true;
}

bool Sen66::getDataReady(bool &ready) {
    uint8_t buf[3];
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_DATA_READY, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(Config::SEN66_CMD_DELAY_MS);
    if (I2C::read_bytes(Config::SEN66_ADDR, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    if (I2C::crc8(buf, 2) != buf[2]) {
        return false;
    }
    ready = (buf[1] == 0x01);
    return true;
}

bool Sen66::readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms) {
    if (I2C::write_cmd(Config::SEN66_ADDR, cmd, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(delay_ms);
    const size_t bytes = words * 3;
    uint8_t buf[27];
    if (bytes > sizeof(buf)) {
        return false;
    }
    if (I2C::read_bytes(Config::SEN66_ADDR, buf, bytes) != ESP_OK) {
        return false;
    }
    for (size_t i = 0; i < words; ++i) {
        const uint8_t *p = &buf[i * 3];
        if (I2C::crc8(p, 2) != p[2]) {
            return false;
        }
        out[i] = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    }
    return true;
}

bool Sen66::getVocState(uint8_t *state, size_t len) {
    if (!state || len < Config::SEN66_VOC_STATE_LEN) {
        return false;
    }
    uint16_t words[4];
    if (!readWords(Config::SEN66_CMD_VOC_STATE, words, 4, Config::SEN66_CMD_DELAY_MS)) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        state[i * 2] = static_cast<uint8_t>(words[i] >> 8);
        state[i * 2 + 1] = static_cast<uint8_t>(words[i] & 0xFF);
    }
    return true;
}

bool Sen66::setVocState(const uint8_t *state, size_t len) {
    if (!state || len < Config::SEN66_VOC_STATE_LEN) {
        return false;
    }
    uint16_t words[4];
    for (size_t i = 0; i < 4; ++i) {
        words[i] = (static_cast<uint16_t>(state[i * 2]) << 8) |
                   static_cast<uint16_t>(state[i * 2 + 1]);
    }
    if (!writeCmdWithWords(Config::SEN66_CMD_VOC_STATE, words, 4)) {
        return false;
    }
    delay(Config::SEN66_CMD_DELAY_MS);
    return true;
}

bool Sen66::deviceReset() {
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_DEVICE_RESET, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(Config::SEN66_DEVICE_RESET_DELAY_MS);
    ok_ = false;
    measuring_ = false;
    measure_start_ms_ = 0;
    last_voc_state_save_ms_ = 0;
    temp_offset_hw_active_ = false;
    temp_offset_hw_value_ = 0.0f;
    last_status_ms_ = 0;
    status_last_ = 0;
    fail_count_ = 0;
    co2_invalid_logged_ = false;
    co2_invalid_since_ms_ = 0;
    resetCo2Smoother();
    asc_default_known_ = true;
    measurement_state_unknown_ = false;
    return true;
}

void Sen66::resetCo2Smoother() {
    co2_first_ = true;
    co2_idx_ = 0;
    for (int &reading : co2_readings_) {
        reading = 400;
    }
}

float Sen66::desiredTempCorrectionC() const {
    return temp_offset_ - Config::BASE_TEMP_OFFSET;
}

bool Sen66::isWarmupActive() const {
    if (!ok_ || !measuring_ || measure_start_ms_ == 0) {
        return false;
    }
    return (millis() - measure_start_ms_) < Config::SEN66_GAS_WARMUP_MS;
}

int Sen66::smoothCo2(int new_val) {
    if (co2_first_) {
        for (int i = 0; i < 5; i++) {
            co2_readings_[i] = new_val;
        }
        co2_first_ = false;
    }

    long sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += co2_readings_[i];
    }
    int avg = static_cast<int>(sum / 5);

    if (abs(new_val - avg) > 150) {
        for (int i = 0; i < 5; i++) {
            co2_readings_[i] = new_val;
        }
        return new_val;
    }

    co2_readings_[co2_idx_] = new_val;
    co2_idx_ = (co2_idx_ + 1) % 5;

    sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += co2_readings_[i];
    }
    return static_cast<int>(sum / 5);
}

bool Sen66::readValues(SensorData &out) {
    uint16_t words[9];
    if (!readWords(Config::SEN66_CMD_READ_VALUES, words, 9, Config::SEN66_CMD_DELAY_MS)) {
        return false;
    }

    const uint16_t pm1_raw = words[0];
    const uint16_t pm25_raw = words[1];
    const uint16_t pm4_raw = words[2];
    const uint16_t pm10_raw = words[3];

    const int16_t rh_raw = static_cast<int16_t>(words[4]);
    const int16_t t_raw = static_cast<int16_t>(words[5]);
    const int16_t voc_raw = static_cast<int16_t>(words[6]);
    const int16_t nox_raw = static_cast<int16_t>(words[7]);

    const uint16_t co2_raw = words[8];

    out.pm1_valid = (pm1_raw != 0xFFFF);
    if (out.pm1_valid) {
        out.pm1 = pm1_raw / 10.0f;
    } else {
        out.pm1 = 0.0f;
    }

    out.pm25_valid = (pm25_raw != 0xFFFF);
    if (out.pm25_valid) {
        out.pm25 = pm25_raw / 10.0f;
    } else {
        out.pm25 = 0.0f;
    }

    out.pm4_valid = (pm4_raw != 0xFFFF);
    if (out.pm4_valid) {
        out.pm4 = pm4_raw / 10.0f;
    } else {
        out.pm4 = 0.0f;
    }

    out.pm10_valid = (pm10_raw != 0xFFFF);
    if (out.pm10_valid) {
        out.pm10 = pm10_raw / 10.0f;
    } else {
        out.pm10 = 0.0f;
    }

    if (!readNumberConcentration(out)) {
        out.pm05_valid = false;
        out.pm05 = 0.0f;
    }

    out.pm_valid = out.pm1_valid || out.pm25_valid || out.pm4_valid || out.pm10_valid;

    out.hum_valid = (rh_raw != 0x7FFF);
    if (out.hum_valid) {
        out.humidity = (rh_raw / 100.0f) + hum_offset_;
        if (!isfinite(out.humidity)) {
            out.hum_valid = false;
            out.humidity = 0.0f;
        }
    } else {
        out.humidity = 0.0f;
    }

    out.temp_valid = (t_raw != 0x7FFF);
    if (out.temp_valid) {
        float temp_correction = desiredTempCorrectionC();
        if (temp_offset_hw_active_) {
            temp_correction -= temp_offset_hw_value_;
        }
        out.temperature = (t_raw / 200.0f) + temp_correction;
    } else {
        out.temperature = 0.0f;
    }

    out.voc_valid = (voc_raw != 0x7FFF);
    if (out.voc_valid) {
        out.voc_index = static_cast<int>(lroundf(voc_raw / 10.0f));
    } else {
        out.voc_index = 0;
    }

    out.nox_valid = (nox_raw != 0x7FFF);
    if (out.nox_valid) {
        out.nox_index = static_cast<int>(lroundf(nox_raw / 10.0f));
    } else {
        out.nox_index = 0;
    }

    out.co2_valid = (co2_raw != 0xFFFF);
    if (out.co2_valid) {
        out.co2 = smoothCo2(static_cast<int>(co2_raw));
        co2_invalid_since_ms_ = 0;
        co2_invalid_logged_ = false;
    } else {
        out.co2 = 0;
        if (co2_invalid_since_ms_ == 0) {
            co2_invalid_since_ms_ = millis();
        } else if (!co2_invalid_logged_ &&
                   (millis() - co2_invalid_since_ms_) >= Config::SEN66_CO2_INVALID_MS) {
            LOGW("SEN66", "CO2 invalid >15s (0xFFFF)");
            co2_invalid_logged_ = true;
        }
    }

    return true;
}

bool Sen66::readNumberConcentration(SensorData &out) {
    uint16_t words[5];
    if (!readWords(Config::SEN66_CMD_READ_NUM_CONC, words, 5, Config::SEN66_CMD_DELAY_MS)) {
        return false;
    }

    const uint16_t pm05_raw = words[0];
    out.pm05_valid = (pm05_raw != 0xFFFF);
    if (out.pm05_valid) {
        out.pm05 = pm05_raw / 10.0f;
    } else {
        out.pm05 = 0.0f;
    }

    return true;
}

bool Sen66::stop() {
    if (!measuring_) {
        return true;
    }
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_STOP, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(Config::SEN66_STOP_DELAY_MS);
    measuring_ = false;
    measurement_state_unknown_ = false;
    return true;
}

bool Sen66::startMeasurement() {
    if (measuring_) {
        return true;
    }
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_START, nullptr, 0) != ESP_OK) {
        // A failed I2C transaction cannot prove that the sensor did not
        // accept START. Force the next attempt through STOP before sending
        // configuration commands that require idle mode.
        measurement_state_unknown_ = true;
        return false;
    }
    delay(Config::SEN66_START_DELAY_MS);
    measuring_ = true;
    measurement_state_unknown_ = false;
    if (measure_start_ms_ == 0) {
        measure_start_ms_ = millis();
    }
    last_voc_state_save_ms_ = millis();
    return true;
}

bool Sen66::setAscRaw(bool enabled) {
    bool current = false;
    const bool initial_read_ok = getAsc(current);
    if (initial_read_ok && current == enabled) {
        return true;
    }

    uint8_t write_failures = 0;
    uint8_t verify_read_failures = initial_read_ok ? 0 : 1;
    bool saw_verify_value = initial_read_ok;
    bool last_verify_value = current;

    for (uint8_t write_attempt = 0; write_attempt < Config::SEN66_ASC_WRITE_ATTEMPTS; ++write_attempt) {
        if (!writeCmdWithWord(Config::SEN66_CMD_ASC, enabled ? 1 : 0)) {
            ++write_failures;
            delay(Config::SEN66_ASC_RETRY_DELAY_MS);
            continue;
        }

        delay(Config::SEN66_ASC_SETTLE_DELAY_MS);
        for (uint8_t verify_attempt = 0; verify_attempt < Config::SEN66_ASC_VERIFY_ATTEMPTS; ++verify_attempt) {
            bool readback = false;
            if (getAsc(readback)) {
                saw_verify_value = true;
                last_verify_value = readback;
                if (readback == enabled) {
                    return true;
                }
            } else {
                ++verify_read_failures;
            }
            delay(Config::SEN66_ASC_RETRY_DELAY_MS);
        }
    }

    uint32_t status = 0;
    const bool status_ok = readStatus(status);
    LOGW("SEN66",
         "ASC apply detail: target=%s initial_read=%s write_failures=%u verify_read_failures=%u last_verify=%s%s",
         enabled ? "enable" : "disable",
         initial_read_ok ? (current ? "enabled" : "disabled") : "failed",
         static_cast<unsigned>(write_failures),
         static_cast<unsigned>(verify_read_failures),
         saw_verify_value ? (last_verify_value ? "enabled" : "disabled") : "n/a",
         status_ok ? "" : ", status=read-failed");
    if (status_ok && status != 0) {
        LOGW("SEN66", "ASC apply device status: 0x%08lX", static_cast<unsigned long>(status));
    }

    return false;
}

bool Sen66::getAsc(bool &enabled) {
    uint16_t value = 0;
    if (!readWords(Config::SEN66_CMD_ASC, &value, 1, Config::SEN66_CMD_DELAY_MS)) {
        return false;
    }
    enabled = (value == 1);
    return true;
}

bool Sen66::performFrc(uint16_t ref_ppm, uint16_t &correction) {
    if (!writeCmdWithWord(Config::SEN66_CMD_FRC, ref_ppm)) {
        return false;
    }
    delay(Config::SEN66_FRC_DELAY_MS);
    uint8_t buf[3] = {};
    if (I2C::read_bytes(Config::SEN66_ADDR, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    if (I2C::crc8(buf, 2) != buf[2]) {
        return false;
    }
    correction = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return true;
}

void Sen66::updatePressure(float pressure_hpa) {
    if (!ok_ || busy_) {
        return;
    }
    if (!isfinite(pressure_hpa)) {
        return;
    }
    uint32_t now = millis();
    if (last_pressure_ms_ != 0 &&
        (now - last_pressure_ms_ < Config::SEN66_PRESSURE_UPDATE_MS)) {
        return;
    }

    uint16_t hpa = static_cast<uint16_t>(lroundf(pressure_hpa));
    if (hpa < Config::SEN66_PRESSURE_MIN_HPA) {
        hpa = Config::SEN66_PRESSURE_MIN_HPA;
    } else if (hpa > Config::SEN66_PRESSURE_MAX_HPA) {
        hpa = Config::SEN66_PRESSURE_MAX_HPA;
    }

    if (setAmbientPressure(hpa)) {
        last_pressure_hpa_ = hpa;
        last_pressure_ms_ = now;
        pressure_fail_count_ = 0;
    } else {
        if (++pressure_fail_count_ == 3) {
            LOGW("SEN66", "ambient pressure set failed");
            pressure_fail_count_ = 0;
        }
    }
}

bool Sen66::forceIdle() {
    if (!measuring_ && !measurement_state_unknown_) {
        return true;
    }
    if (measurement_state_unknown_) {
        LOGI("SEN66", "forcing idle after warm restart");
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_STOP, nullptr, 0) == ESP_OK) {
            delay(Config::SEN66_STOP_DELAY_MS);
            measuring_ = false;
            measurement_state_unknown_ = false;
            return true;
        }
        delay(Config::SEN66_CMD_DELAY_MS);
    }
    if (measurement_state_unknown_) {
        LOGW("SEN66", "STOP failed while resyncing state, resetting sensor");
        if (deviceReset()) {
            return true;
        }
    }
    return false;
}

bool Sen66::start(bool asc_enabled) {
    busy_ = true;
    if (!forceIdle()) {
        ok_ = false;
        measuring_ = false;
        busy_ = false;
        return false;
    }
    if (!applyTempOffsetParams()) {
        LOGW("SEN66", "temp offset set failed");
    } else {
        LOGI("SEN66",
             "temp compensation via HW: base %.1f C, user %.1f C",
             Config::BASE_TEMP_OFFSET,
             temp_offset_);
    }
    if (voc_state_valid_) {
        if (!setVocState(voc_state_, sizeof(voc_state_))) {
            LOGW("SEN66", "VOC state restore failed");
        } else {
            LOGI("SEN66", "VOC state restored");
        }
    }
    if (asc_enabled && asc_default_known_) {
        // ASC is volatile and defaults to enabled after a hard reset.
        Logger::log(Logger::Info, "SEN66", "ASC enabled (default after reset)");
    } else if (!setAscRaw(asc_enabled)) {
        Logger::log(Logger::Warn, "SEN66",
                    "ASC set failed (%s)",
                    asc_enabled ? "enable" : "disable");
    } else {
        Logger::log(Logger::Info, "SEN66",
                    "ASC %s",
                    asc_enabled ? "enabled" : "disabled");
    }
    if (!startMeasurement()) {
        ok_ = false;
        busy_ = false;
        return false;
    }
    asc_default_known_ = false;
    ok_ = true;
    busy_ = false;
    return true;
}

void Sen66::beginLateStart(bool asc_enabled) {
    busy_ = true;
    ok_ = false;
    late_start_asc_enabled_ = asc_enabled;
    late_start_stop_attempt_ = 0;
    late_start_asc_write_attempt_ = 0;
    late_start_asc_verify_attempt_ = 0;
    late_start_due_ms_ = 0;
    late_start_phase_ = (!measuring_ && !measurement_state_unknown_)
        ? LateStartPhase::TempWrite
        : LateStartPhase::StopWrite;
    if (measurement_state_unknown_) {
        LOGI("SEN66", "forcing idle after warm restart");
    }
}

CooperativeStart::Result Sen66::finishLateStart(bool success, uint32_t now_ms) {
    late_start_phase_ = LateStartPhase::Idle;
    busy_ = false;
    if (!success) {
        ok_ = false;
        measuring_ = false;
        return CooperativeStart::Result::Failed;
    }
    measuring_ = true;
    measurement_state_unknown_ = false;
    if (measure_start_ms_ == 0) {
        measure_start_ms_ = now_ms;
    }
    last_voc_state_save_ms_ = now_ms;
    asc_default_known_ = false;
    ok_ = true;
    return CooperativeStart::Result::Success;
}

bool Sen66::readLateWord(uint16_t &value) {
    uint8_t buf[3] = {};
    if (I2C::read_bytes(Config::SEN66_ADDR, buf, sizeof(buf)) != ESP_OK ||
        I2C::crc8(buf, 2) != buf[2]) {
        return false;
    }
    value = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return true;
}

CooperativeStart::Result Sen66::pollLateStart(uint32_t now_ms) {
    switch (late_start_phase_) {
        case LateStartPhase::Idle:
            return CooperativeStart::Result::Idle;
        case LateStartPhase::StopWrite:
            ++late_start_stop_attempt_;
            if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_STOP, nullptr, 0) == ESP_OK) {
                late_start_due_ms_ = millis() + Config::SEN66_STOP_DELAY_MS;
                late_start_phase_ = LateStartPhase::StopWait;
            } else if (late_start_stop_attempt_ < 3U) {
                late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
                late_start_phase_ = LateStartPhase::StopRetryWait;
            } else if (measurement_state_unknown_) {
                LOGW("SEN66", "STOP failed while resyncing state, resetting sensor");
                late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
                late_start_phase_ = LateStartPhase::DeviceResetRetryWait;
            } else {
                // The failed transaction cannot prove that the device stayed
                // in measurement mode or accepted STOP. Keep that ambiguity
                // across the bounded outer retry so the next attempt starts
                // with STOP again instead of sending configuration commands.
                measurement_state_unknown_ = true;
                return finishLateStart(false, now_ms);
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::StopWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                measuring_ = false;
                measurement_state_unknown_ = false;
                late_start_phase_ = LateStartPhase::TempWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::StopRetryWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::StopWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DeviceResetRetryWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::DeviceResetWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DeviceResetWrite:
            if (I2C::write_cmd(Config::SEN66_ADDR,
                               Config::SEN66_CMD_DEVICE_RESET,
                               nullptr,
                               0) != ESP_OK) {
                return finishLateStart(false, now_ms);
            }
            late_start_due_ms_ = millis() + Config::SEN66_DEVICE_RESET_DELAY_MS;
            late_start_phase_ = LateStartPhase::DeviceResetWait;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::DeviceResetWait:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            measuring_ = false;
            measurement_state_unknown_ = false;
            measure_start_ms_ = 0;
            last_voc_state_save_ms_ = 0;
            temp_offset_hw_active_ = false;
            temp_offset_hw_value_ = 0.0f;
            last_status_ms_ = 0;
            status_last_ = 0;
            fail_count_ = 0;
            co2_invalid_logged_ = false;
            co2_invalid_since_ms_ = 0;
            resetCo2Smoother();
            asc_default_known_ = true;
            late_start_phase_ = LateStartPhase::TempWrite;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::TempWrite: {
            const float desired_correction = desiredTempCorrectionC();
            const int16_t offset_scaled =
                static_cast<int16_t>(lroundf(desired_correction * 200.0f));
            const int16_t slope_scaled =
                static_cast<int16_t>(lroundf(Config::SEN66_TEMP_OFFSET_SLOPE * 10000.0f));
            const uint16_t words[4] = {
                static_cast<uint16_t>(offset_scaled),
                static_cast<uint16_t>(slope_scaled),
                Config::SEN66_TEMP_OFFSET_TIME_S,
                Config::SEN66_TEMP_OFFSET_SLOT,
            };
            if (writeCmdWithWords(Config::SEN66_CMD_TEMP_OFFSET, words, 4)) {
                temp_offset_hw_active_ = true;
                temp_offset_hw_value_ = desired_correction;
                LOGI("SEN66", "temp compensation via HW: base %.1f C, user %.1f C",
                     Config::BASE_TEMP_OFFSET, temp_offset_);
            } else {
                LOGW("SEN66", "temp offset set failed");
            }
            late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
            late_start_phase_ = LateStartPhase::TempWait;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::TempWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = voc_state_valid_
                    ? LateStartPhase::VocWrite
                    : LateStartPhase::AscReadWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::VocWrite: {
            uint16_t words[4] = {};
            for (size_t i = 0; i < 4U; ++i) {
                words[i] = (static_cast<uint16_t>(voc_state_[i * 2U]) << 8) |
                           voc_state_[i * 2U + 1U];
            }
            if (writeCmdWithWords(Config::SEN66_CMD_VOC_STATE, words, 4)) {
                LOGI("SEN66", "VOC state restored");
            } else {
                LOGW("SEN66", "VOC state restore failed");
            }
            late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
            late_start_phase_ = LateStartPhase::VocWait;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::VocWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::AscReadWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscReadWrite:
            if (late_start_asc_enabled_ && asc_default_known_) {
                LOGI("SEN66", "ASC enabled (default after reset)");
                late_start_phase_ = LateStartPhase::StartWrite;
                return CooperativeStart::Result::InProgress;
            }
            if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_ASC, nullptr, 0) != ESP_OK) {
                late_start_phase_ = LateStartPhase::AscApplyWrite;
                return CooperativeStart::Result::InProgress;
            }
            late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
            late_start_phase_ = LateStartPhase::AscReadWait;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscReadWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::AscReadResponse;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscReadResponse: {
            uint16_t value = 0;
            if (readLateWord(value) && ((value == 1U) == late_start_asc_enabled_)) {
                LOGI("SEN66", "ASC %s", late_start_asc_enabled_ ? "enabled" : "disabled");
                late_start_phase_ = LateStartPhase::StartWrite;
            } else {
                late_start_phase_ = LateStartPhase::AscApplyWrite;
            }
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::AscApplyWrite:
            if (writeCmdWithWord(Config::SEN66_CMD_ASC,
                                 late_start_asc_enabled_ ? 1U : 0U)) {
                late_start_asc_verify_attempt_ = 0;
                late_start_due_ms_ = millis() + Config::SEN66_ASC_SETTLE_DELAY_MS;
                late_start_phase_ = LateStartPhase::AscApplySettle;
            } else {
                ++late_start_asc_write_attempt_;
                if (late_start_asc_write_attempt_ >= Config::SEN66_ASC_WRITE_ATTEMPTS) {
                    late_start_due_ms_ = millis() + Config::SEN66_ASC_RETRY_DELAY_MS;
                    late_start_phase_ = LateStartPhase::AscFinalDelay;
                } else {
                    late_start_due_ms_ = millis() + Config::SEN66_ASC_RETRY_DELAY_MS;
                    late_start_phase_ = LateStartPhase::AscRetryWait;
                }
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscApplySettle:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::AscVerifyWrite;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscVerifyWrite:
            if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_ASC, nullptr, 0) != ESP_OK) {
                ++late_start_asc_verify_attempt_;
                late_start_due_ms_ = millis() + Config::SEN66_ASC_RETRY_DELAY_MS;
                late_start_phase_ = LateStartPhase::AscRetryWait;
                return CooperativeStart::Result::InProgress;
            }
            late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
            late_start_phase_ = LateStartPhase::AscVerifyWait;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscVerifyWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::AscVerifyResponse;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscVerifyResponse: {
            uint16_t value = 0;
            if (readLateWord(value) && ((value == 1U) == late_start_asc_enabled_)) {
                LOGI("SEN66", "ASC %s", late_start_asc_enabled_ ? "enabled" : "disabled");
                late_start_phase_ = LateStartPhase::StartWrite;
                return CooperativeStart::Result::InProgress;
            }
            ++late_start_asc_verify_attempt_;
            late_start_due_ms_ = millis() + Config::SEN66_ASC_RETRY_DELAY_MS;
            late_start_phase_ = LateStartPhase::AscRetryWait;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::AscRetryWait:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            if (late_start_asc_verify_attempt_ < Config::SEN66_ASC_VERIFY_ATTEMPTS &&
                late_start_asc_write_attempt_ < Config::SEN66_ASC_WRITE_ATTEMPTS) {
                late_start_phase_ = late_start_asc_verify_attempt_ == 0U
                    ? LateStartPhase::AscApplyWrite
                    : LateStartPhase::AscVerifyWrite;
            } else {
                ++late_start_asc_write_attempt_;
                if (late_start_asc_write_attempt_ < Config::SEN66_ASC_WRITE_ATTEMPTS) {
                    late_start_asc_verify_attempt_ = 0;
                    late_start_phase_ = LateStartPhase::AscApplyWrite;
                } else {
                    LOGW("SEN66", "ASC set failed (%s)",
                         late_start_asc_enabled_ ? "enable" : "disable");
                    late_start_phase_ = LateStartPhase::AscFinalStatusWrite;
                }
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscFinalDelay:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            LOGW("SEN66", "ASC set failed (%s)",
                 late_start_asc_enabled_ ? "enable" : "disable");
            late_start_phase_ = LateStartPhase::AscFinalStatusWrite;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscFinalStatusWrite:
            if (I2C::write_cmd(Config::SEN66_ADDR,
                               Config::SEN66_CMD_READ_STATUS,
                               nullptr,
                               0) != ESP_OK) {
                LOGW("SEN66", "ASC apply detail: status=read-failed");
                late_start_phase_ = LateStartPhase::StartWrite;
                return CooperativeStart::Result::InProgress;
            }
            late_start_due_ms_ = millis() + Config::SEN66_CMD_DELAY_MS;
            late_start_phase_ = LateStartPhase::AscFinalStatusWait;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscFinalStatusWait:
            if (deadlineReached(now_ms, late_start_due_ms_)) {
                late_start_phase_ = LateStartPhase::AscFinalStatusResponse;
            }
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::AscFinalStatusResponse: {
            uint8_t buf[6] = {};
            if (I2C::read_bytes(Config::SEN66_ADDR, buf, sizeof(buf)) != ESP_OK ||
                I2C::crc8(&buf[0], 2) != buf[2] ||
                I2C::crc8(&buf[3], 2) != buf[5]) {
                LOGW("SEN66", "ASC apply detail: status=read-failed");
            } else {
                const uint32_t status =
                    (static_cast<uint32_t>(buf[0]) << 24) |
                    (static_cast<uint32_t>(buf[1]) << 16) |
                    (static_cast<uint32_t>(buf[3]) << 8) |
                    static_cast<uint32_t>(buf[4]);
                if (status != 0U) {
                    LOGW("SEN66", "ASC apply device status: 0x%08lX",
                         static_cast<unsigned long>(status));
                }
            }
            late_start_phase_ = LateStartPhase::StartWrite;
            return CooperativeStart::Result::InProgress;
        }
        case LateStartPhase::StartWrite:
            if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_START, nullptr, 0) != ESP_OK) {
                // Delivery is ambiguous after a failed transaction. Preserve
                // that ambiguity so the bounded outer retry starts with STOP.
                measurement_state_unknown_ = true;
                return finishLateStart(false, now_ms);
            }
            late_start_due_ms_ = millis() + Config::SEN66_START_DELAY_MS;
            late_start_phase_ = LateStartPhase::StartWait;
            return CooperativeStart::Result::InProgress;
        case LateStartPhase::StartWait:
            if (!deadlineReached(now_ms, late_start_due_ms_)) {
                return CooperativeStart::Result::InProgress;
            }
            return finishLateStart(true, now_ms);
        default:
            return finishLateStart(false, now_ms);
    }
}

bool Sen66::setAscEnabled(bool enabled) {
    if (!ok_) {
        return false;
    }
    busy_ = true;
    bool was_measuring = measuring_;
    if (was_measuring && !stop()) {
        busy_ = false;
        return false;
    }
    bool ok = setAscRaw(enabled);
    if (ok) {
        Logger::log(Logger::Info, "SEN66",
                    "ASC %s",
                    enabled ? "enabled" : "disabled");
    } else {
        Logger::log(Logger::Warn, "SEN66",
                    "ASC set failed (%s)",
                    enabled ? "enable" : "disable");
    }
    if (was_measuring && !startMeasurement()) {
        LOGW("SEN66", "start failed after ASC");
    }
    busy_ = false;
    return ok;
}

bool Sen66::calibrateFRC(uint16_t ref_ppm, bool has_pressure, float pressure_hpa,
                         uint16_t &correction) {
    if (!ok_) {
        return false;
    }
    busy_ = true;
    if (!stop()) {
        LOGW("SEN66", "stop failed for FRC");
        busy_ = false;
        return false;
    }

    if (has_pressure && isfinite(pressure_hpa)) {
        uint16_t hpa = static_cast<uint16_t>(lroundf(pressure_hpa));
        if (hpa < Config::SEN66_PRESSURE_MIN_HPA) {
            hpa = Config::SEN66_PRESSURE_MIN_HPA;
        } else if (hpa > Config::SEN66_PRESSURE_MAX_HPA) {
            hpa = Config::SEN66_PRESSURE_MAX_HPA;
        }
        if (!setAmbientPressure(hpa)) {
            LOGW("SEN66", "ambient pressure set failed");
        }
    }

    if (!performFrc(ref_ppm, correction)) {
        LOGW("SEN66", "FRC failed");
        busy_ = false;
        return false;
    }
    if (correction == 0xFFFF) {
        LOGW("SEN66", "FRC correction invalid");
    } else {
        Logger::log(Logger::Info, "SEN66",
                    "FRC OK. correction: %u",
                    static_cast<unsigned>(correction));
    }

    if (!startMeasurement()) {
        LOGW("SEN66", "start failed after FRC");
    }
    busy_ = false;
    return true;
}

bool Sen66::readStatus(uint32_t &status) {
    uint16_t words[2] = {};
    if (!readWords(Config::SEN66_CMD_READ_STATUS, words, 2, Config::SEN66_CMD_DELAY_MS)) {
        return false;
    }
    status = (static_cast<uint32_t>(words[0]) << 16) | static_cast<uint32_t>(words[1]);
    return true;
}

void Sen66::poll(SensorData &data, bool &changed) {
    changed = false;
    if (!ok_ || busy_ || !measuring_) {
        return;
    }
    uint32_t now = millis();
    if (now - last_poll_ms_ < Config::SEN66_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    if (now - last_status_ms_ >= Config::SEN66_STATUS_MS) {
        uint32_t status = 0;
        if (readStatus(status)) {
            if (status != status_last_) {
                if (status != 0) {
                    Logger::log(Logger::Debug, "SEN66", "status: 0x%08lX",
                                static_cast<unsigned long>(status));
                }
                Sen66Status::Transition transitions[8] = {};
                const size_t transition_count =
                    Sen66Status::collectTransitions(status_last_,
                                                    status,
                                                    transitions,
                                                    sizeof(transitions) / sizeof(transitions[0]));
                for (size_t i = 0; i < transition_count; ++i) {
                    Logger::log(transitions[i].level, "SEN66", "%s", transitions[i].message);
                }
            }
            status_last_ = status;
        }
        last_status_ms_ = now;
    }

    bool ready = false;
    if (!getDataReady(ready)) {
        if (++fail_count_ == 3) {
            LOGW("SEN66", "data ready read failed");
            fail_count_ = 0;
        }
        return;
    }
    if (!ready) {
        return;
    }

    SensorData newData = data;
    if (readValues(newData)) {
        changed = (memcmp(&data, &newData, sizeof(SensorData)) != 0);
        data = newData;
        last_data_ms_ = now;
        fail_count_ = 0;
    } else {
        if (++fail_count_ == 3) {
            LOGW("SEN66", "read values failed");
            fail_count_ = 0;
        }
    }
}
