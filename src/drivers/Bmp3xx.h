// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>
#include "core/CooperativeStart.h"

class Bmp3xx {
public:
    enum class Variant : uint8_t {
        Unknown = 0,
        BMP388,
        BMP390
    };

    bool begin();
    bool start();
    void beginLateStart();
    CooperativeStart::Result pollLateStart(uint32_t now_ms);
    bool isLateStartActive() const { return late_start_phase_ != LateStartPhase::Idle; }
    void poll();
    bool takeNewData(float &pressure_hpa, float &temperature_c);
    bool isOk() const { return ok_; }
    bool isPressureValid() const { return pressure_valid_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    Variant variant() const { return variant_; }
    const char *variantLabel() const;
    void invalidate();

private:
    enum class LateStartPhase : uint8_t {
        Idle = 0,
        DetectPrimaryChip,
        DetectPrimaryErr,
        DetectPrimaryPower,
        DetectPrimaryOsr,
        DetectPrimaryOdr,
        DetectAltChip,
        DetectAltErr,
        DetectAltPower,
        DetectAltOsr,
        DetectAltOdr,
        SoftReset,
        WaitReset,
        WaitCmdReady,
        ReadCalibration,
        WriteOsr,
        WriteOdr,
        WriteConfig,
        WritePower,
        WaitConfig,
        ReadError,
    };

    bool lateDetectRead(uint8_t addr, uint8_t reg, uint8_t reserved_mask,
                        LateStartPhase next_phase, LateStartPhase fallback_phase);
    CooperativeStart::Result finishLateStart(bool success);
    struct Calibration {
        double par_t1 = 0.0;
        double par_t2 = 0.0;
        double par_t3 = 0.0;
        double par_p1 = 0.0;
        double par_p2 = 0.0;
        double par_p3 = 0.0;
        double par_p4 = 0.0;
        double par_p5 = 0.0;
        double par_p6 = 0.0;
        double par_p7 = 0.0;
        double par_p8 = 0.0;
        double par_p9 = 0.0;
        double par_p10 = 0.0;
        double par_p11 = 0.0;
        double t_lin = 0.0;
    };

    bool detect(uint8_t addr);
    bool writeU8(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t *buf, size_t len);
    bool readU8(uint8_t reg, uint8_t &value);
    bool softReset();
    bool waitCmdReady(uint32_t timeout_ms);
    bool readCalibration();
    bool configure();
    bool dataReady();
    bool readRaw();
    bool compute(float &pressure_hpa, float &temperature_c);
    void tryRecover(uint32_t now, const char *reason);
    void handleNoData(uint32_t now, const char *reason);

    bool ok_ = false;
    uint8_t addr_ = 0;
    bool pressure_has_ = false;
    float pressure_filtered_ = 0.0f;
    float temperature_c_ = 0.0f;
    Calibration calib_;
    uint32_t raw_temperature_ = 0;
    uint32_t raw_pressure_ = 0;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint32_t no_data_since_ms_ = 0;
    uint32_t last_recover_ms_ = 0;
    bool pressure_valid_ = false;
    bool has_new_data_ = false;
    Variant variant_ = Variant::Unknown;
    LateStartPhase late_start_phase_ = LateStartPhase::Idle;
    uint32_t late_start_due_ms_ = 0;
    uint32_t late_start_deadline_ms_ = 0;
};
