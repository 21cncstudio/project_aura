// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "Sfa3x.h"
#include <math.h>
#include "core/Logger.h"
#include "config/AppConfig.h"
#include "core/I2CHelper.h"

bool Sfa3x::begin() {
    ok_ = true;
    measuring_ = false;
    data_valid_ = false;
    has_new_data_ = false;
    last_hcho_ppb_ = 0.0f;
    last_poll_ms_ = 0;
    last_data_ms_ = 0;
    fail_count_ = 0;
    return true;
}

void Sfa3x::start() {
    if (measuring_) {
        return;
    }
    if (!writeCmd(Config::SFA3X_CMD_START)) {
        ok_ = false;
        return;
    }
    delay(Config::SFA3X_START_DELAY_MS);
    measuring_ = true;
    ok_ = true;
}

void Sfa3x::stop() {
    if (!measuring_) {
        return;
    }
    if (!writeCmd(Config::SFA3X_CMD_STOP)) {
        return;
    }
    delay(Config::SFA3X_STOP_DELAY_MS);
    measuring_ = false;
}

bool Sfa3x::readData(float &hcho_ppb) {
    uint16_t words[3];
    if (!readWords(Config::SFA3X_CMD_READ_VALUES, words, 3, Config::SFA3X_READ_DELAY_MS)) {
        return false;
    }
    const int16_t hcho_raw = static_cast<int16_t>(words[0]);
    hcho_ppb = hcho_raw / 5.0f;
    return true;
}

void Sfa3x::poll() {
    if (!ok_ || !measuring_) {
        return;
    }
    const uint32_t now = millis();
    if (now - last_poll_ms_ < Config::SFA3X_POLL_MS) {
        return;
    }
    last_poll_ms_ = now;

    float hcho_ppb = 0.0f;
    if (!readData(hcho_ppb)) {
        if (++fail_count_ == 3) {
            LOGW("SFA30", "read values failed");
            fail_count_ = 0;
        }
        return;
    }

    fail_count_ = 0;
    if (isfinite(hcho_ppb) && hcho_ppb >= 0.0f) {
        last_hcho_ppb_ = hcho_ppb;
        data_valid_ = true;
        has_new_data_ = true;
        last_data_ms_ = now;
    }
}

bool Sfa3x::takeNewData(float &hcho_ppb) {
    if (!has_new_data_ || !data_valid_) {
        return false;
    }
    hcho_ppb = last_hcho_ppb_;
    has_new_data_ = false;
    return true;
}

void Sfa3x::invalidate() {
    data_valid_ = false;
    has_new_data_ = false;
}

bool Sfa3x::readWords(uint16_t cmd, uint16_t *out, size_t words, uint32_t delay_ms) {
    if (!writeCmd(cmd)) {
        return false;
    }
    delay(delay_ms);
    const size_t bytes = words * 3;
    uint8_t buf[9];
    if (bytes > sizeof(buf)) {
        return false;
    }
    if (!readBytes(buf, bytes)) {
        return false;
    }
    for (size_t i = 0; i < words; ++i) {
        const uint8_t *p = &buf[i * 3];
        if (I2C::crc8(p, 2) != p[2]) {
            return false;
        }
        out[i] = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    }
    return true;
}

bool Sfa3x::writeCmd(uint16_t cmd) {
    return I2C::write_cmd(Config::SFA3X_ADDR, cmd, nullptr, 0) == ESP_OK;
}

bool Sfa3x::readBytes(uint8_t *buf, size_t len) {
    return I2C::read_bytes(Config::SFA3X_ADDR, buf, len) == ESP_OK;
}
