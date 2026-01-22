// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Dps310.h"
#include <driver/i2c.h>
#include <math.h>
#include "core/Logger.h"
#include "config/AppConfig.h"

bool Dps310::begin() {
    ok_ = false;
    addr_ = 0;
    pressure_has_ = false;
    pressure_filtered_ = 0.0f;
    temperature_c_ = 0.0f;
    c0_ = 0;
    c1_ = 0;
    c00_ = 0;
    c10_ = 0;
    c01_ = 0;
    c11_ = 0;
    c20_ = 0;
    c21_ = 0;
    c30_ = 0;
    temp_scale_ = 0;
    pressure_scale_ = 0;
    raw_temperature_ = 0;
    raw_pressure_ = 0;
    last_poll_ms_ = 0;
    last_data_ms_ = 0;
    no_data_since_ms_ = 0;
    last_recover_ms_ = 0;
    pressure_valid_ = false;
    has_new_data_ = false;
    return true;
}

int32_t Dps310::twosComplement(int32_t val, uint8_t bits) {
    if (val & (static_cast<uint32_t>(1) << (bits - 1))) {
        val -= static_cast<uint32_t>(1) << bits;
    }
    return val;
}

bool Dps310::detect(uint8_t addr) {
    uint8_t reg = Config::DPS310_PRODREVID;
    uint8_t value = 0;
    esp_err_t err = i2c_master_write_read_device(
        Config::I2C_PORT,
        addr,
        &reg,
        1,
        &value,
        1,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    if (err != ESP_OK) {
        return false;
    }
    if (value != 0x10) {
        return false;
    }
    addr_ = addr;
    return true;
}

bool Dps310::writeU8(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    esp_err_t err = i2c_master_write_to_device(
        Config::I2C_PORT,
        addr_,
        data,
        sizeof(data),
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Dps310::readBytes(uint8_t reg, uint8_t *buf, size_t len) {
    esp_err_t err = i2c_master_write_read_device(
        Config::I2C_PORT,
        addr_,
        &reg,
        1,
        buf,
        len,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

bool Dps310::readU8(uint8_t reg, uint8_t &value) {
    return readBytes(reg, &value, 1);
}

bool Dps310::waitMeasBit(uint8_t bit, uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        uint8_t value = 0;
        if (!readU8(Config::DPS310_MEASCFG, value)) {
            return false;
        }
        if (value & (1u << bit)) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool Dps310::readCalibration() {
    uint8_t coeffs[18] = {};
    if (!readBytes(0x10, coeffs, sizeof(coeffs))) {
        return false;
    }

    c0_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[0]) << 4) |
                               ((static_cast<uint16_t>(coeffs[1]) >> 4) & 0x0F));
    c0_ = static_cast<int16_t>(twosComplement(c0_, 12));

    c1_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[1] & 0x0F) << 8) |
                               static_cast<uint16_t>(coeffs[2]));
    c1_ = static_cast<int16_t>(twosComplement(c1_, 12));

    c00_ = (static_cast<uint32_t>(coeffs[3]) << 12) |
           (static_cast<uint32_t>(coeffs[4]) << 4) |
           ((static_cast<uint32_t>(coeffs[5]) >> 4) & 0x0F);
    c00_ = twosComplement(c00_, 20);

    c10_ = (static_cast<uint32_t>(coeffs[5] & 0x0F) << 16) |
           (static_cast<uint32_t>(coeffs[6]) << 8) |
           static_cast<uint32_t>(coeffs[7]);
    c10_ = twosComplement(c10_, 20);

    c01_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[8]) << 8) |
                                static_cast<uint16_t>(coeffs[9]));
    c01_ = static_cast<int16_t>(twosComplement(c01_, 16));

    c11_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[10]) << 8) |
                                static_cast<uint16_t>(coeffs[11]));
    c11_ = static_cast<int16_t>(twosComplement(c11_, 16));

    c20_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[12]) << 8) |
                                static_cast<uint16_t>(coeffs[13]));
    c20_ = static_cast<int16_t>(twosComplement(c20_, 16));

    c21_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[14]) << 8) |
                                static_cast<uint16_t>(coeffs[15]));
    c21_ = static_cast<int16_t>(twosComplement(c21_, 16));

    c30_ = static_cast<int16_t>((static_cast<uint16_t>(coeffs[16]) << 8) |
                                static_cast<uint16_t>(coeffs[17]));
    c30_ = static_cast<int16_t>(twosComplement(c30_, 16));

    return true;
}

bool Dps310::setBit(uint8_t reg, uint8_t bit, bool value) {
    uint8_t data = 0;
    if (!readU8(reg, data)) {
        return false;
    }
    if (value) {
        data |= (1u << bit);
    } else {
        data &= ~(1u << bit);
    }
    return writeU8(reg, data);
}

bool Dps310::configPressure(uint8_t rate, uint8_t os) {
    uint8_t cfg = static_cast<uint8_t>(((rate & 0x07) << 4) | (os & 0x0F));
    if (!writeU8(Config::DPS310_PRSCFG, cfg)) {
        return false;
    }
    if (!setBit(Config::DPS310_CFGREG, 2, os > 3)) {
        return false;
    }
    static const int32_t scale[] = {
        524288, 1572864, 3670016, 7864320, 253952, 516096, 1040384, 2088960
    };
    pressure_scale_ = scale[os & 0x07];
    return true;
}

bool Dps310::configTemperature(uint8_t rate, uint8_t os) {
    uint8_t cfg = static_cast<uint8_t>(((rate & 0x07) << 4) | (os & 0x0F));
    if (!writeU8(Config::DPS310_TMPCFG, cfg)) {
        return false;
    }
    if (!setBit(Config::DPS310_CFGREG, 3, os > 3)) {
        return false;
    }
    static const int32_t scale[] = {
        524288, 1572864, 3670016, 7864320, 253952, 516096, 1040384, 2088960
    };
    temp_scale_ = scale[os & 0x07];

    uint8_t tmpcoef = 0;
    if (readU8(Config::DPS310_TMPCOEFSRCE, tmpcoef)) {
        bool src = (tmpcoef & 0x80) != 0;
        setBit(Config::DPS310_TMPCFG, 7, src);
    }
    return true;
}

bool Dps310::setMode(uint8_t mode) {
    uint8_t cfg = 0;
    if (!readU8(Config::DPS310_MEASCFG, cfg)) {
        return false;
    }
    cfg = static_cast<uint8_t>((cfg & 0xF8) | (mode & 0x07));
    return writeU8(Config::DPS310_MEASCFG, cfg);
}

bool Dps310::temperatureAvailable() {
    uint8_t cfg = 0;
    if (!readU8(Config::DPS310_MEASCFG, cfg)) {
        return false;
    }
    return (cfg & (1u << 5)) != 0;
}

bool Dps310::pressureAvailable() {
    uint8_t cfg = 0;
    if (!readU8(Config::DPS310_MEASCFG, cfg)) {
        return false;
    }
    return (cfg & (1u << 4)) != 0;
}

bool Dps310::readRaw() {
    uint8_t buf[3] = {};
    if (!readBytes(Config::DPS310_TMPB2, buf, sizeof(buf))) {
        return false;
    }
    raw_temperature_ = twosComplement(
        (static_cast<uint32_t>(buf[0]) << 16) |
        (static_cast<uint32_t>(buf[1]) << 8) |
        static_cast<uint32_t>(buf[2]),
        24
    );
    if (!readBytes(Config::DPS310_PRSB2, buf, sizeof(buf))) {
        return false;
    }
    raw_pressure_ = twosComplement(
        (static_cast<uint32_t>(buf[0]) << 16) |
        (static_cast<uint32_t>(buf[1]) << 8) |
        static_cast<uint32_t>(buf[2]),
        24
    );
    return true;
}

bool Dps310::compute(float &pressure_hpa, float &temperature_c) {
    if (temp_scale_ == 0 || pressure_scale_ == 0) {
        return false;
    }
    float scaled_temp = static_cast<float>(raw_temperature_) / temp_scale_;
    temperature_c = scaled_temp * c1_ + (c0_ / 2.0f);

    float pressure = static_cast<float>(raw_pressure_) / pressure_scale_;
    pressure = c00_ +
               pressure * (c10_ +
                           pressure * (c20_ + pressure * c30_)) +
               scaled_temp * (c01_ + pressure * (c11_ + pressure * c21_));

    pressure_hpa = pressure / 100.0f;
    return true;
}

bool Dps310::start() {
    if (detect(Config::DPS310_ADDR_PRIMARY)) {
        LOGI("DPS310", "found at 0x77");
    } else if (detect(Config::DPS310_ADDR_ALT)) {
        LOGI("DPS310", "found at 0x76");
    } else {
        ok_ = false;
        return false;
    }

    if (!writeU8(Config::DPS310_RESET, 0x89)) {
        ok_ = false;
        return false;
    }
    delay(20);

    if (!waitMeasBit(7, 500)) {
        LOGW("DPS310", "COEF_RDY timeout");
    }
    if (!waitMeasBit(6, 500)) {
        LOGW("DPS310", "SENSOR_RDY timeout");
    }

    if (!readCalibration()) {
        LOGW("DPS310", "calib read failed");
        ok_ = false;
        return false;
    }
    if (!configPressure(3, 5)) {
        ok_ = false;
        return false;
    }
    if (!configTemperature(3, 5)) {
        ok_ = false;
        return false;
    }
    if (!setMode(Config::DPS310_MODE_CONT_PRESTEMP)) {
        ok_ = false;
        return false;
    }
    ok_ = true;
    pressure_valid_ = false;
    pressure_has_ = false;
    no_data_since_ms_ = 0;
    last_data_ms_ = 0;
    has_new_data_ = false;
    return true;
}

void Dps310::tryRecover(uint32_t now, const char *reason) {
    if (now - last_recover_ms_ < Config::DPS310_RECOVER_COOLDOWN_MS) {
        return;
    }
    last_recover_ms_ = now;
    Logger::log(Logger::Warn, "DPS310", "%s - reinit", reason);
    bool ok = start();
    if (ok) {
        LOGI("DPS310", "recovery OK");
        ok_ = true;
        pressure_has_ = false;
        no_data_since_ms_ = 0;
        last_data_ms_ = 0;
        pressure_valid_ = false;
    } else {
        LOGW("DPS310", "recovery failed");
        ok_ = false;
    }
}

void Dps310::handleNoData(uint32_t now, const char *reason) {
    if (no_data_since_ms_ == 0) {
        no_data_since_ms_ = now;
    }
    if (last_data_ms_ != 0 && (now - last_data_ms_) > Config::DPS310_STALE_MS) {
        pressure_valid_ = false;
    }
    if (now - no_data_since_ms_ >= Config::DPS310_RECOVER_MS) {
        tryRecover(now, reason);
    }
}

void Dps310::poll() {
    uint32_t now = millis();
    if (!ok_) {
        tryRecover(now, "not ready");
        return;
    }
    if (now - last_poll_ms_ < Config::DPS310_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    if (!temperatureAvailable() || !pressureAvailable()) {
        handleNoData(now, "no data");
        return;
    }

    if (!readRaw()) {
        handleNoData(now, "read fail");
        return;
    }

    float pressure_hpa = 0.0f;
    float temperature_c = 0.0f;
    if (!compute(pressure_hpa, temperature_c)) {
        handleNoData(now, "compute fail");
        return;
    }

    if (!isfinite(pressure_hpa) || pressure_hpa <= 0.0f) {
        handleNoData(now, "invalid");
        return;
    }
    temperature_c_ = temperature_c;
    no_data_since_ms_ = 0;

    if (!pressure_has_) {
        pressure_filtered_ = pressure_hpa;
        pressure_has_ = true;
    } else {
        pressure_filtered_ += Config::DPS310_PRESSURE_ALPHA *
                              (pressure_hpa - pressure_filtered_);
    }

    last_data_ms_ = now;
    pressure_valid_ = true;
    has_new_data_ = true;
}

bool Dps310::takeNewData(float &pressure_hpa, float &temperature_c) {
    if (!has_new_data_) {
        return false;
    }
    pressure_hpa = pressure_filtered_;
    temperature_c = temperature_c_;
    has_new_data_ = false;
    return true;
}

void Dps310::invalidate() {
    pressure_valid_ = false;
    has_new_data_ = false;
}
