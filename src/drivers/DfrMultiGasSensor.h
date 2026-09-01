// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>

struct DfrMultiGasSensorConfig {
    const char *log_tag = "";
    const char *label = "";
    uint8_t address = 0;
    uint8_t expected_gas_type = 0;
    float min_ppm = 0.0f;
    float max_ppm = 0.0f;
    const uint8_t *allowed_gas_types = nullptr;
    size_t allowed_gas_type_count = 0;
    bool optional_installation = false;
    uint32_t runtime_transport_retry_delay_ms = 0;
    bool defer_semantic_invalidation_until_failure_limit = false;
};

class DfrMultiGasSensor {
public:
    enum class GasType : uint8_t {
        None = 0,
        NH3,
        SO2,
        NO2,
        CO,
        Unknown,
        H2S,
        O3,
        O2,
    };

    explicit DfrMultiGasSensor(const DfrMultiGasSensorConfig &config) : config_(config) {}

    // Startup and runtime recovery are deliberately read-only. Communication
    // mode provisioning must be performed outside the normal boot path and
    // followed by the sensor power cycle required by DFRobot.
    bool begin();
    bool start();
    void poll();

    bool isPresent() const { return present_; }
    bool isDataValid() const { return data_valid_; }
    bool isWarmupActive() const;
    float ppm() const { return ppm_; }
    uint8_t ppmDecimals() const { return ppm_decimals_; }
    float concentration() const { return ppm_; }
    uint8_t concentrationDecimals() const { return ppm_decimals_; }
    GasType gasType() const { return gas_type_; }
    uint8_t rawGasType() const { return raw_gas_type_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    void invalidate();

    const char *label() const { return config_.label; }
    uint8_t address() const { return config_.address; }
    static const char *gasTypeLabel(GasType type);

protected:
    static GasType mapGasType(uint8_t gas_type_raw);
    void clampPpm(float min_ppm, float max_ppm);

private:
    enum class FailureReason : uint8_t {
        None = 0,
        I2cWrite,
        I2cRead,
        BadHeader,
        BadChecksum,
        BadDecimals,
        UnexpectedGasType,
        InvalidConcentration,
    };

    bool isGasTypeAccepted(uint8_t gas_type_raw) const;
    bool pingAddress();
    bool readGasConcentration(float &ppm,
                              uint8_t &gas_type,
                              uint8_t &decimal_places,
                              FailureReason &failure_reason,
                              bool allow_runtime_transport_retry);
    bool readGasConcentrationOnce(float &ppm,
                                  uint8_t &gas_type,
                                  uint8_t &decimal_places,
                                  FailureReason &failure_reason,
                                  bool &retryable_transport_failure);
    bool transact(const uint8_t *tx_frame,
                  uint8_t *rx_frame,
                  FailureReason *failure_reason = nullptr,
                  bool *retryable_transport_failure = nullptr);
    void notePollFailure(uint32_t now, FailureReason reason);
    bool isInStartupFaultGrace(uint32_t now_ms) const;
    static const char *failureReasonLabel(FailureReason reason);
    static uint8_t checksum7(const uint8_t *frame);
    static uint8_t checksum6(const uint8_t *frame);
    static void buildFrame(uint8_t command,
                           uint8_t arg0,
                           uint8_t arg1,
                           uint8_t arg2,
                           uint8_t arg3,
                           uint8_t arg4,
                           uint8_t *frame);

    DfrMultiGasSensorConfig config_{};
    bool present_ = false;
    bool data_valid_ = false;
    bool warned_type_mismatch_ = false;
    float ppm_ = 0.0f;
    uint8_t ppm_decimals_ = 1;
    GasType gas_type_ = GasType::None;
    uint8_t raw_gas_type_ = 0;
    uint8_t fail_count_ = 0;
    bool warmup_started_ = false;
    bool warmup_complete_ = false;
    uint32_t warmup_started_ms_ = 0;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint32_t last_retry_ms_ = 0;
    bool fail_cooldown_active_ = false;
    uint32_t fail_cooldown_started_ms_ = 0;
    uint8_t cooldown_probe_fail_count_ = 0;
    uint8_t start_attempts_ = 0;
    uint8_t absent_retry_count_ = 0;
    bool absent_retry_active_ = false;
    bool absent_retry_exhausted_ = false;
    bool start_retry_exhausted_logged_ = false;
    FailureReason last_read_failure_reason_ = FailureReason::None;
};
