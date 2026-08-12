#pragma once

#ifdef USE_REAL_BMP3XX_HEADER
#include "../../../src/drivers/Bmp3xx.h"
#else

#include "Arduino.h"
#include "core/CooperativeStart.h"

struct Bmp3xxTestState {
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

class Bmp3xx {
public:
    enum class Variant : uint8_t {
        Unknown = 0,
        BMP388,
        BMP390
    };

    static Bmp3xxTestState &state() {
        static Bmp3xxTestState instance;
        return instance;
    }

    static Variant &variant_state() {
        static Variant instance = Variant::BMP390;
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
    Variant variant() const { return variant_state(); }
    const char *variantLabel() const {
        switch (variant_state()) {
            case Variant::BMP388:
                return "BMP388";
            case Variant::BMP390:
                return "BMP390";
            default:
                return "BMP3xx";
        }
    }
    void invalidate() {
        state().pressure_valid = false;
        state().has_new_data = false;
        state().invalidate_called = true;
    }
};

#endif
