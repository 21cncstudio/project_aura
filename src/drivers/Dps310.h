// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>

class Dps310 {
public:
    bool begin();
    bool start();
    void poll();
    bool takeNewData(float &pressure_hpa, float &temperature_c);
    bool isOk() const { return ok_; }
    bool isPressureValid() const { return pressure_valid_; }
    uint32_t lastDataMs() const { return last_data_ms_; }
    void invalidate();

private:
    static int32_t twosComplement(int32_t val, uint8_t bits);
    bool detect(uint8_t addr);
    bool writeU8(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t *buf, size_t len);
    bool readU8(uint8_t reg, uint8_t &value);
    bool waitMeasBit(uint8_t bit, uint32_t timeout_ms);
    bool readCalibration();
    bool setBit(uint8_t reg, uint8_t bit, bool value);
    bool configPressure(uint8_t rate, uint8_t os);
    bool configTemperature(uint8_t rate, uint8_t os);
    bool setMode(uint8_t mode);
    bool temperatureAvailable();
    bool pressureAvailable();
    bool readRaw();
    bool compute(float &pressure_hpa, float &temperature_c);
    void tryRecover(uint32_t now, const char *reason);
    void handleNoData(uint32_t now, const char *reason);

    bool ok_ = false;
    uint8_t addr_ = 0;
    bool pressure_has_ = false;
    float pressure_filtered_ = 0.0f;
    float temperature_c_ = 0.0f;
    int16_t c0_ = 0;
    int16_t c1_ = 0;
    int32_t c00_ = 0;
    int32_t c10_ = 0;
    int16_t c01_ = 0;
    int16_t c11_ = 0;
    int16_t c20_ = 0;
    int16_t c21_ = 0;
    int16_t c30_ = 0;
    int32_t temp_scale_ = 0;
    int32_t pressure_scale_ = 0;
    int32_t raw_temperature_ = 0;
    int32_t raw_pressure_ = 0;
    uint32_t last_poll_ms_ = 0;
    uint32_t last_data_ms_ = 0;
    uint32_t no_data_since_ms_ = 0;
    uint32_t last_recover_ms_ = 0;
    bool pressure_valid_ = false;
    bool has_new_data_ = false;
};
