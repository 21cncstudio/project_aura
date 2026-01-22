// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Pcf8523.h"
#include <driver/i2c.h>
#include <string.h>
#include "config/AppConfig.h"

bool Pcf8523::begin() {
    return true;
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
        Config::I2C_PORT,
        Config::PCF8523_ADDR,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
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
        Config::I2C_PORT,
        Config::PCF8523_ADDR,
        data,
        len + 1,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Pcf8523::readTime(tm &out, bool &osc_stop, bool &valid) {
    uint8_t buf[7] = { 0 };
    if (!read(Config::PCF8523_REG_SECONDS, buf, sizeof(buf))) {
        return false;
    }
    osc_stop = (buf[0] & 0x80) != 0;
    int sec = bcd2bin(buf[0] & 0x7F);
    int min = bcd2bin(buf[1] & 0x7F);
    int hour = bcd2bin(buf[2] & 0x3F);
    int day = bcd2bin(buf[3] & 0x3F);
    int wday = bcd2bin(buf[4] & 0x07);
    int month = bcd2bin(buf[5] & 0x1F);
    int year = bcd2bin(buf[6]) + 2000;
    valid = !(sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 ||
              month < 1 || month > 12 || year < 2000 || year > 2099);
    memset(&out, 0, sizeof(out));
    if (valid) {
        out.tm_sec = sec;
        out.tm_min = min;
        out.tm_hour = hour;
        out.tm_mday = day;
        out.tm_mon = month - 1;
        out.tm_year = year - 1900;
        out.tm_wday = wday;
    }
    out.tm_isdst = 0;
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
