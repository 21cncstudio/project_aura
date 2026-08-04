// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <stdint.h>

namespace Ch422gReadyProbe {

// CH422G encodes each command as a separate 7-bit I2C address.
constexpr uint8_t kWriteOcAddress = 0x23;
constexpr uint8_t kWriteSetAddress = 0x24;
constexpr uint8_t kWriteIoAddress = 0x38;
constexpr uint8_t kWriteOcSafeValue = 0x0F;
constexpr uint8_t kWriteIoSafeValue = 0xFF;
constexpr uint8_t kWriteSetOutputValue = 0x01;

enum class Status : uint8_t {
    NotRun = 0,
    Ready,
    InvalidOps,
    HostStartFailed,
    Timeout,
    HostStopFailed,
};

enum class Phase : uint8_t {
    NotRun = 0,
    PrimeIo,
    EnableOutputs,
    LoadSettle,
    ValidateSet,
    ValidateOc,
    ValidateIo,
    Complete,
};

struct LineState {
    bool sda_high = true;
    bool scl_high = true;
};

struct RecoveryResult {
    LineState after{};
    uint8_t pulses = 0;
};

struct Result {
    Status status = Status::NotRun;
    Phase phase = Phase::NotRun;
    esp_err_t last_error = ESP_OK;
    uint32_t waited_ms = 0;
    uint16_t attempts = 0;
    uint8_t failed_address = 0;
    uint8_t failed_value = 0;
    uint16_t bus_recoveries = 0;
    bool failure_lines_valid = false;
    bool failure_sda_high = true;
    bool failure_scl_high = true;
    bool recovery_sda_high = true;
    bool recovery_scl_high = true;
    uint8_t recovery_pulses = 0;

    bool ready() const { return status == Status::Ready; }
};

struct Ops {
    esp_err_t (*start_host)(i2c_port_t port,
                            gpio_num_t sda,
                            gpio_num_t scl,
                            uint32_t clock_hz) = nullptr;
    esp_err_t (*write)(i2c_port_t port,
                       uint8_t address,
                       uint8_t value,
                       uint32_t transaction_timeout_ms) = nullptr;
    esp_err_t (*stop_host)(i2c_port_t port) = nullptr;
    LineState (*sample_lines)(gpio_num_t sda, gpio_num_t scl) = nullptr;
    RecoveryResult (*recover_bus)(gpio_num_t sda, gpio_num_t scl) = nullptr;
    uint32_t (*now_ms)() = nullptr;
    void (*delay_ms)(uint32_t delay_ms) = nullptr;
};

Result wait(i2c_port_t port,
            gpio_num_t sda,
            gpio_num_t scl,
            uint32_t clock_hz,
            uint32_t timeout_ms,
            uint32_t poll_ms,
            uint32_t load_settle_ms,
            uint32_t transaction_timeout_ms);
const char *statusText(Status status);
const char *phaseText(Phase phase);

namespace detail {
Result waitWithOps(i2c_port_t port,
                   gpio_num_t sda,
                   gpio_num_t scl,
                   uint32_t clock_hz,
                   uint32_t timeout_ms,
                   uint32_t poll_ms,
                   uint32_t load_settle_ms,
                   uint32_t transaction_timeout_ms,
                   const Ops &ops);
}

} // namespace Ch422gReadyProbe
