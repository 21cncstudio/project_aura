#pragma once

#include <Arduino.h>
#include "config/AppData.h"
#include "drivers/Bmp580.h"
#include "drivers/Dps310.h"
#include "drivers/Sen66.h"
#include "drivers/Sfa3x.h"

class StorageManager;
class PressureHistory;

class SensorManager {
public:
    struct PollResult {
        bool data_changed = false;
        bool warmup_changed = false;
    };
    enum PressureSensorType : uint8_t {
        PRESSURE_NONE = 0,
        PRESSURE_DPS310,
        PRESSURE_BMP580
    };

    void begin(StorageManager &storage, float temp_offset, float hum_offset);
    PollResult poll(SensorData &data, StorageManager &storage, PressureHistory &pressure_history,
                    bool co2_asc_enabled);

    void setOffsets(float temp_offset, float hum_offset);
    bool isOk() const { return sen66_.isOk(); }
    bool isBusy() const { return sen66_.isBusy(); }
    bool isDpsOk() const { return isPressureOk(); }
    bool isSfaOk() const { return sfa3x_.isOk(); }
    bool isPressureOk() const;
    PressureSensorType pressureSensorType() const { return pressure_sensor_; }
    const char *pressureSensorLabel() const;
    bool deviceReset() { return sen66_.deviceReset(); }
    void scheduleRetry(uint32_t delay_ms) { sen66_.scheduleRetry(delay_ms); }
    uint32_t retryAtMs() const { return sen66_.retryAtMs(); }
    bool start(bool asc_enabled) { return sen66_.start(asc_enabled); }
    bool isWarmupActive() const { return sen66_.isWarmupActive(); }
    uint32_t lastDataMs() const { return sen66_.lastDataMs(); }
    bool setAscEnabled(bool enabled) { return sen66_.setAscEnabled(enabled); }
    bool calibrateFrc(uint16_t ref_ppm, bool has_pressure, float pressure_hpa,
                      uint16_t &correction) {
        return sen66_.calibrateFRC(ref_ppm, has_pressure, pressure_hpa, correction);
    }

    void clearVocState(StorageManager &storage);

private:
    Bmp580 bmp580_;
    Dps310 dps310_;
    Sfa3x sfa3x_;
    Sen66 sen66_;
    bool warmup_active_last_ = false;
    PressureSensorType pressure_sensor_ = PRESSURE_NONE;
};
