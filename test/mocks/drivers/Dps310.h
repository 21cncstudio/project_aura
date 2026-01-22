#pragma once

#include "Arduino.h"

struct Dps310TestState {
    bool ok = true;
    bool start_ok = true;
    bool pressure_valid = false;
    bool has_new_data = false;
    bool invalidate_called = false;
    float pressure = 0.0f;
    float temperature = 0.0f;
    uint32_t last_data_ms = 0;
};

class Dps310 {
public:
    static Dps310TestState &state() {
        static Dps310TestState instance;
        return instance;
    }

    bool begin() { return true; }
    bool start() { return state().start_ok; }
    void poll() {}
    bool takeNewData(float &pressure_hpa, float &temperature_c) {
        if (!state().has_new_data) {
            return false;
        }
        pressure_hpa = state().pressure;
        temperature_c = state().temperature;
        state().has_new_data = false;
        state().pressure_valid = true;
        state().last_data_ms = millis();
        return true;
    }
    bool isOk() const { return state().ok; }
    bool isPressureValid() const { return state().pressure_valid; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
    void invalidate() {
        state().pressure_valid = false;
        state().has_new_data = false;
        state().invalidate_called = true;
    }
};
