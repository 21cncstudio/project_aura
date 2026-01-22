#pragma once

#include "Arduino.h"
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
    bool update_pressure_called = false;
    bool clear_voc_called = false;
    bool load_voc_called = false;
    bool save_voc_called = false;
    bool device_reset_called = false;
    bool asc_enabled = true;
    uint32_t last_data_ms = 0;
    uint32_t retry_at_ms = 0;
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
    void setOffsets(float, float) {}
    void loadVocState(StorageManager &) { state().load_voc_called = true; }
    void saveVocState(StorageManager &) { state().save_voc_called = true; }
    void clearVocState(StorageManager &) { state().clear_voc_called = true; }
    void scheduleRetry(uint32_t delay_ms) { state().retry_at_ms = millis() + delay_ms; }
    uint32_t retryAtMs() const { return state().retry_at_ms; }

    bool start(bool asc_enabled) {
        state().start_called = true;
        state().asc_enabled = asc_enabled;
        state().ok = state().start_ok;
        return state().start_ok;
    }
    bool stop() { return true; }
    void poll(SensorData &data, bool &changed) {
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
        correction = 0;
        return true;
    }
    void updatePressure(float pressure_hpa) {
        state().update_pressure_called = true;
        state().last_pressure = pressure_hpa;
    }
    bool setAscEnabled(bool enabled) {
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
