// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/DfrMultiGasSensor.h"

#include <driver/i2c.h>
#include <math.h>
#include <string.h>

#include "config/AppConfig.h"
#include "core/Logger.h"
#include "drivers/DfrGasProbeLogPolicy.h"
#ifndef UNIT_TEST
#include "core/I2cBusRecovery.h"
#include <esp_err.h>
#endif

namespace {

constexpr uint8_t kFrameLen = 9;

} // namespace

bool DfrMultiGasSensor::begin() {
    present_ = false;
    data_valid_ = false;
    warned_type_mismatch_ = false;
    ppm_ = 0.0f;
    ppm_decimals_ = 1;
    gas_type_ = GasType::None;
    raw_gas_type_ = 0;
    fail_count_ = 0;
    warmup_started_ = false;
    warmup_started_ms_ = 0;
    last_poll_ms_ = 0;
    last_data_ms_ = 0;
    last_retry_ms_ = 0;
    fail_cooldown_active_ = false;
    fail_cooldown_started_ms_ = 0;
    cooldown_probe_fail_count_ = 0;
    start_attempts_ = 0;
    absent_retry_count_ = 0;
    absent_retry_active_ = false;
    absent_retry_exhausted_ = false;
    start_retry_exhausted_logged_ = false;
    last_read_failure_reason_ = FailureReason::None;
    return true;
}

bool DfrMultiGasSensor::start() {
    last_retry_ms_ = millis();
    if (!pingAddress()) {
        if (start_attempts_ < UINT8_MAX) {
            ++start_attempts_;
        }
        if (!start_retry_exhausted_logged_ &&
            start_attempts_ >= Config::DFR_GAS_MAX_START_ATTEMPTS) {
            if (absent_retry_active_) {
                absent_retry_active_ = false;
                if (absent_retry_count_ < UINT8_MAX) {
                    ++absent_retry_count_;
                }
            }
            if (absent_retry_count_ >= Config::DFR_GAS_MAX_ABSENT_RETRIES) {
                LOGI(config_.log_tag, "not installed after %u background retries, stop probing until reboot",
                     static_cast<unsigned>(Config::DFR_GAS_MAX_ABSENT_RETRIES));
                absent_retry_exhausted_ = true;
            } else if (absent_retry_count_ > 0) {
                LOGI(config_.log_tag,
                     "background retry %u/%u failed, next retry in %lu ms",
                     static_cast<unsigned>(absent_retry_count_),
                     static_cast<unsigned>(Config::DFR_GAS_MAX_ABSENT_RETRIES),
                     static_cast<unsigned long>(Config::DFR_GAS_ABSENT_RETRY_MS));
            } else {
                LOGI(config_.log_tag, "not installed after %u attempts, background retry in %lu ms",
                     static_cast<unsigned>(Config::DFR_GAS_MAX_START_ATTEMPTS),
                     static_cast<unsigned long>(Config::DFR_GAS_ABSENT_RETRY_MS));
            }
            start_retry_exhausted_logged_ = true;
        }
        present_ = false;
        data_valid_ = false;
        ppm_ = 0.0f;
        ppm_decimals_ = 1;
        gas_type_ = GasType::None;
        raw_gas_type_ = 0;
        fail_count_ = 0;
        warmup_started_ = false;
        warned_type_mismatch_ = false;
        fail_cooldown_active_ = false;
        fail_cooldown_started_ms_ = 0;
        cooldown_probe_fail_count_ = 0;
        last_read_failure_reason_ = FailureReason::None;
        return false;
    }

    start_attempts_ = 0;
    absent_retry_count_ = 0;
    absent_retry_active_ = false;
    absent_retry_exhausted_ = false;
    start_retry_exhausted_logged_ = false;
    const bool was_present = present_;
    present_ = true;
    if (!was_present) {
        warmup_started_ = true;
        warmup_started_ms_ = millis();
        data_valid_ = false;
        ppm_ = 0.0f;
        ppm_decimals_ = 1;
        gas_type_ = GasType::None;
        raw_gas_type_ = 0;
        fail_count_ = 0;
        warned_type_mismatch_ = false;
        fail_cooldown_active_ = false;
        fail_cooldown_started_ms_ = 0;
        cooldown_probe_fail_count_ = 0;
        last_read_failure_reason_ = FailureReason::None;
    }

    // Probe only through the normal read path. Communication-mode mutation is
    // intentionally excluded from boot and runtime recovery because DFRobot
    // requires a real sensor power cycle after changing modes. Do not publish
    // this probe: warmup and regular polling still own data validity.
    float startup_ppm = 0.0f;
    uint8_t startup_gas_type = 0;
    uint8_t startup_decimals = 1;
    FailureReason startup_read_failure = FailureReason::None;
    if (readGasConcentration(startup_ppm,
                             startup_gas_type,
                             startup_decimals,
                             startup_read_failure)) {
        last_read_failure_reason_ = FailureReason::None;
        LOGI(config_.log_tag,
             "read-only startup probe confirmed; mode write skipped");
        return true;
    }

    last_read_failure_reason_ = startup_read_failure;
    LOGW(config_.log_tag,
         "read-only startup probe failed (%s); communication mode unchanged",
         failureReasonLabel(startup_read_failure));
    return true;
}

void DfrMultiGasSensor::poll() {
    const uint32_t now = millis();

    if (!present_) {
        if (absent_retry_exhausted_) {
            return;
        }
        if (start_attempts_ >= Config::DFR_GAS_MAX_START_ATTEMPTS) {
            if (now - last_retry_ms_ < Config::DFR_GAS_ABSENT_RETRY_MS) {
                return;
            }
            start_attempts_ = 0;
            absent_retry_active_ = true;
            start_retry_exhausted_logged_ = false;
        }
        if (now - last_retry_ms_ >= Config::DFR_GAS_RETRY_MS) {
            start();
        }
        return;
    }

    if (fail_cooldown_active_) {
        if (now - fail_cooldown_started_ms_ < Config::DFR_GAS_FAIL_COOLDOWN_MS) {
            return;
        }

        fail_cooldown_active_ = false;
        fail_cooldown_started_ms_ = 0;
        if (!pingAddress()) {
            if (cooldown_probe_fail_count_ < UINT8_MAX) {
                ++cooldown_probe_fail_count_;
            }
            if (cooldown_probe_fail_count_ >=
                Config::DFR_GAS_MAX_COOLDOWN_RECOVERY_FAILS) {
                if (isInStartupFaultGrace(now)) {
                    LOGW(config_.log_tag,
                         "cooldown address probe failed %u times during startup grace, keeping sensor present",
                         static_cast<unsigned>(cooldown_probe_fail_count_));
                    cooldown_probe_fail_count_ = 0;
                    fail_cooldown_active_ = true;
                    fail_cooldown_started_ms_ = now;
                    last_poll_ms_ = now;
                    return;
                } else {
                    LOGW(config_.log_tag,
                         "cooldown address probe failed %u times, marking sensor not present",
                         static_cast<unsigned>(cooldown_probe_fail_count_));
                    present_ = false;
                    data_valid_ = false;
                    ppm_ = 0.0f;
                    gas_type_ = GasType::None;
                    raw_gas_type_ = 0;
                    fail_count_ = 0;
                    warmup_started_ = false;
                    warmup_started_ms_ = 0;
                    warned_type_mismatch_ = false;
                    fail_cooldown_active_ = false;
                    fail_cooldown_started_ms_ = 0;
                    cooldown_probe_fail_count_ = 0;
                    last_retry_ms_ = now;
                }
                return;
            }
            fail_cooldown_active_ = true;
            fail_cooldown_started_ms_ = now;
            LOGW(config_.log_tag,
                 "cooldown elapsed, address probe failed (%u/%u); no mode write attempted",
                 static_cast<unsigned>(cooldown_probe_fail_count_),
                 static_cast<unsigned>(Config::DFR_GAS_MAX_COOLDOWN_RECOVERY_FAILS));
            return;
        }

        cooldown_probe_fail_count_ = 0;
        fail_count_ = 0;
        warned_type_mismatch_ = false;
        last_poll_ms_ = now;
        LOGI(config_.log_tag,
             "cooldown elapsed, address acknowledged; resuming read-only polling");
        return;
    }

    if (data_valid_ && last_data_ms_ != 0 &&
        (now - last_data_ms_ > Config::DFR_GAS_STALE_MS)) {
        data_valid_ = false;
    }

    if (now - last_poll_ms_ < Config::DFR_GAS_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    float ppm = 0.0f;
    uint8_t gas_type = 0;
    uint8_t decimals = 1;
    FailureReason read_failure = FailureReason::None;
    if (!readGasConcentration(ppm, gas_type, decimals, read_failure)) {
        last_read_failure_reason_ = read_failure;
        if (fail_count_ < UINT8_MAX) {
            ++fail_count_;
        }
        if (fail_count_ >= Config::DFR_GAS_MAX_FAILS) {
            data_valid_ = false;
            fail_cooldown_active_ = true;
            fail_cooldown_started_ms_ = now;
            cooldown_probe_fail_count_ = 0;
            LOGW(config_.log_tag, "read failed %u times (%s), entering cooldown %lu ms",
                 static_cast<unsigned>(fail_count_),
                 failureReasonLabel(read_failure),
                 static_cast<unsigned long>(Config::DFR_GAS_FAIL_COOLDOWN_MS));
        }
        return;
    }

    cooldown_probe_fail_count_ = 0;
    fail_count_ = 0;
    last_read_failure_reason_ = FailureReason::None;
    last_data_ms_ = now;
    raw_gas_type_ = gas_type;
    gas_type_ = mapGasType(gas_type);

    if (!isGasTypeAccepted(gas_type)) {
        if (!warned_type_mismatch_) {
            if (config_.allowed_gas_type_count > 0) {
                Logger::log(Logger::Warn, config_.log_tag,
                            "unsupported gas type 0x%02X for this slot",
                            gas_type);
            } else {
                Logger::log(Logger::Warn, config_.log_tag,
                            "unexpected gas type 0x%02X (expected 0x%02X)",
                            gas_type, config_.expected_gas_type);
            }
            warned_type_mismatch_ = true;
        }
        data_valid_ = false;
        return;
    }
    warned_type_mismatch_ = false;

    if (!isfinite(ppm) || ppm < config_.min_ppm) {
        data_valid_ = false;
        return;
    }
    if (config_.max_ppm > config_.min_ppm && ppm > config_.max_ppm) {
        ppm = config_.max_ppm;
    }

    ppm_ = ppm;
    ppm_decimals_ = decimals;
    data_valid_ = !isWarmupActive();
}

bool DfrMultiGasSensor::isWarmupActive() const {
    if (!present_ || !warmup_started_) {
        return false;
    }
    return (millis() - warmup_started_ms_) < Config::DFR_GAS_WARMUP_MS;
}

void DfrMultiGasSensor::invalidate() {
    data_valid_ = false;
}

void DfrMultiGasSensor::clampPpm(float min_ppm, float max_ppm) {
    if (!data_valid_) {
        return;
    }
    if (!isfinite(ppm_) || ppm_ < min_ppm) {
        data_valid_ = false;
        ppm_ = 0.0f;
        return;
    }
    if (max_ppm > min_ppm && ppm_ > max_ppm) {
        ppm_ = max_ppm;
    }
}

const char *DfrMultiGasSensor::gasTypeLabel(GasType type) {
    switch (type) {
        case GasType::NH3:
            return "NH3";
        case GasType::SO2:
            return "SO2";
        case GasType::NO2:
            return "NO2";
        case GasType::CO:
            return "CO";
        case GasType::O2:
            return "O2";
        case GasType::H2S:
            return "H2S";
        case GasType::O3:
            return "O3";
        case GasType::Unknown:
            return "Unknown";
        case GasType::None:
        default:
            return "None";
    }
}

DfrMultiGasSensor::GasType DfrMultiGasSensor::mapGasType(uint8_t gas_type_raw) {
    switch (gas_type_raw) {
        case Config::DFR_GAS_TYPE_NH3:
            return GasType::NH3;
        case Config::DFR_GAS_TYPE_O3:
            return GasType::O3;
        case Config::DFR_GAS_TYPE_SO2:
            return GasType::SO2;
        case Config::DFR_GAS_TYPE_NO2:
            return GasType::NO2;
        case Config::DFR_GAS_TYPE_CO:
            return GasType::CO;
        case Config::DFR_GAS_TYPE_O2:
            return GasType::O2;
        case Config::DFR_GAS_TYPE_H2S:
            return GasType::H2S;
        case 0:
            return GasType::None;
        default:
            return GasType::Unknown;
    }
}

bool DfrMultiGasSensor::isGasTypeAccepted(uint8_t gas_type_raw) const {
    if (config_.allowed_gas_type_count > 0 && config_.allowed_gas_types) {
        for (size_t i = 0; i < config_.allowed_gas_type_count; ++i) {
            if (config_.allowed_gas_types[i] == gas_type_raw) {
                return true;
            }
        }
        return false;
    }

    if (config_.expected_gas_type != 0) {
        return gas_type_raw == config_.expected_gas_type;
    }

    return true;
}

bool DfrMultiGasSensor::pingAddress() {
#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_before =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
#endif
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
#ifndef UNIT_TEST
        const I2cBusRecovery::LineState lines_after =
            I2cBusRecovery::sample(
                static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
                static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
        LOGW(config_.log_tag,
             "addr=0x%02X stage=address-probe err=%d(%s) lines before=%u/%u after=%u/%u",
             static_cast<unsigned>(config_.address),
             static_cast<int>(ESP_ERR_NO_MEM),
             esp_err_to_name(ESP_ERR_NO_MEM),
             lines_before.sda_high ? 1U : 0U,
             lines_before.scl_high ? 1U : 0U,
             lines_after.sda_high ? 1U : 0U,
             lines_after.scl_high ? 1U : 0U);
#endif
        return false;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (config_.address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(
        Config::SENSOR_I2C_PORT,
        cmd,
        pdMS_TO_TICKS(Config::DFR_GAS_I2C_TIMEOUT_MS)
    );
    i2c_cmd_link_delete(cmd);
#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_after =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
    if (err != ESP_OK) {
        const DfrGasProbeLogPolicy::ProbeFailure probe_failure{
            config_.optional_installation,
            present_,
            err == ESP_FAIL,
            lines_before.idle(),
            lines_after.idle(),
        };
        const bool flexible_optional_slot =
            config_.allowed_gas_types != nullptr &&
            config_.allowed_gas_type_count > 0;
        if (flexible_optional_slot &&
            DfrGasProbeLogPolicy::isExpectedOptionalAbsence(probe_failure)) {
            LOGI(config_.log_tag,
                 "optional slot addr=0x%02X not acknowledged; treating as not installed",
                 static_cast<unsigned>(config_.address));
        } else if (DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(
                       probe_failure)) {
            Logger::logWithoutAlert(
                Logger::Warn,
                config_.log_tag,
                "addr=0x%02X stage=address-probe err=%d(%s) lines before=%u/%u after=%u/%u",
                static_cast<unsigned>(config_.address),
                static_cast<int>(err),
                esp_err_to_name(err),
                lines_before.sda_high ? 1U : 0U,
                lines_before.scl_high ? 1U : 0U,
                lines_after.sda_high ? 1U : 0U,
                lines_after.scl_high ? 1U : 0U);
        } else {
            LOGW(config_.log_tag,
                 "addr=0x%02X stage=address-probe err=%d(%s) lines before=%u/%u after=%u/%u",
                 static_cast<unsigned>(config_.address),
                 static_cast<int>(err),
                 esp_err_to_name(err),
                 lines_before.sda_high ? 1U : 0U,
                 lines_before.scl_high ? 1U : 0U,
                 lines_after.sda_high ? 1U : 0U,
                 lines_after.scl_high ? 1U : 0U);
        }
    }
#endif
    return err == ESP_OK;
}

bool DfrMultiGasSensor::readGasConcentration(float &ppm,
                                             uint8_t &gas_type,
                                             uint8_t &decimal_places,
                                             FailureReason &failure_reason) {
    failure_reason = FailureReason::None;
    uint8_t tx[kFrameLen] = {0};
    buildFrame(Config::DFR_GAS_CMD_READ_GAS, 0, 0, 0, 0, 0, tx);

    uint8_t rx[kFrameLen] = {0};
    if (!transact(tx, rx, &failure_reason)) {
        return false;
    }
    if (rx[0] != 0xFF || rx[1] != Config::DFR_GAS_CMD_READ_GAS) {
        failure_reason = FailureReason::BadHeader;
        return false;
    }
    // Some DFR firmware revisions sum bytes 1..6 instead of the documented 1..7.
    if (rx[8] != checksum7(rx) && rx[8] != checksum6(rx)) {
        failure_reason = FailureReason::BadChecksum;
        return false;
    }

    const uint16_t raw = static_cast<uint16_t>(rx[2] << 8) | rx[3];
    const uint8_t decimals = rx[5];
    float scale = 1.0f;
    if (decimals == 1) {
        scale = 0.1f;
    } else if (decimals == 2) {
        scale = 0.01f;
    } else if (decimals != 0) {
        failure_reason = FailureReason::BadDecimals;
        return false;
    }

    ppm = static_cast<float>(raw) * scale;
    gas_type = rx[4];
    decimal_places = decimals;
    return true;
}

bool DfrMultiGasSensor::transact(const uint8_t *tx_frame,
                                 uint8_t *rx_frame,
                                 FailureReason *failure_reason) {
    if (failure_reason) {
        *failure_reason = FailureReason::None;
    }
    uint8_t tx[kFrameLen + 1] = {0};
    tx[0] = 0x00;
    memcpy(&tx[1], tx_frame, kFrameLen);

#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_before_write =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
#endif
    esp_err_t err = i2c_master_write_to_device(
        Config::SENSOR_I2C_PORT,
        config_.address,
        tx,
        sizeof(tx),
        pdMS_TO_TICKS(Config::DFR_GAS_I2C_TIMEOUT_MS)
    );
#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_after_write =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
#endif
    if (err != ESP_OK) {
#ifndef UNIT_TEST
        LOGW(config_.log_tag,
             "cmd=0x%02X stage=write err=%d(%s) lines before=%u/%u after=%u/%u",
             static_cast<unsigned>(tx_frame[2]),
             static_cast<int>(err),
             esp_err_to_name(err),
             lines_before_write.sda_high ? 1U : 0U,
             lines_before_write.scl_high ? 1U : 0U,
             lines_after_write.sda_high ? 1U : 0U,
             lines_after_write.scl_high ? 1U : 0U);
#endif
        if (failure_reason) {
            *failure_reason = FailureReason::I2cWrite;
        }
        return false;
    }

    delay(Config::DFR_GAS_CMD_DELAY_MS);

#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_before_read =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
#endif
    uint8_t reg = 0x00;
    err = i2c_master_write_read_device(
        Config::SENSOR_I2C_PORT,
        config_.address,
        &reg,
        1,
        rx_frame,
        kFrameLen,
        pdMS_TO_TICKS(Config::DFR_GAS_I2C_TIMEOUT_MS)
    );
#ifndef UNIT_TEST
    const I2cBusRecovery::LineState lines_after_read =
        I2cBusRecovery::sample(
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN),
            static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN));
    if (err != ESP_OK) {
        LOGW(config_.log_tag,
             "cmd=0x%02X stage=read-back err=%d(%s) lines before=%u/%u after_write=%u/%u before_read=%u/%u after=%u/%u",
             static_cast<unsigned>(tx_frame[2]),
             static_cast<int>(err),
             esp_err_to_name(err),
             lines_before_write.sda_high ? 1U : 0U,
             lines_before_write.scl_high ? 1U : 0U,
             lines_after_write.sda_high ? 1U : 0U,
             lines_after_write.scl_high ? 1U : 0U,
             lines_before_read.sda_high ? 1U : 0U,
             lines_before_read.scl_high ? 1U : 0U,
             lines_after_read.sda_high ? 1U : 0U,
             lines_after_read.scl_high ? 1U : 0U);
    }
#endif
    if (err != ESP_OK && failure_reason) {
        *failure_reason = FailureReason::I2cRead;
    }
    return err == ESP_OK;
}

bool DfrMultiGasSensor::isInStartupFaultGrace(uint32_t now_ms) const {
    return warmup_started_ &&
           (now_ms - warmup_started_ms_) < Config::DFR_GAS_STARTUP_FAULT_GRACE_MS;
}

const char *DfrMultiGasSensor::failureReasonLabel(FailureReason reason) {
    switch (reason) {
        case FailureReason::I2cWrite:
            return "i2c write failed";
        case FailureReason::I2cRead:
            return "i2c read failed";
        case FailureReason::BadHeader:
            return "bad frame header";
        case FailureReason::BadChecksum:
            return "bad checksum";
        case FailureReason::BadDecimals:
            return "bad decimals";
        case FailureReason::None:
        default:
            return "unknown";
    }
}

uint8_t DfrMultiGasSensor::checksum7(const uint8_t *frame) {
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 7; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    return static_cast<uint8_t>(~sum + 1);
}

uint8_t DfrMultiGasSensor::checksum6(const uint8_t *frame) {
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 6; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    return static_cast<uint8_t>(~sum + 1);
}

void DfrMultiGasSensor::buildFrame(uint8_t command,
                                   uint8_t arg0,
                                   uint8_t arg1,
                                   uint8_t arg2,
                                   uint8_t arg3,
                                   uint8_t arg4,
                                   uint8_t *frame) {
    frame[0] = 0xFF;
    frame[1] = 0x01;
    frame[2] = command;
    frame[3] = arg0;
    frame[4] = arg1;
    frame[5] = arg2;
    frame[6] = arg3;
    frame[7] = arg4;
    frame[8] = checksum7(frame);
}
