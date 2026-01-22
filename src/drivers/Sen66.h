// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>

class StorageManager;
#include "config/AppConfig.h"
#include "config/AppData.h"

class Sen66 {
public:
    bool begin();
    void setOffsets(float temp_offset, float hum_offset);
    void loadVocState(StorageManager &storage);
    void saveVocState(StorageManager &storage);
    void clearVocState(StorageManager &storage);
    void scheduleRetry(uint32_t delay_ms);
    uint32_t retryAtMs() const { return retry_at_ms_; }

    bool start(bool asc_enabled);
    bool stop();
    void poll(SensorData &data, bool &changed);
    bool readValues(SensorData &out);
    bool calibrateFRC(uint16_t ref_ppm, bool has_pressure, float pressure_hpa, uint16_t &correction);
    void updatePressure(float pressure_hpa);
    bool setAscEnabled(bool enabled);
    bool deviceReset();

    bool isOk() const { return ok_; }
    bool isBusy() const { return busy_; }
    bool isMeasuring() const { return measuring_; }
    bool isWarmupActive() const;
    uint32_t lastDataMs() const { return last_data_ms_; }

private:
    bool writeCmdWithWord(uint16_t cmd, uint16_t word);
    bool writeCmdWithWords(uint16_t cmd, const uint16_t *words, size_t count);
    bool setAmbientPressure(uint16_t hpa);
    bool getDataReady(bool &ready);
    bool readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms);
    bool getVocState(uint8_t *state, size_t len);
    bool setVocState(const uint8_t *state, size_t len);
    bool readStatus(uint16_t &status);
    bool setTemperatureOffsetParams(float offset_c, float slope, uint16_t time_constant_s, uint16_t slot);
    bool applyTempOffsetParams();
    bool startMeasurement();
    bool forceIdle();
    bool setAscRaw(bool enabled);
    bool getAsc(bool &enabled);
    bool performFrc(uint16_t ref_ppm, uint16_t &correction);
    int smoothCo2(int new_val);

    float temp_offset_ = 0.0f;
    float hum_offset_ = 0.0f;
    bool ok_ = false;
    bool busy_ = false;
    bool measuring_ = false;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_status_ms_ = 0;
    uint8_t fail_count_ = 0;
    uint16_t status_last_ = 0;
    uint32_t retry_at_ms_ = 0;
    uint32_t measure_start_ms_ = 0;
    uint32_t last_pressure_ms_ = 0;
    uint16_t last_pressure_hpa_ = 0;
    uint8_t pressure_fail_count_ = 0;
    uint32_t last_data_ms_ = 0;
    bool temp_offset_hw_active_ = false;
    float temp_offset_hw_value_ = 0.0f;

    uint8_t voc_state_[Config::SEN66_VOC_STATE_LEN] = {};
    bool voc_state_valid_ = false;
    uint32_t last_voc_state_save_ms_ = 0;

    bool co2_invalid_logged_ = false;
    uint32_t co2_invalid_since_ms_ = 0;
    bool co2_first_ = true;
    int co2_readings_[5] = { 400, 400, 400, 400, 400 };
    int co2_idx_ = 0;
};
