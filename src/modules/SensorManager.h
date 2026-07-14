// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include "config/AppData.h"
#include "drivers/Bmp3xx.h"
#include "drivers/Bmp580.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "drivers/Dps310.h"
#include "drivers/Sen0466.h"
#include "drivers/Sen66.h"
#include "drivers/Sfa30.h"
#include "drivers/Sfa40.h"

class StorageManager;
class PressureHistory;

class SensorManager {
public:
    using SfaStatus = Sfa40::Status;

    struct PollResult {
        bool data_changed = false;
        bool warmup_changed = false;
    };
    enum PressureSensorType : uint8_t {
        PRESSURE_NONE = 0,
        PRESSURE_DPS310,
        PRESSURE_BMP58X,
        PRESSURE_BMP3XX
    };
    enum HchoSensorType : uint8_t {
        HCHO_SENSOR_NONE = 0,
        HCHO_SENSOR_SFA30,
        HCHO_SENSOR_SFA40
    };

    void begin(StorageManager &storage, float temp_offset, float hum_offset);
    PollResult poll(SensorData &data, StorageManager &storage, PressureHistory &pressure_history,
                    bool co2_asc_enabled);

    void setOffsets(float temp_offset, float hum_offset);
    bool isInitialized() const { return initialized_; }
    bool isOk() const { return initialized_ && sen66_.isOk(); }
    bool isBusy() const { return initialized_ && sen66_.isBusy(); }
    bool isDpsOk() const { return isPressureOk(); }
    bool isSfaOk() const { return currentHchoStatus() == SfaStatus::Ok; }
    bool isSfaPresent() const { return currentHchoStatus() != SfaStatus::Absent; }
    bool hasSfaFault() const { return currentHchoStatus() == SfaStatus::Fault; }
    bool isSfaWarmupActive() const { return currentHchoWarmupActive(); }
    SfaStatus sfaStatus() const { return currentHchoStatus(); }
    bool isCoPresent() const { return sen0466_.isPresent(); }
    bool isCoValid() const { return sen0466_.isDataValid(); }
    bool isCoWarmupActive() const { return sen0466_.isWarmupActive(); }
    DfrOptionalGasSensor::OptionalGasType optionalGasType() const { return optional_gas_.optionalGasType(); }
    bool isOptionalGasPresent() const { return optional_gas_.isPresent(); }
    bool isOptionalGasValid() const { return optional_gas_.isDataValid(); }
    bool isOptionalGasWarmupActive() const { return optional_gas_.isWarmupActive(); }
    bool isNh3Present() const {
        return optional_gas_.isPresent() &&
               optional_gas_.optionalGasType() == DfrOptionalGasSensor::OptionalGasType::NH3;
    }
    bool isNh3Valid() const {
        return optional_gas_.isDataValid() &&
               optional_gas_.optionalGasType() == DfrOptionalGasSensor::OptionalGasType::NH3;
    }
    bool isNh3WarmupActive() const {
        return optional_gas_.isWarmupActive() &&
               optional_gas_.optionalGasType() == DfrOptionalGasSensor::OptionalGasType::NH3;
    }
    bool isPressureOk() const;
    PressureSensorType pressureSensorType() const { return pressure_sensor_; }
    const char *pressureSensorLabel() const;
    const char *hchoSensorLabel() const;
    HchoSensorType hchoSensorType() const { return hcho_sensor_type_; }
    Sfa40::Diagnostics sfa40Diagnostics() const { return sfa40_.diagnostics(); }
    bool deviceReset() { return initialized_ && sen66_.deviceReset(); }
    void scheduleRetry(uint32_t delay_ms) {
        if (!initialized_) return;
        sen66_start_attempts_ = 0;
        sen66_retry_exhausted_logged_ = false;
        sen66_.scheduleRetry(delay_ms);
    }
    uint32_t retryAtMs() const { return initialized_ ? sen66_.retryAtMs() : 0; }
    bool start(bool asc_enabled) { return initialized_ && sen66_.start(asc_enabled); }
    bool isWarmupActive() const { return initialized_ && sen66_.isWarmupActive(); }
    uint32_t lastDataMs() const { return initialized_ ? sen66_.lastDataMs() : 0; }
    bool setAscEnabled(bool enabled) { return initialized_ && sen66_.setAscEnabled(enabled); }
    bool calibrateFrc(uint16_t ref_ppm, bool has_pressure, float pressure_hpa,
                      uint16_t &correction) {
        return initialized_ && sen66_.calibrateFRC(ref_ppm, has_pressure, pressure_hpa, correction);
    }

    void clearVocState(StorageManager &storage);

private:
    SfaStatus currentHchoStatus() const;
    bool currentHchoWarmupActive() const;
    bool currentHchoTakeNewData(float &hcho_ppb);
    void currentHchoInvalidate();
    uint32_t currentHchoLastDataMs() const;
    float currentHchoMinPpb() const;
    float currentHchoMaxPpb() const;

    Bmp3xx bmp3xx_;
    Bmp580 bmp580_;
    Dps310 dps310_;
    Sfa30 sfa30_;
    Sfa40 sfa40_;
    Sen0466 sen0466_;
    DfrOptionalGasSensor optional_gas_;
    Sen66 sen66_;
    HchoSensorType hcho_sensor_type_ = HCHO_SENSOR_NONE;
    bool warmup_active_last_ = false;
    bool sfa_warmup_active_last_ = false;
    SfaStatus sfa_status_last_ = SfaStatus::Absent;
    uint8_t sen66_start_attempts_ = 0;
    bool sen66_retry_exhausted_logged_ = false;
    PressureSensorType pressure_sensor_ = PRESSURE_NONE;
    bool initialized_ = false;
};
