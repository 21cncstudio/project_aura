// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/I2cBusRecovery.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

namespace I2cBusRecovery {
namespace {

constexpr uint32_t kPulseHalfPeriodUs = 5;
constexpr uint32_t kSclReleaseTimeoutUs = 1000;
constexpr uint32_t kSclReleasePollUs = 10;
constexpr uint8_t kMaxRecoveryPulses = 9;

LineState sampleWithOps(gpio_num_t sda, gpio_num_t scl, const GpioOps &ops) {
    return {
        ops.get_level(sda) == 1,
        ops.get_level(scl) == 1,
    };
}

Status classify(const LineState &before, const LineState &after) {
    if (after.idle()) {
        return before.idle() ? Status::Idle : Status::Recovered;
    }
    if (!after.sda_high && !after.scl_high) {
        return Status::BothStuckLow;
    }
    if (!after.scl_high) {
        return Status::SclStuckLow;
    }
    return Status::SdaStuckLow;
}

bool waitForSclHigh(gpio_num_t scl, const GpioOps &ops) {
    for (uint32_t waited_us = 0; waited_us < kSclReleaseTimeoutUs; waited_us += kSclReleasePollUs) {
        if (ops.get_level(scl) == 1) {
            return true;
        }
        ops.delay_us(kSclReleasePollUs);
    }
    return ops.get_level(scl) == 1;
}

void leavePassive(gpio_num_t sda, gpio_num_t scl, const GpioOps &ops) {
    ops.set_direction(sda, GPIO_MODE_INPUT);
    ops.set_direction(scl, GPIO_MODE_INPUT);
    ops.set_pull_mode(sda, GPIO_FLOATING);
    ops.set_pull_mode(scl, GPIO_FLOATING);
}

} // namespace

namespace detail {

Result recoverWithOps(gpio_num_t sda, gpio_num_t scl, const GpioOps &ops) {
    Result result{};
    if (!ops.set_level || !ops.set_direction || !ops.set_pull_mode || !ops.get_level || !ops.delay_us) {
        return result;
    }

    // Prepare the output latch before enabling the pads to avoid a low glitch.
    ops.set_level(sda, 1);
    ops.set_level(scl, 1);
    ops.set_direction(sda, GPIO_MODE_INPUT_OUTPUT_OD);
    ops.set_direction(scl, GPIO_MODE_INPUT_OUTPUT_OD);
    ops.set_pull_mode(sda, GPIO_FLOATING);
    ops.set_pull_mode(scl, GPIO_FLOATING);
    ops.delay_us(kPulseHalfPeriodUs);

    result.before = sampleWithOps(sda, scl, ops);
    if (result.before.idle()) {
        result.after = result.before;
        result.status = Status::Idle;
        leavePassive(sda, scl, ops);
        return result;
    }

    // A slave holding SCL cannot be clocked out. Leave the bus passive so it
    // can release naturally or after a real peripheral power cycle.
    if (!result.before.scl_high) {
        result.after = result.before;
        result.status = classify(result.before, result.after);
        leavePassive(sda, scl, ops);
        return result;
    }

    for (uint8_t pulse = 0; pulse < kMaxRecoveryPulses; ++pulse) {
        ops.set_level(scl, 0);
        ops.delay_us(kPulseHalfPeriodUs);
        ops.set_level(scl, 1);
        ++result.pulses;
        if (!waitForSclHigh(scl, ops)) {
            result.after = sampleWithOps(sda, scl, ops);
            result.status = classify(result.before, result.after);
            leavePassive(sda, scl, ops);
            return result;
        }
        ops.delay_us(kPulseHalfPeriodUs);
        if (ops.get_level(sda) == 1) {
            break;
        }
    }

    // Generate STOP only while SCL is physically high.
    if (ops.get_level(scl) == 1) {
        ops.set_level(sda, 0);
        ops.delay_us(kPulseHalfPeriodUs);
        ops.set_level(scl, 1);
        if (waitForSclHigh(scl, ops)) {
            ops.delay_us(kPulseHalfPeriodUs);
            ops.set_level(sda, 1);
            ops.delay_us(kPulseHalfPeriodUs);
        }
    }

    result.after = sampleWithOps(sda, scl, ops);
    result.status = classify(result.before, result.after);
    leavePassive(sda, scl, ops);
    return result;
}

} // namespace detail

#ifndef UNIT_TEST
namespace {

void delayUs(uint32_t delay_us) {
    delayMicroseconds(delay_us);
}

const GpioOps kEspGpioOps{
    gpio_set_level,
    gpio_set_direction,
    gpio_set_pull_mode,
    gpio_get_level,
    delayUs,
};

} // namespace

LineState sample(gpio_num_t sda, gpio_num_t scl) {
    return {
        gpio_get_level(sda) == 1,
        gpio_get_level(scl) == 1,
    };
}

Result recover(gpio_num_t sda, gpio_num_t scl) {
    return detail::recoverWithOps(sda, scl, kEspGpioOps);
}
#endif

const char *statusText(Status status) {
    switch (status) {
        case Status::Idle: return "idle";
        case Status::Recovered: return "recovered";
        case Status::SdaStuckLow: return "sda_stuck_low";
        case Status::SclStuckLow: return "scl_stuck_low";
        case Status::BothStuckLow: return "both_stuck_low";
        default: return "unknown";
    }
}

} // namespace I2cBusRecovery
