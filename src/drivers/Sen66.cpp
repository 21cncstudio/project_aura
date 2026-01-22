// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Sen66.h"
#include <math.h>
#include <string.h>
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "core/I2CHelper.h"
#include "modules/StorageManager.h"

bool Sen66::begin() {
    ok_ = false;
    busy_ = false;
    measuring_ = false;
    last_poll_ms_ = 0;
    last_status_ms_ = 0;
    fail_count_ = 0;
    status_last_ = 0;
    retry_at_ms_ = 0;
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
    co2_first_ = true;
    co2_idx_ = 0;
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

void Sen66::scheduleRetry(uint32_t delay_ms) {
    retry_at_ms_ = millis() + delay_ms;
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
    if (!setTemperatureOffsetParams(
            temp_offset_,
            Config::SEN66_TEMP_OFFSET_SLOPE,
            Config::SEN66_TEMP_OFFSET_TIME_S,
            Config::SEN66_TEMP_OFFSET_SLOT)) {
        return false;
    }
    temp_offset_hw_active_ = true;
    temp_offset_hw_value_ = temp_offset_;
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
    return true;
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

    if (pm1_raw != 0xFFFF) {
        out.pm1 = pm1_raw / 10.0f;
    } else {
        out.pm1 = 0.0f;
    }

    if (pm4_raw != 0xFFFF) {
        out.pm4 = pm4_raw / 10.0f;
    } else {
        out.pm4 = 0.0f;
    }

    out.pm25_valid = (pm25_raw != 0xFFFF);
    if (out.pm25_valid) {
        out.pm25 = pm25_raw / 10.0f;
    } else {
        out.pm25 = 0.0f;
    }

    out.pm10_valid = (pm10_raw != 0xFFFF);
    if (out.pm10_valid) {
        out.pm10 = pm10_raw / 10.0f;
    } else {
        out.pm10 = 0.0f;
    }

    out.pm_valid = out.pm25_valid || out.pm10_valid;

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
        float temp_offset = temp_offset_;
        if (temp_offset_hw_active_) {
            temp_offset -= temp_offset_hw_value_;
        }
        out.temperature = (t_raw / 200.0f) - Config::BASE_TEMP_OFFSET + temp_offset;
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

bool Sen66::stop() {
    if (!measuring_) {
        return true;
    }
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_STOP, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(Config::SEN66_STOP_DELAY_MS);
    measuring_ = false;
    return true;
}

bool Sen66::startMeasurement() {
    if (measuring_) {
        return true;
    }
    if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_START, nullptr, 0) != ESP_OK) {
        return false;
    }
    delay(Config::SEN66_START_DELAY_MS);
    measuring_ = true;
    if (measure_start_ms_ == 0) {
        measure_start_ms_ = millis();
    }
    last_voc_state_save_ms_ = millis();
    return true;
}

bool Sen66::setAscRaw(bool enabled) {
    if (!writeCmdWithWord(Config::SEN66_CMD_ASC, enabled ? 1 : 0)) {
        return false;
    }
    delay(Config::SEN66_CMD_DELAY_MS);
    for (int attempt = 0; attempt < 3; ++attempt) {
        bool readback = false;
        if (getAsc(readback) && readback == enabled) {
            return true;
        }
        delay(Config::SEN66_CMD_DELAY_MS);
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
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (I2C::write_cmd(Config::SEN66_ADDR, Config::SEN66_CMD_STOP, nullptr, 0) == ESP_OK) {
            delay(Config::SEN66_STOP_DELAY_MS);
            measuring_ = false;
            return true;
        }
        delay(Config::SEN66_CMD_DELAY_MS);
    }
    return false;
}

bool Sen66::start(bool asc_enabled) {
    busy_ = true;
    forceIdle();
    if (!applyTempOffsetParams()) {
        LOGW("SEN66", "temp offset set failed");
    } else {
        LOGI("SEN66", "temp offset: %.1f C", temp_offset_);
    }
    if (voc_state_valid_) {
        if (!setVocState(voc_state_, sizeof(voc_state_))) {
            LOGW("SEN66", "VOC state restore failed");
        } else {
            LOGI("SEN66", "VOC state restored");
        }
    }
    if (!setAscRaw(asc_enabled)) {
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
    ok_ = true;
    busy_ = false;
    return true;
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

bool Sen66::readStatus(uint16_t &status) {
    return readWords(Config::SEN66_CMD_READ_STATUS, &status, 1, Config::SEN66_CMD_DELAY_MS);
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
        uint16_t status = 0;
        if (readStatus(status)) {
            if (status != 0 && status != status_last_) {
                Logger::log(Logger::Debug, "SEN66", "status: 0x%04X", status);
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
