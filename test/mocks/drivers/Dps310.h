#pragma once

#ifdef USE_REAL_DPS310_HEADER
#include "../../../src/drivers/Dps310.h"
#else

#include "Arduino.h"
#include "core/CooperativeStart.h"

struct Dps310TestState {
    bool ok = true;
    bool start_ok = true;
    bool pressure_valid = false;
    bool has_new_data = false;
    bool invalidate_called = false;
    uint8_t start_call_count = 0;
    uint8_t late_start_begin_count = 0;
    uint8_t late_start_poll_count = 0;
    uint8_t late_start_polls_to_complete = 1;
    bool late_start_active = false;
    uint32_t poll_call_count = 0;
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
    bool start() {
        ++state().start_call_count;
        return state().start_ok;
    }
    void beginLateStart() {
        ++state().late_start_begin_count;
        state().late_start_poll_count = 0;
        state().late_start_active = true;
    }
    CooperativeStart::Result pollLateStart(uint32_t) {
        if (!state().late_start_active) return CooperativeStart::Result::Idle;
        if (++state().late_start_poll_count < state().late_start_polls_to_complete) {
            return CooperativeStart::Result::InProgress;
        }
        state().late_start_active = false;
        state().ok = state().start_ok;
        return state().start_ok ? CooperativeStart::Result::Success
                                : CooperativeStart::Result::Failed;
    }
    bool isLateStartActive() const { return state().late_start_active; }
    void poll() { ++state().poll_call_count; }
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

#endif
