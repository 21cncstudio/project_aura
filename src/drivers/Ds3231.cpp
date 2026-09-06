// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Ds3231.h"

#include <driver/i2c.h>
#include <string.h>

#include "config/AppConfig.h"

namespace {

uint8_t bcd2bin(uint8_t val) {
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
    const uint8_t value = bcd2bin(raw);
    if (!allow_zero && value == 0) {
        return false;
    }
    return value <= max_value;
}

bool hasValidHourLayout(uint8_t raw) {
    if ((raw & 0x80) != 0) {
        return false;
    }
    if ((raw & 0x40) != 0) {
        return isBcdWithin(raw, 0x1F, 12, false);
    }
    return isBcdWithin(raw, 0x3F, 23, true);
}

} // namespace

bool Ds3231::probe() {
    return probeStrength() != ProbeStrength::None;
}

Ds3231::ProbeStrength Ds3231::probeStrength() {
    uint8_t meta_regs[4] = { 0 };
    if (!readProbeMeta(meta_regs)) {
        return ProbeStrength::None;
    }

    uint8_t wrap_regs[4] = { 0 };
    uint8_t head_regs[2] = { 0 };
    if (!readProbeWrap(wrap_regs) || !readProbeHead(head_regs)) {
        return ProbeStrength::None;
    }

    return classifyProbe(meta_regs, wrap_regs, head_regs);
}

bool Ds3231::readProbeMeta(uint8_t out[4]) {
    return read(Config::DS3231_REG_STATUS, out, 4);
}

bool Ds3231::readProbeWrap(uint8_t out[4]) {
    return read(Config::DS3231_REG_TEMP_MSB, out, 4);
}

bool Ds3231::readProbeHead(uint8_t out[2]) {
    return read(Config::DS3231_REG_SECONDS, out, 2);
}

Ds3231::ProbeStrength Ds3231::classifyProbe(const uint8_t meta[4],
                                             const uint8_t wrap[4],
                                             const uint8_t head[2]) {
    if (!meta || !wrap || !head) {
        return ProbeStrength::None;
    }

    // Keep probe read-only and identify the chip only by immutable register shape.
    // Calendar contents can be dirty after power loss and must not affect detect.
    const bool meta_valid =
        (meta[0] & Config::DS3231_STATUS_RESERVED_MASK) == 0 &&
        (meta[3] & Config::DS3231_TEMP_LSB_UNUSED_MASK) == 0;
    const bool wrap_valid =
        wrap[2] == head[0] &&
        wrap[3] == head[1];

    if (!meta_valid || !wrap_valid) {
        return ProbeStrength::None;
    }

    if ((meta[3] & 0xC0) != 0) {
        return ProbeStrength::Strong;
    }
    return ProbeStrength::Weak;
}

bool Ds3231::begin() {
    return true;
}

uint8_t Ds3231::bcd2bin(uint8_t val) {
    return val - 6 * (val >> 4);
}

uint8_t Ds3231::bin2bcd(uint8_t val) {
    return val + 6 * (val / 10);
}

bool Ds3231::read(uint8_t reg, uint8_t *buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }
    const esp_err_t err = i2c_master_write_read_device(
        Config::SENSOR_I2C_PORT,
        Config::DS3231_ADDR,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(Config::SENSOR_I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Ds3231::write(uint8_t reg, const uint8_t *buf, size_t len) {
    if (!buf || len == 0 || len > 18) {
        return false;
    }
    uint8_t data[19] = { 0 };
    data[0] = reg;
    memcpy(&data[1], buf, len);
    const esp_err_t err = i2c_master_write_to_device(
        Config::SENSOR_I2C_PORT,
        Config::DS3231_ADDR,
        data,
        len + 1,
        pdMS_TO_TICKS(Config::SENSOR_I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Ds3231::readCalendar(tm &out, bool &valid) {
    uint8_t buf[7] = { 0 };
    if (!read(Config::DS3231_REG_SECONDS, buf, sizeof(buf))) {
        return false;
    }

    const bool layout_valid =
        isBcdWithin(buf[0], 0x7F, 59, true) &&
        isBcdWithin(buf[1], 0x7F, 59, true) &&
        hasValidHourLayout(buf[2]) &&
        (buf[3] & 0xF8) == 0 &&
        (buf[3] & 0x07) >= 1 &&
        (buf[3] & 0x07) <= 7 &&
        (buf[4] & 0xC0) == 0 &&
        isBcdWithin(buf[4], 0x3F, 31, false) &&
        (buf[5] & 0x60) == 0 &&
        isBcdWithin(buf[5], 0x1F, 12, false) &&
        isBcdByte(buf[6]);

    memset(&out, 0, sizeof(out));
    out.tm_isdst = 0;
    if (!layout_valid) {
        valid = false;
        return true;
    }

    const int sec = bcd2bin(buf[0] & 0x7F);
    const int min = bcd2bin(buf[1] & 0x7F);
    int hour = 0;
    if ((buf[2] & 0x40) != 0) {
        hour = bcd2bin(buf[2] & 0x1F);
        const bool pm = (buf[2] & 0x20) != 0;
        if (hour == 12) {
            hour = pm ? 12 : 0;
        } else if (pm) {
            hour += 12;
        }
    } else {
        hour = bcd2bin(buf[2] & 0x3F);
    }

    const int day = bcd2bin(buf[4] & 0x3F);
    const int month = bcd2bin(buf[5] & 0x1F);
    const int year = bcd2bin(buf[6]) + 2000;
    const int weekday = buf[3] & 0x07;
    valid = !(sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 ||
              month < 1 || month > 12 || year < 2000 || year > 2099 ||
              weekday < 1 || weekday > 7);
    if (valid) {
        out.tm_sec = sec;
        out.tm_min = min;
        out.tm_hour = hour;
        out.tm_mday = day;
        out.tm_mon = month - 1;
        out.tm_year = year - 1900;
        out.tm_wday = weekday % 7;
    }
    return true;
}

bool Ds3231::readStatus(uint8_t &status) {
    return read(Config::DS3231_REG_STATUS, &status, 1);
}

bool Ds3231::readTime(tm &out, bool &osc_stop, bool &valid) {
    uint8_t status = 0;
    if (!readCalendar(out, valid) || !readStatus(status)) {
        return false;
    }
    osc_stop = (status & Config::DS3231_STATUS_OSF) != 0;
    return true;
}

bool Ds3231::writeCalendar(const tm &utc_tm) {
    const uint8_t weekday = (utc_tm.tm_wday == 0) ? 7 : utc_tm.tm_wday;
    uint8_t buf[7] = { 0 };
    buf[0] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_sec)) & 0x7F;
    buf[1] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_min)) & 0x7F;
    buf[2] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_hour)) & 0x3F;
    buf[3] = bin2bcd(weekday) & 0x07;
    buf[4] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_mday)) & 0x3F;
    buf[5] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_mon + 1)) & 0x1F;
    buf[6] = bin2bcd(static_cast<uint8_t>(utc_tm.tm_year + 1900 - 2000));
    return write(Config::DS3231_REG_SECONDS, buf, sizeof(buf));
}

bool Ds3231::writeStatus(uint8_t status) {
    return write(Config::DS3231_REG_STATUS, &status, 1);
}

bool Ds3231::writeTime(const tm &utc_tm) {
    if (!writeCalendar(utc_tm)) {
        return false;
    }
    return clearOscillatorStop();
}

bool Ds3231::clearOscillatorStop() {
    uint8_t status = 0;
    if (!readStatus(status)) {
        return false;
    }
    status &= static_cast<uint8_t>(~Config::DS3231_STATUS_OSF);
    return writeStatus(status);
}

bool Ds3231::isBatteryLow(bool &low) {
    low = false;
    return false;
}
