// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include "config/AppData.h"
#include "core/CooperativeStart.h"

class StorageManager;

class Sen69c {
public:
    static constexpr uint32_t Co2ConditioningMs = 24000;
    static constexpr uint32_t HchoUnavailableMs = 60000;
    bool begin();
    void setIdentity(const char *serial);
    void setOffsets(float temperature, float humidity);
    void loadVocState(StorageManager &storage);
    void saveVocState(StorageManager &storage);
    void clearVocState(StorageManager &storage);
    void beginLateStart(bool asc);
    CooperativeStart::Result pollLateStart(uint32_t now);
    bool start(bool asc);
    bool stop();
    bool deviceReset();
    bool setAscEnabled(bool enabled);
    bool calibrateFRC(uint16_t reference, bool has_pressure, float pressure, uint16_t &correction);
    void updatePressure(float pressure);
    void poll(SensorData &data, bool &changed);
    bool isOk() const { return ok_; }
    bool isBusy() const { return start_phase_ != StartPhase::Idle; }
    bool isWarmupActive() const;
    bool isCo2WarmupActive() const;
    bool isHchoWarmupActive() const;
    bool hasHchoFault() const;
    int acquiredCo2() const { return acquired_co2_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    uint32_t pm05LastDataMs() const { return pm05_last_ms_; }
    uint32_t hchoLastDataMs() const { return hcho_last_ms_; }
    bool takeHcho(float &value);
    void invalidateHcho() { hcho_new_ = false; }

private:
    enum class StartPhase : uint8_t {
        Idle, Guard, Stop, Reset, Temperature, Voc, AscRead, AscReceive,
        AscWrite, AscVerify, AscVerifyReceive, Start, Started
    };
    enum class PollPhase : uint8_t { Idle, Ready, Values, Numbers, Status, ClearedStatus };
    struct VocRecord {
        uint8_t version = 1;
        char serial[32]{};
        uint8_t state[8]{};
    };
    static constexpr const char *VocPath = "/sen69c_voc_v1.bin";
    bool readWords(uint16_t command, uint16_t *words, size_t count, uint32_t delay_ms = 20);
    bool applyTemperature();
    bool startMeasurement();
    bool applyAsc(bool enabled);
    bool conditioning() const;
    void failPoll();
    void acceptStatus(uint32_t raw, bool allow_recovery);
    void decodeValues(const uint16_t *words, SensorData &data);
    CooperativeStart::Result finishStart(bool success);
    bool ok_ = false;
    bool measuring_ = false;
    bool state_unknown_ = false;
    bool asc_ = true;
    bool voc_valid_ = false;
    bool temperature_applied_ = false;
    bool temperature_dirty_ = false;
    bool pressure_pending_ = false;
    bool co2_ready_ = false;
    bool hcho_new_ = false;
    float temp_offset_ = 0, humidity_offset_ = 0, applied_offset_ = 0, hcho_ = 0;
    VocRecord voc_{};
    uint32_t start_ms_ = 0, reset_ms_ = 0, gas_start_ms_ = 0, due_ms_ = 0;
    uint32_t unknown_since_ms_ = 0;
    uint32_t last_poll_ms_ = 0, last_data_ms_ = 0, hcho_last_ms_ = 0;
    uint32_t last_voc_ms_ = 0, last_pressure_ms_ = 0, status_ = 0;
    uint32_t status_clear_due_ms_ = 0, errors_absent_once_ = 0;
    uint8_t failures_ = 0;
    int acquired_co2_ = 0;
    uint32_t pm05_last_ms_ = 0;
    uint16_t pressure_hpa_ = 1013;
    StartPhase start_phase_ = StartPhase::Idle;
    PollPhase poll_phase_ = PollPhase::Idle;
};
