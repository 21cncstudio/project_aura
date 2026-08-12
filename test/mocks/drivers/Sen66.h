#pragma once

#include "Arduino.h"
#include "core/CooperativeStart.h"
#include "modules/StorageManager.h"
#include "config/AppData.h"

struct Sen66TestState {
    bool ok = true;
    bool busy = false;
    bool warmup = false;
    bool provide_data = false;
    bool poll_changed = false;
    bool update_last_data_on_poll = false;
    bool start_ok = true;
    bool start_called = false;
    uint8_t start_call_count = 0;
    uint8_t late_start_begin_count = 0;
    uint8_t late_start_poll_count = 0;
    uint8_t late_start_polls_to_complete = 1;
    bool late_start_active = false;
    uint32_t set_offsets_call_count = 0;
    uint32_t poll_call_count = 0;
    uint32_t frc_call_count = 0;
    uint32_t asc_call_count = 0;
    bool update_pressure_called = false;
    bool clear_voc_called = false;
    bool load_voc_called = false;
    bool save_voc_called = false;
    bool device_reset_called = false;
    bool asc_enabled = true;
    uint32_t last_data_ms = 0;
    float last_pressure = 0.0f;
    SensorData poll_data;
};

class Sen66 {
public:
    static Sen66TestState &state() {
        static Sen66TestState instance;
        return instance;
    }

    bool begin() { return true; }
    void setOffsets(float, float) { ++state().set_offsets_call_count; }
    void loadVocState(StorageManager &) { state().load_voc_called = true; }
    void saveVocState(StorageManager &) { state().save_voc_called = true; }
    void clearVocState(StorageManager &) { state().clear_voc_called = true; }
    bool start(bool asc_enabled) {
        state().start_called = true;
        ++state().start_call_count;
        state().asc_enabled = asc_enabled;
        state().ok = state().start_ok;
        return state().start_ok;
    }
    void beginLateStart(bool asc_enabled) {
        ++state().late_start_begin_count;
        state().late_start_poll_count = 0;
        state().late_start_active = true;
        state().busy = true;
        state().asc_enabled = asc_enabled;
    }
    CooperativeStart::Result pollLateStart(uint32_t) {
        if (!state().late_start_active) return CooperativeStart::Result::Idle;
        if (++state().late_start_poll_count < state().late_start_polls_to_complete) {
            return CooperativeStart::Result::InProgress;
        }
        state().late_start_active = false;
        state().busy = false;
        state().ok = state().start_ok;
        return state().start_ok ? CooperativeStart::Result::Success
                                : CooperativeStart::Result::Failed;
    }
    bool isLateStartActive() const { return state().late_start_active; }
    bool stop() { return true; }
    void poll(SensorData &data, bool &changed) {
        ++state().poll_call_count;
        changed = state().poll_changed;
        if (state().provide_data) {
            data = state().poll_data;
        }
        if (state().update_last_data_on_poll) {
            state().last_data_ms = millis();
        }
    }
    bool readValues(SensorData &) { return false; }
    bool calibrateFRC(uint16_t, bool, float, uint16_t &correction) {
        ++state().frc_call_count;
        correction = 0;
        return true;
    }
    void updatePressure(float pressure_hpa) {
        state().update_pressure_called = true;
        state().last_pressure = pressure_hpa;
    }
    bool setAscEnabled(bool enabled) {
        ++state().asc_call_count;
        state().asc_enabled = enabled;
        return true;
    }
    bool deviceReset() {
        state().device_reset_called = true;
        return true;
    }

    bool isOk() const { return state().ok; }
    bool isBusy() const { return state().busy; }
    bool isMeasuring() const { return true; }
    bool isWarmupActive() const { return state().warmup; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
};
