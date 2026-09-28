#pragma once
#include "Arduino.h"
struct Sen0321TestState {
    bool present = false, valid = false, warmup = false;
    float ppm = 0;
    uint32_t last_data_ms = 0;
};
class Sen0321 {
public:
    static constexpr uint8_t Address = 0x73;
    static Sen0321TestState &state() { static Sen0321TestState s; return s; }
    void begin() {}
    bool start() { return state().present; }
    void poll() {}
    bool isPresent() const { return state().present; }
    bool isDataValid() const { return state().valid && !state().warmup && uint32_t(millis() - state().last_data_ms) <= 18000; }
    bool isWarmupActive() const { return state().warmup; }
    float ppm() const { return state().ppm; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
    void invalidate() { state().valid = false; }
};
