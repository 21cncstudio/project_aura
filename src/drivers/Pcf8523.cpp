// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Pcf8523.h"
#include <driver/i2c.h>
#include <string.h>
#include "config/AppConfig.h"

namespace {

constexpr uint8_t kPcf8523BatteryLowFlag = 0x04;

uint8_t bcd2binLocal(uint8_t val) {
    return val - 6 * (val >> 4);
}

bool isBcdByte(uint8_t raw) {
    return ((raw >> 4) & 0x0F) <= 9 && (raw & 0x0F) <= 9;
}

bool isBcdWithin(uint8_t raw, uint8_t mask, uint8_t max_value, bool allow_zero) {
    raw &= mask;
    if (!isBcdByte(raw)) {
        return false;
    }
    const uint8_t value = bcd2binLocal(raw);
    if (!allow_zero && value == 0) {
        return false;
    }
    return value <= max_value;
}

} // namespace

bool Pcf8523::probe() {
    bool matched = false;
    return readProbeSignature(matched) && matched;
}

bool Pcf8523::readProbeSignature(bool &matched) {
    uint8_t signature[5] = { 0 };
    matched = false;
    if (!read(Config::PCF8523_REG_OFFSET, signature, sizeof(signature))) {
        return false;
    }
    matched = signature[0] == 0x00 &&
              signature[1] == 0x00 &&
              signature[2] == Config::PCF8523_TMR_FREQ_RESET &&
              signature[4] == Config::PCF8523_TMR_FREQ_RESET;
    return true;
}

bool Pcf8523::probeFallback() {
    uint8_t ctrl[3] = { 0 };
    uint8_t time_regs[7] = { 0 };
    uint8_t timer_regs[5] = { 0 };
    if (!readProbeFallbackControl(ctrl) ||
        !readProbeFallbackTime(time_regs) ||
        !readProbeFallbackTimers(timer_regs)) {
        return false;
    }

    return probeFallbackMatches(ctrl, time_regs, timer_regs);
}

bool Pcf8523::readProbeFallbackControl(uint8_t out[3]) {
    return read(Config::PCF8523_REG_CONTROL_1, out, 3);
}

bool Pcf8523::readProbeFallbackTime(uint8_t out[7]) {
    return read(Config::PCF8523_REG_SECONDS, out, 7);
}

bool Pcf8523::readProbeFallbackTimers(uint8_t out[5]) {
    return read(Config::PCF8523_REG_OFFSET, out, 5);
}

bool Pcf8523::probeFallbackMatches(const uint8_t control[3],
                                   const uint8_t time_regs[7],
                                   const uint8_t timer_regs[5]) {
    if (!control || !time_regs || !timer_regs) {
        return false;
    }

    const bool ctrl1_layout_valid = (control[0] & 0x40) == 0;
    const bool ctrl3_reserved_clear = (control[2] & 0x10) == 0;
    const uint8_t weekday_raw = time_regs[4];
    const bool weekday_layout_valid = (weekday_raw & 0xF8) == 0;
    const bool month_layout_valid = (time_regs[5] & 0xE0) == 0;
    const bool timer_a_freq_layout_valid =
        (timer_regs[Config::PCF8523_REG_TMR_A_FREQ_CTRL - Config::PCF8523_REG_OFFSET] & 0xF8) == 0;
    const bool timer_b_freq_layout_valid =
        (timer_regs[Config::PCF8523_REG_TMR_B_FREQ_CTRL - Config::PCF8523_REG_OFFSET] & 0xF8) == 0;

    return ctrl1_layout_valid &&
           ctrl3_reserved_clear &&
           weekday_layout_valid &&
           month_layout_valid &&
           timer_a_freq_layout_valid &&
           timer_b_freq_layout_valid;
}

bool Pcf8523::begin() {
    // Enable battery switch-over (standard mode, battery low detection on).
    // Default after POR is 0xE0 (switch-over disabled) which causes
    // spurious OS bit on any VCC glitch or software restart.
    uint8_t ctrl3 = 0x00;
    return write(Config::PCF8523_REG_CONTROL_3, &ctrl3, 1);
}

uint8_t Pcf8523::bcd2bin(uint8_t val) {
    return val - 6 * (val >> 4);
}

uint8_t Pcf8523::bin2bcd(uint8_t val) {
    return val + 6 * (val / 10);
}

bool Pcf8523::read(uint8_t reg, uint8_t *buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }
    esp_err_t err = i2c_master_write_read_device(
        Config::SENSOR_I2C_PORT,
        Config::PCF8523_ADDR,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(Config::SENSOR_I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Pcf8523::write(uint8_t reg, const uint8_t *buf, size_t len) {
    if (!buf || len == 0 || len > 7) {
        return false;
    }
    uint8_t data[8] = { 0 };
    data[0] = reg;
    memcpy(&data[1], buf, len);
    esp_err_t err = i2c_master_write_to_device(
        Config::SENSOR_I2C_PORT,
        Config::PCF8523_ADDR,
        data,
        len + 1,
        pdMS_TO_TICKS(Config::SENSOR_I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Pcf8523::readTime(tm &out, bool &osc_stop, bool &valid) {
    uint8_t buf[7] = { 0 };
    if (!read(Config::PCF8523_REG_SECONDS, buf, sizeof(buf))) {
        return false;
    }
    osc_stop = (buf[0] & 0x80) != 0;

    const bool layout_valid =
        isBcdWithin(buf[0], 0x7F, 59, true) &&
        isBcdWithin(buf[1], 0x7F, 59, true) &&
        isBcdWithin(buf[2], 0x3F, 23, true) &&
        isBcdWithin(buf[3], 0x3F, 31, false) &&
        (buf[4] & 0xF8) == 0 &&
        (buf[5] & 0xE0) == 0 &&
        isBcdWithin(buf[5], 0x1F, 12, false) &&
        isBcdByte(buf[6]);

    memset(&out, 0, sizeof(out));
    out.tm_isdst = 0;
    if (!layout_valid) {
        valid = false;
        return true;
    }

    int sec = bcd2bin(buf[0] & 0x7F);
    int min = bcd2bin(buf[1] & 0x7F);
    int hour = bcd2bin(buf[2] & 0x3F);
    int day = bcd2bin(buf[3] & 0x3F);
    int wday = bcd2bin(buf[4] & 0x07);
    int month = bcd2bin(buf[5] & 0x1F);
    int year = bcd2bin(buf[6]) + 2000;
    valid = !(sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 ||
              month < 1 || month > 12 || year < 2000 || year > 2099);
    if (valid) {
        out.tm_sec = sec;
        out.tm_min = min;
        out.tm_hour = hour;
        out.tm_mday = day;
        out.tm_mon = month - 1;
        out.tm_year = year - 1900;
        out.tm_wday = wday;
    }
    return true;
}

bool Pcf8523::writeTime(const tm &utc_tm) {
    uint8_t buf[7] = { 0 };
    buf[0] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_sec)) & 0x7F;
    buf[1] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_min));
    buf[2] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_hour));
    buf[3] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_mday));
    buf[4] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_wday));
    buf[5] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_mon + 1));
    buf[6] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_year + 1900 - 2000));
    return write(Config::PCF8523_REG_SECONDS, buf, sizeof(buf));
}

bool Pcf8523::clearOscillatorStop() {
    uint8_t sec = 0;
    if (!read(Config::PCF8523_REG_SECONDS, &sec, 1)) {
        return false;
    }
    sec &= 0x7F;  // Clear OS bit (bit 7), keep seconds
    return write(Config::PCF8523_REG_SECONDS, &sec, 1);
}

bool Pcf8523::isBatteryLow(bool &low) {
    uint8_t ctrl3 = 0;
    if (!readControl3(ctrl3)) {
        return false;
    }
    low = (ctrl3 & kPcf8523BatteryLowFlag) != 0;
    return true;
}

bool Pcf8523::readControl3(uint8_t &value) {
    return read(Config::PCF8523_REG_CONTROL_3, &value, 1);
}
