// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <driver/gpio.h>
#include <stdint.h>

namespace I2cBusRecovery {

struct LineState {
    bool sda_high = false;
    bool scl_high = false;

    bool idle() const { return sda_high && scl_high; }
};

enum class Status : uint8_t {
    Idle = 0,
    Recovered,
    SdaStuckLow,
    SclStuckLow,
    BothStuckLow,
};

struct Result {
    LineState before{};
    LineState after{};
    Status status = Status::BothStuckLow;
    uint8_t pulses = 0;

    bool busReady() const {
        return status == Status::Idle || status == Status::Recovered;
    }
};

struct GpioOps {
    esp_err_t (*set_level)(gpio_num_t pin, uint32_t level) = nullptr;
    esp_err_t (*set_direction)(gpio_num_t pin, gpio_mode_t mode) = nullptr;
    esp_err_t (*set_pull_mode)(gpio_num_t pin, gpio_pull_mode_t mode) = nullptr;
    int (*get_level)(gpio_num_t pin) = nullptr;
    void (*delay_us)(uint32_t delay_us) = nullptr;
};

LineState sample(gpio_num_t sda, gpio_num_t scl);
Result recover(gpio_num_t sda, gpio_num_t scl);
const char *statusText(Status status);

namespace detail {
Result recoverWithOps(gpio_num_t sda, gpio_num_t scl, const GpioOps &ops);
}

} // namespace I2cBusRecovery
