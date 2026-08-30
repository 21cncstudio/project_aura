// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Ch422gReadyProbe.h"

#include "core/I2cBusRecovery.h"

#include <algorithm>

#ifndef UNIT_TEST
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace Ch422gReadyProbe {

namespace detail {

Result waitWithOps(i2c_port_t port,
                   gpio_num_t sda,
                   gpio_num_t scl,
                   uint32_t clock_hz,
                   uint32_t timeout_ms,
                   uint32_t poll_ms,
                   uint32_t load_settle_ms,
                   uint32_t transaction_timeout_ms,
                   const Ops &ops) {
    Result result{};
    if (!ops.start_host || !ops.write || !ops.stop_host || !ops.sample_lines ||
        !ops.recover_bus || !ops.now_ms || !ops.delay_ms || clock_hz == 0 ||
        timeout_ms == 0 || poll_ms == 0 || load_settle_ms == 0 ||
        transaction_timeout_ms == 0) {
        result.status = Status::InvalidOps;
        result.last_error = ESP_ERR_INVALID_ARG;
        return result;
    }

    const uint32_t started_ms = ops.now_ms();
    bool timed_out = false;

    const auto write_step = [&](Phase phase, uint8_t address, uint8_t value) {
        result.phase = phase;
        const uint32_t elapsed_ms = ops.now_ms() - started_ms;
        if (elapsed_ms >= timeout_ms) {
            timed_out = true;
            result.last_error = ESP_ERR_TIMEOUT;
            result.failed_address = address;
            result.failed_value = value;
            return false;
        }

        const uint32_t remaining_ms = timeout_ms - elapsed_ms;
        result.last_error = ops.write(
            port,
            address,
            value,
            std::min(transaction_timeout_ms, remaining_ms));
        if (result.last_error != ESP_OK) {
            result.failed_address = address;
            result.failed_value = value;
            return false;
        }
        if ((ops.now_ms() - started_ms) >= timeout_ms) {
            timed_out = true;
            result.last_error = ESP_ERR_TIMEOUT;
            result.failed_address = address;
            result.failed_value = value;
            return false;
        }
        result.failed_address = 0;
        result.failed_value = 0;
        return true;
    };

    do {
        result.waited_ms = ops.now_ms() - started_ms;
        if (result.waited_ms >= timeout_ms) {
            timed_out = true;
            break;
        }

        ++result.attempts;
        result.last_error = ops.start_host(port, sda, scl, clock_hz);
        if (result.last_error != ESP_OK) {
            result.status = Status::HostStartFailed;
            result.waited_ms = ops.now_ms() - started_ms;
            return result;
        }

        // Preload the eight EXIO latches first. The value is profile-specific:
        // the 7-inch board must keep USB_SEL low for native Type_C2, while the
        // legacy 4.3-inch diagnostic retains its original all-high image.
        bool sequence_ok =
            write_step(Phase::PrimeIo, kWriteIoAddress, kWriteIoSafeValue) &&
            write_step(Phase::EnableOutputs, kWriteSetAddress, kWriteSetOutputValue);

        if (sequence_ok) {
            result.phase = Phase::LoadSettle;
            result.waited_ms = ops.now_ms() - started_ms;
            if (result.waited_ms >= timeout_ms) {
                sequence_ok = false;
                timed_out = true;
                result.last_error = ESP_ERR_TIMEOUT;
            } else {
                const uint32_t remaining_ms = timeout_ms - result.waited_ms;
                if (remaining_ms <= load_settle_ms) {
                    ops.delay_ms(remaining_ms);
                    result.waited_ms = ops.now_ms() - started_ms;
                    result.last_error = ESP_ERR_TIMEOUT;
                    sequence_ok = false;
                    timed_out = true;
                } else {
                    ops.delay_ms(load_settle_ms);

                    // Validate the exact write order used by the vendor reset
                    // after the LCD/backlight load has stabilized.
                    sequence_ok =
                        write_step(Phase::ValidateSet, kWriteSetAddress, kWriteSetOutputValue) &&
                        write_step(Phase::ValidateOc, kWriteOcAddress, kWriteOcSafeValue) &&
                        write_step(Phase::ValidateIo, kWriteIoAddress, kWriteIoSafeValue);
                    if (sequence_ok) {
                        result.phase = Phase::Complete;
                    }
                }
            }
        }

        if (!sequence_ok && result.failed_address != 0) {
            const LineState lines = ops.sample_lines(sda, scl);
            result.failure_lines_valid = true;
            result.failure_sda_high = lines.sda_high;
            result.failure_scl_high = lines.scl_high;
        }

        result.waited_ms = ops.now_ms() - started_ms;
        const esp_err_t stop_error = ops.stop_host(port);
        if (stop_error != ESP_OK) {
            result.status = Status::HostStopFailed;
            result.last_error = stop_error;
            return result;
        }

        if (sequence_ok) {
            result.status = Status::Ready;
            result.last_error = ESP_OK;
            return result;
        }

        // ESP_ERR_TIMEOUT means the legacy controller saw a busy or wedged
        // bus. Reusing that host made all later cold-boot retries meaningless.
        // Tear it down first, recover the GPIO bus, then install a fresh host
        // after a passive dwell.
        if (result.last_error == ESP_ERR_TIMEOUT && result.failed_address != 0) {
            const RecoveryResult recovery = ops.recover_bus(sda, scl);
            ++result.bus_recoveries;
            result.recovery_sda_high = recovery.after.sda_high;
            result.recovery_scl_high = recovery.after.scl_high;
            result.recovery_pulses = recovery.pulses;
        }

        result.waited_ms = ops.now_ms() - started_ms;
        if (result.waited_ms >= timeout_ms) {
            timed_out = true;
            break;
        }
        const uint32_t remaining_ms = timeout_ms - result.waited_ms;
        ops.delay_ms(std::min(poll_ms, remaining_ms));
    } while (true);

    result.waited_ms = ops.now_ms() - started_ms;
    result.status = Status::Timeout;
    if (timed_out && result.last_error == ESP_OK) {
        result.last_error = ESP_ERR_TIMEOUT;
    }
    return result;
}

} // namespace detail

#ifndef UNIT_TEST
namespace {

esp_err_t startHost(i2c_port_t port,
                    gpio_num_t sda,
                    gpio_num_t scl,
                    uint32_t clock_hz) {
    i2c_config_t config{};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = sda;
    config.scl_io_num = scl;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = clock_hz;

    const esp_err_t config_error = i2c_param_config(port, &config);
    if (config_error != ESP_OK) {
        return config_error;
    }
    return i2c_driver_install(port, config.mode, 0, 0, 0);
}

esp_err_t writeDevice(i2c_port_t port,
                      uint8_t address,
                      uint8_t value,
                      uint32_t transaction_timeout_ms) {
    return i2c_master_write_to_device(
        port,
        address,
        &value,
        sizeof(value),
        pdMS_TO_TICKS(transaction_timeout_ms));
}

esp_err_t stopHost(i2c_port_t port) {
    return i2c_driver_delete(port);
}

LineState sampleLines(gpio_num_t sda, gpio_num_t scl) {
    const I2cBusRecovery::LineState lines = I2cBusRecovery::sample(sda, scl);
    return {lines.sda_high, lines.scl_high};
}

RecoveryResult recoverBus(gpio_num_t sda, gpio_num_t scl) {
    const I2cBusRecovery::Result recovery = I2cBusRecovery::recover(sda, scl);
    return {{recovery.after.sda_high, recovery.after.scl_high}, recovery.pulses};
}

uint32_t nowMs() {
    return millis();
}

void delayMs(uint32_t delay_ms) {
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

const Ops kProductionOps{
    startHost,
    writeDevice,
    stopHost,
    sampleLines,
    recoverBus,
    nowMs,
    delayMs,
};

} // namespace

Result wait(i2c_port_t port,
            gpio_num_t sda,
            gpio_num_t scl,
            uint32_t clock_hz,
            uint32_t timeout_ms,
            uint32_t poll_ms,
            uint32_t load_settle_ms,
            uint32_t transaction_timeout_ms) {
    return detail::waitWithOps(port,
                               sda,
                               scl,
                               clock_hz,
                               timeout_ms,
                               poll_ms,
                               load_settle_ms,
                               transaction_timeout_ms,
                               kProductionOps);
}
#endif

const char *statusText(Status status) {
    switch (status) {
        case Status::NotRun: return "not_run";
        case Status::Ready: return "ready";
        case Status::InvalidOps: return "invalid_ops";
        case Status::HostStartFailed: return "host_start_failed";
        case Status::Timeout: return "timeout";
        case Status::HostStopFailed: return "host_stop_failed";
        default: return "unknown";
    }
}

const char *phaseText(Phase phase) {
    switch (phase) {
        case Phase::NotRun: return "not_run";
        case Phase::PrimeIo: return "prime_io";
        case Phase::EnableOutputs: return "enable_outputs";
        case Phase::LoadSettle: return "load_settle";
        case Phase::ValidateSet: return "validate_set";
        case Phase::ValidateOc: return "validate_oc";
        case Phase::ValidateIo: return "validate_io";
        case Phase::Complete: return "complete";
        default: return "unknown";
    }
}

} // namespace Ch422gReadyProbe
