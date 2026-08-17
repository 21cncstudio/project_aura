// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <atomic>
#include "config/AppData.h"
#include "core/StartupProbePolicy.h"
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

    // Permanently stop runtime access to the shared I2C bus. The gate is
    // restored only by begin(), which represents a fresh sensor startup.
    void disableSharedI2c();
    // Wait for operations admitted before disableSharedI2c() to drain. This
    // wait is bounded and returns false when timeout_ms expires.
    bool waitForSharedI2cIdle(uint32_t timeout_ms);
    // Stop the active continuous HCHO measurement during controlled teardown.
    // Call only after disableSharedI2c() and waitForSharedI2cIdle() succeeded.
    bool stopHchoForRestart();
    bool isSharedI2cAvailable() const {
        return shared_i2c_available_.load(std::memory_order_acquire);
    }
#ifdef UNIT_TEST
    void setSharedI2cActiveUsersForTest(uint32_t count) {
        shared_i2c_active_users_.store(count, std::memory_order_release);
    }
    uint32_t sen66NextRetryMsForTest() const {
        return sen66_probe_.nextDueMs();
    }
#endif

    void setOffsets(float temp_offset, float hum_offset);
    bool isInitialized() const { return initialized_; }
    bool isOk() const { return initialized_ && sen66_.isOk(); }
    bool isBusy() const { return initialized_ && sen66_.isBusy(); }
    bool isDpsOk() const { return isPressureOk(); }
    bool isSfaOk() const { return currentHchoStatus() == SfaStatus::Ok; }
    bool isSfaPresent() const { return currentHchoStatus() != SfaStatus::Absent; }
    bool hasSfaFault() const { return currentHchoStatus() == SfaStatus::Fault; }
    bool isSfaWarmupActive() const { return currentHchoWarmupActive(); }
    bool isSfaDetecting() const {
        return initialized_ && hcho_sensor_type_ == HCHO_SENSOR_NONE && hcho_probe_.pending();
    }
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
    bool isPressureDetecting() const {
        return initialized_ && pressure_sensor_ == PRESSURE_NONE && pressure_probe_.pending();
    }
    PressureSensorType pressureSensorType() const { return pressure_sensor_; }
    const char *pressureSensorLabel() const;
    const char *hchoSensorLabel() const;
    HchoSensorType hchoSensorType() const { return hcho_sensor_type_; }
    Sfa40::Diagnostics sfa40Diagnostics() const { return sfa40_.diagnostics(); }
    bool deviceReset();
    void scheduleRetry(uint32_t delay_ms);
    bool start(bool asc_enabled);
    bool isWarmupActive() const { return initialized_ && sen66_.isWarmupActive(); }
    bool isSen66Detecting() const {
        return initialized_ && !sen66_.isOk() &&
               (sen66_probe_.pending() || late_probe_kind_ == LateProbeKind::Sen66);
    }
    bool isSen66StartupProbePending() const {
        return initialized_ && !sen66_.isOk() && sen66_probe_.pending();
    }
    uint32_t lastDataMs() const { return initialized_ ? sen66_.lastDataMs() : 0; }
    bool setAscEnabled(bool enabled);
    bool calibrateFrc(uint16_t ref_ppm, bool has_pressure, float pressure_hpa,
                      uint16_t &correction);

    bool resetVocState(StorageManager &storage, uint32_t retry_delay_ms);

private:
    class SharedI2cLease {
    public:
        explicit SharedI2cLease(SensorManager &owner, uint32_t wait_ms = 0U);
        ~SharedI2cLease();

        SharedI2cLease(const SharedI2cLease &) = delete;
        SharedI2cLease &operator=(const SharedI2cLease &) = delete;

        explicit operator bool() const { return acquired_; }

    private:
        SensorManager &owner_;
        bool acquired_ = false;
    };

    enum class LateProbeKind : uint8_t {
        None = 0,
        Pressure,
        Hcho,
        Sen66,
    };
    enum class PressureLateStage : uint8_t {
        Bmp580 = 0,
        Bmp3xx,
        Dps310,
    };
    enum class HchoLateStage : uint8_t {
        Sfa30Warm = 0,
        Sfa40,
        Sfa30Fallback,
    };

    bool detectPressureSensor();
    bool detectHchoSensor();
    void startNextLateProbe(uint32_t now_ms, bool co2_asc_enabled);
    void pollActiveLateProbe(uint32_t now_ms, PollResult &result);
    void finishPressureLateProbe(bool success, PollResult &result);
    void finishHchoLateProbe(bool success, PollResult &result);
    void finishSen66LateProbe(bool success, PollResult &result);
    SfaStatus currentHchoStatus() const;
    bool currentHchoWarmupActive() const;
    bool currentHchoTakeNewData(float &hcho_ppb);
    void currentHchoInvalidate();
    uint32_t currentHchoLastDataMs() const;
    float currentHchoMinPpb() const;
    float currentHchoMaxPpb() const;
    bool acquireSharedI2c(uint32_t wait_ms);
    void releaseSharedI2c();

    static constexpr uint32_t COMMAND_ACQUIRE_TIMEOUT_MS = 500U;

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
    StartupProbePolicy::State pressure_probe_;
    StartupProbePolicy::State hcho_probe_;
    StartupProbePolicy::State sen66_probe_;
    PressureSensorType pressure_sensor_ = PRESSURE_NONE;
    LateProbeKind late_probe_kind_ = LateProbeKind::None;
    PressureLateStage pressure_late_stage_ = PressureLateStage::Bmp580;
    HchoLateStage hcho_late_stage_ = HchoLateStage::Sfa40;
    bool late_driver_started_ = false;
    bool late_sen66_asc_enabled_ = true;
    uint8_t late_probe_cursor_ = 0;
    bool initialized_ = false;
    std::atomic<bool> shared_i2c_available_{true};
    std::atomic<uint32_t> shared_i2c_active_users_{0};
};
