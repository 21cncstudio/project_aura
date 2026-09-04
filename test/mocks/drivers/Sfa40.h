#pragma once

#include "Arduino.h"
#include "core/CooperativeStart.h"

struct Sfa40TestState {
    enum class Status : uint8_t {
        Absent = 0,
        Ok,
        Fault,
    };

    Status status = Status::Ok;
    bool data_valid = false;
    bool has_new_data = false;
    bool invalidate_called = false;
    bool warmup_active = false;
    bool fallback_to_sfa30 = false;
    bool start_called = false;
    bool stop_ok = true;
    uint8_t stop_call_count = 0;
    uint8_t start_call_count = 0;
    uint8_t late_start_begin_count = 0;
    uint8_t late_start_poll_count = 0;
    uint8_t late_start_polls_to_complete = 1;
    bool late_start_active = false;
    uint32_t poll_call_count = 0;
    float hcho_ppb = 0.0f;
    uint32_t last_data_ms = 0;
};

class Sfa40 {
public:
    enum class Protocol : uint8_t {
        Unknown = 0,
        Production,
        B4,
    };

    enum class SelfTestStatus : uint8_t {
        Idle = 0,
        Running,
        Passed,
        Failed,
        ReadError,
    };

    using Status = Sfa40TestState::Status;

    struct Diagnostics {
        bool measuring = false;
        bool measurement_state_unknown = false;
        bool data_valid = false;
        bool has_new_data = false;
        bool warmup_active = false;
        bool selftest_active = false;
        Status status = Status::Absent;
        const char *last_error = "none";
        Protocol protocol = Protocol::Unknown;
        bool serial_valid = false;
        uint8_t serial_word_count = 0;
        uint16_t serial_words[5] = {};
        uint32_t start_ms = 0;
        uint32_t first_ready_ms = 0;
        uint32_t first_within_spec_ms = 0;
        uint32_t last_measurement_ms = 0;
        uint32_t last_data_ms = 0;
        uint32_t measurement_reads = 0;
        uint32_t measurement_frames_ok = 0;
        uint32_t measurement_data_ok = 0;
        uint32_t measurement_read_failures = 0;
        uint32_t read_command_errors = 0;
        uint32_t read_bytes_errors = 0;
        uint32_t read_crc_errors = 0;
        uint32_t status_not_ready_count = 0;
        uint32_t status_not_within_spec_count = 0;
        uint32_t status_invalid_count = 0;
        bool last_status_valid = false;
        uint16_t raw_hcho = 0;
        uint16_t raw_humidity = 0;
        uint16_t raw_temperature = 0;
        uint8_t status_byte = 0;
        uint8_t status_reserved = 0;
        float hcho_ppb = 0.0f;
        float humidity_percent = 0.0f;
        float temperature_c = 0.0f;
    };

    static Sfa40TestState &state() {
        static Sfa40TestState instance;
        return instance;
    }

    bool begin() { return true; }
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
    bool stop() {
        ++state().stop_call_count;
        return state().stop_ok;
    }
    bool readData(float &) { return false; }
    bool startSelfTest() { return false; }
    SelfTestStatus readSelfTestStatus(uint16_t &raw_result) {
        raw_result = 0;
        return SelfTestStatus::Idle;
    }
    void poll() { ++state().poll_call_count; }
    bool isDataValid() const { return state().data_valid; }
    bool isOk() const { return state().status == Status::Ok; }
    bool isPresent() const { return state().status != Status::Absent; }
    bool hasFault() const { return state().status == Status::Fault; }
    bool isWarmupActive() const { return state().warmup_active; }
    Status status() const { return state().status; }
    bool shouldFallbackToSfa30() const { return state().fallback_to_sfa30; }
    static const char *protocolLabel(Protocol protocol) {
        switch (protocol) {
            case Protocol::Production:
                return "production";
            case Protocol::B4:
                return "B4";
            default:
                return "unknown";
        }
    }
    const char *label() const { return "SFA40"; }
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
    Diagnostics diagnostics() const {
        Diagnostics out;
        out.data_valid = state().data_valid;
        out.has_new_data = state().has_new_data;
        out.warmup_active = state().warmup_active;
        out.status = state().status;
        out.last_data_ms = state().last_data_ms;
        out.hcho_ppb = state().hcho_ppb;
        return out;
    }
};
