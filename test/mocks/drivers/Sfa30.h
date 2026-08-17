#pragma once

#include "Arduino.h"
#include "core/CooperativeStart.h"

struct Sfa30TestState {
    enum class Status : uint8_t {
        Absent = 0,
        Ok,
        Fault,
    };

    Status status = Status::Absent;
    bool data_valid = false;
    bool has_new_data = false;
    bool invalidate_called = false;
    bool probe_ok = false;
    bool probe_called = false;
    bool start_called = false;
    bool stop_ok = true;
    uint8_t stop_call_count = 0;
    uint8_t probe_call_count = 0;
    uint8_t start_call_count = 0;
    uint8_t late_start_begin_count = 0;
    uint8_t late_start_poll_count = 0;
    uint8_t late_start_polls_to_complete = 1;
    bool late_start_active = false;
    uint32_t poll_call_count = 0;
    bool warmup_active = false;
    float hcho_ppb = 0.0f;
    uint32_t last_data_ms = 0;
};

class Sfa30 {
public:
    using Status = Sfa30TestState::Status;

    static Sfa30TestState &state() {
        static Sfa30TestState instance;
        return instance;
    }

    bool begin() { return true; }
    bool probe() {
        state().probe_called = true;
        ++state().probe_call_count;
        return state().probe_ok;
    }
    void start() {
        state().start_called = true;
        ++state().start_call_count;
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
        return state().status == Status::Ok ? CooperativeStart::Result::Success
                                            : CooperativeStart::Result::Failed;
    }
    bool isLateStartActive() const { return state().late_start_active; }
    bool lateStartIdentified() const { return state().probe_ok; }
    bool stop() {
        ++state().stop_call_count;
        return state().stop_ok;
    }
    bool readData(float &) { return false; }
    void poll() { ++state().poll_call_count; }
    bool isDataValid() const { return state().data_valid; }
    bool isOk() const { return state().status == Status::Ok; }
    bool isPresent() const { return state().status != Status::Absent; }
    bool hasFault() const { return state().status == Status::Fault; }
    Status status() const { return state().status; }
    bool isWarmupActive() const { return state().warmup_active; }
    const char *label() const { return "SFA30"; }
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
