#pragma once
#include "drivers/Sen66.h"

// Manager tests keep the established SEN66 fixture. Selection/protocol are
// exercised separately against the real facade and real SEN69C driver.
class Sen6x : public Sen66 {
public:
    enum class Model : uint8_t { None, Sen66, Sen69c, Unsupported };
    static Model &selectedModel() { static Model m = Model::Sen66; return m; }
    static bool &co2Warmup() { static bool value = false; return value; }
    static bool &controlStartFailure() { static bool value = false; return value; }
    bool setAscEnabled(bool enabled) {
        const bool success = Sen66::setAscEnabled(enabled);
        if (controlStartFailure()) { state().ok = false; return false; }
        return success;
    }
    bool calibrateFRC(uint16_t reference, bool has_pressure, float pressure, uint16_t &correction) {
        const bool success = Sen66::calibrateFRC(reference, has_pressure, pressure, correction);
        if (controlStartFailure()) { state().ok = false; return false; }
        return success;
    }
    void poll(SensorData &data, bool &changed) {
        Sen66::poll(data, changed);
        if (state().update_pm05_on_poll) state().pm05_last_data_ms = millis();
    }
    uint32_t pm05LastDataMs() const { return isSen69c() ? state().pm05_last_data_ms : lastDataMs(); }
    bool isSen69c() const { return selectedModel() == Model::Sen69c; }
    Model model() const { return selectedModel(); }
    const char *label() const { return isSen69c() ? "SEN69C" : "SEN66"; }
    const char *serial() const { return "test-serial"; }
    bool isCo2WarmupActive() const { return isSen69c() && co2Warmup(); }
    bool isHchoWarmupActive() const { return isSen69c() && state().poll_data.hcho_warmup; }
    bool hasHchoFault() const { return isSen69c() && !state().ok; }
    bool takeHcho(float &v) {
        if (!isSen69c() || !state().poll_data.hcho_valid) return false;
        v = state().poll_data.hcho; return true;
    }
    void invalidateHcho() {}
    uint32_t hchoLastDataMs() const { return state().last_data_ms; }
};
