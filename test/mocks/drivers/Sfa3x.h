#pragma once

#include "Arduino.h"

struct Sfa3xTestState {
    bool ok = true;
    bool data_valid = false;
    bool has_new_data = false;
    bool invalidate_called = false;
    float hcho_ppb = 0.0f;
    uint32_t last_data_ms = 0;
};

class Sfa3x {
public:
    static Sfa3xTestState &state() {
        static Sfa3xTestState instance;
        return instance;
    }

    bool begin() { return true; }
    void start() {}
    void stop() {}
    bool readData(float &) { return false; }
    void poll() {}
    bool isDataValid() const { return state().data_valid; }
    bool isOk() const { return state().ok; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
    bool takeNewData(float &hcho_ppb) {
        if (!state().has_new_data) {
            return false;
        }
        hcho_ppb = state().hcho_ppb;
        state().has_new_data = false;
        state().data_valid = true;
        state().last_data_ms = millis();
        return true;
    }
    void invalidate() {
        state().data_valid = false;
        state().invalidate_called = true;
    }
};
