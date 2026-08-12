#pragma once

#include "Arduino.h"

struct DfrOptionalGasSensorTestState {
    enum class OptionalGasType : uint8_t {
        None = 0,
        NH3,
        SO2,
        NO2,
        H2S,
        O3,
        O2 = 6,
    };

    bool present = false;
    bool start_ok = false;
    bool start_called = false;
    bool data_valid = false;
    bool warmup = false;
    bool invalidate_called = false;
    uint32_t poll_call_count = 0;
    float ppm = 0.0f;
    uint8_t ppm_decimals = 1;
    uint32_t last_data_ms = 0;
    OptionalGasType gas_type = OptionalGasType::None;
};

class DfrOptionalGasSensor {
public:
    using OptionalGasType = DfrOptionalGasSensorTestState::OptionalGasType;

    bool begin() { return true; }
    bool start() {
        state().start_called = true;
        state().present = state().start_ok;
        if (!state().present) {
            state().data_valid = false;
            state().ppm = 0.0f;
            state().ppm_decimals = 1;
            state().warmup = false;
            state().gas_type = OptionalGasType::None;
        }
        return state().start_ok;
    }
    void poll() { ++state().poll_call_count; }
    bool isPresent() const { return state().present; }
    bool isDataValid() const { return state().data_valid; }
    bool isWarmupActive() const { return state().warmup; }
    float ppm() const { return state().ppm; }
    uint8_t ppmDecimals() const { return state().ppm_decimals; }
    OptionalGasType optionalGasType() const { return state().gas_type; }
    const char *optionalGasLabel() const { return optionalGasLabel(state().gas_type); }
    const char *label() const { return "DFR Optional Gas"; }
    uint8_t address() const { return 0x75; }
    uint32_t lastDataMs() const { return state().last_data_ms; }
    void invalidate() {
        state().invalidate_called = true;
        state().data_valid = false;
    }

    static DfrOptionalGasSensorTestState &state() {
        static DfrOptionalGasSensorTestState instance;
        return instance;
    }

    static const char *optionalGasLabel(OptionalGasType type) {
        switch (type) {
            case OptionalGasType::NH3:
                return "NH3";
            case OptionalGasType::SO2:
                return "SO2";
            case OptionalGasType::NO2:
                return "NO2";
            case OptionalGasType::H2S:
                return "H2S";
            case OptionalGasType::O3:
                return "O3";
            case OptionalGasType::O2:
                return "O2";
            case OptionalGasType::None:
            default:
                return "None";
        }
    }

    static const char *unitForType(OptionalGasType type) {
        return type == OptionalGasType::O2 ? "%Vol" : "ppm";
    }

    static float minPpmForType(OptionalGasType type) {
        switch (type) {
            case OptionalGasType::NH3:
            case OptionalGasType::SO2:
            case OptionalGasType::NO2:
            case OptionalGasType::H2S:
            case OptionalGasType::O3:
            case OptionalGasType::O2:
            case OptionalGasType::None:
            default:
                return 0.0f;
        }
    }

    static float maxPpmForType(OptionalGasType type) {
        switch (type) {
            case OptionalGasType::NH3:
            case OptionalGasType::H2S:
                return 100.0f;
            case OptionalGasType::SO2:
            case OptionalGasType::NO2:
                return 20.0f;
            case OptionalGasType::O3:
                return 10.0f;
            case OptionalGasType::O2:
                return 25.0f;
            case OptionalGasType::None:
            default:
                return 0.0f;
        }
    }
};
