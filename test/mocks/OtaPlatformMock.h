// Native-only controls for the platform calls used by the real OTA handler.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace OtaPlatformMock {

struct UpdateState {
    size_t begin_calls = 0;
    size_t write_calls = 0;
    size_t end_calls = 0;
    size_t abort_calls = 0;
    size_t begin_size = 0;
    int begin_command = -1;
    bool end_even_if_remaining = false;
    bool running = false;
    bool begin_result = true;
    bool end_result = true;
    // Zero disables injection; otherwise fail that one-based write call.
    size_t fail_write_call = 0;
    size_t failed_write_bytes = 0;
    // Exercise both platform errors that reset Update and those needing abort.
    bool self_abort_on_write_failure = false;
    bool self_abort_on_end_failure = false;
    std::string error_text = "simulated update failure";
    std::vector<size_t> write_sizes;
    std::vector<uint8_t> bytes;
};

struct State {
    UpdateState update;
    std::string hardware_target = "aura-aq-v1";
    bool boot_pending_verify = false;
    bool partition_available = true;
    uint32_t partition_size = 4UL * 1024UL * 1024UL;
    bool wifi_connected = true;
    int wifi_rssi = -42;
};

State &state();
void reset();

}  // namespace OtaPlatformMock
