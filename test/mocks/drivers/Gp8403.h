#pragma once

#include <cstdint>

struct Gp8403TestState {
    bool begin_ok = true;
    bool probe_ok = true;
    bool range_ok = true;
    bool write_ok = true;
    uint32_t begin_calls = 0;
    uint32_t probe_calls = 0;
    uint32_t range_calls = 0;
    uint32_t write_calls = 0;
};

class Gp8403 {
public:
    static Gp8403TestState &state() {
        static Gp8403TestState value;
        return value;
    }

    bool begin(uint8_t) {
        ++state().begin_calls;
        return state().begin_ok;
    }
    bool probe() {
        ++state().probe_calls;
        return state().probe_ok;
    }
    bool setOutputRange10V() {
        ++state().range_calls;
        return state().range_ok;
    }
    bool writeChannelRaw12(uint8_t, uint16_t) {
        ++state().write_calls;
        return state().write_ok;
    }
    bool writeChannelMillivolts(uint8_t, uint16_t) {
        ++state().write_calls;
        return state().write_ok;
    }
};
