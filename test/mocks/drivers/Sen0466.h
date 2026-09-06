#pragma once

#include "Arduino.h"

struct Sen0466TestState {
    bool present = false;
    bool start_ok = false;
    bool begin_called = false;
    bool start_called = false;
    bool data_valid = false;
    bool warmup = false;
    bool invalidate_called = false;
    uint32_t poll_call_count = 0;
    float co_ppm = 0.0f;
    uint32_t last_data_ms = 0;
};

class Sen0466 {
public:
    static Sen0466TestState &state() {
        static Sen0466TestState instance;
        return instance;
    }

    bool begin() {
        state().begin_called = true;
        return true;
    }
    bool start() {
        state().start_called = true;
        state().present = state().start_ok;
        if (!state().present) {
            state().data_valid = false;
            state().co_ppm = 0.0f;
            state().warmup = false;
        }
        return state().start_ok;
    }
    void poll() { ++state().poll_call_count; }
    bool isPresent() const { return state().present; }
    bool isDataValid() const { return state().data_valid; }
    bool isWarmupActive() const { return state().warmup; }
    float coPpm() const { return state().co_ppm; }
    const char *label() const { return "SEN0466 CO"; }
    uint8_t address() const { return 0x74; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
    void invalidate() {
        state().invalidate_called = true;
        state().data_valid = false;
    }
};
