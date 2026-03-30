// SPDX-FileCopyrightText: 2025-2026 netscout2001
// SPDX-License-Identifier: GPL-3.0-or-later

#include "drivers/Lc709203f.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include <driver/i2c.h>

uint8_t Lc709203f::crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

bool Lc709203f::writeWord(uint8_t cmd, uint16_t value) {
    uint8_t buf[5];
    buf[0] = (kAddr << 1) | 0;
    buf[1] = cmd;
    buf[2] = static_cast<uint8_t>(value & 0xFF);
    buf[3] = static_cast<uint8_t>(value >> 8);
    buf[4] = crc8(buf, 4);
    const esp_err_t err = i2c_master_write_to_device(
        Config::I2C_PORT, kAddr, buf + 1, 4,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS));
    return err == ESP_OK;
}

bool Lc709203f::readWord(uint8_t cmd, uint16_t &value) {
    uint8_t rx[3] = {};
    const esp_err_t err = i2c_master_write_read_device(
        Config::I2C_PORT, kAddr, &cmd, 1, rx, 3,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS));
    if (err != ESP_OK) return false;
    uint8_t crc_buf[5];
    crc_buf[0] = (kAddr << 1) | 0;
    crc_buf[1] = cmd;
    crc_buf[2] = (kAddr << 1) | 1;
    crc_buf[3] = rx[0];
    crc_buf[4] = rx[1];
    if (crc8(crc_buf, 5) != rx[2]) return false;
    value = static_cast<uint16_t>(rx[0]) | (static_cast<uint16_t>(rx[1]) << 8);
    return true;
}

bool Lc709203f::readWordAveraged(uint8_t cmd, uint16_t &value) {
    uint32_t sum = 0; uint8_t count = 0;
    for (uint8_t i = 0; i < kAvgSamples; i++) {
        if (i > 0) vTaskDelay(pdMS_TO_TICKS(kAvgDelayMs));
        uint16_t raw = 0;
        if (readWord(cmd, raw)) { sum += raw; count++; }
    }
    if (count == 0) return false;
    value = static_cast<uint16_t>(sum / count);
    return true;
}

bool Lc709203f::begin() {
    present_ = false; data_valid_ = false;
    voltage_v_ = 0.0f; percent_ = 0.0f;
    current_dir_ = 0x0000; last_poll_ms_ = 0;
    return true;
}

bool Lc709203f::start() {
    present_ = false; data_valid_ = false; current_dir_ = 0x0000;

    uint16_t ic_ver = 0;
    if (!readWord(kCmdIcVersion, ic_ver)) {
        LOGI("LC709203F", "not found at 0x%02X", static_cast<unsigned>(kAddr));
        return false;
    }
    if (!writeWord(kCmdPowerMode, 0x0001)) {
        LOGW("LC709203F", "power mode set failed");
        return false;
    }
    writeWord(kCmdTempMode, 0x0001);
    writeWord(kCmdApa, kApa2000mAh);
    writeWord(kCmdBattProf, 0x0001);

    // Per datasheet: IC init time is within 80ms from power on
    vTaskDelay(pdMS_TO_TICKS(kInitSettleMs));

    // initRSOC only when battery is full (>= 4.10V)
    uint16_t raw_v = 0;
    if (readWordAveraged(kCmdVoltage, raw_v)) {
        if (raw_v >= 4100) {
            writeWord(0x07, 0xAA55);
            vTaskDelay(pdMS_TO_TICKS(2));
            LOGI("LC709203F", "initRSOC executed (battery full at %u mV)",
                 static_cast<unsigned>(raw_v));
        } else {
            LOGI("LC709203F", "initRSOC skipped (battery at %u mV)",
                 static_cast<unsigned>(raw_v));
        }
    }

    present_ = true;
    LOGI("LC709203F", "found at 0x%02X, IC ver: 0x%04X",
         static_cast<unsigned>(kAddr), static_cast<unsigned>(ic_ver));
    return true;
}

// poll() reads voltage, RSOC and Current Direction register.
// Current Direction is exposed raw via currentDir() for BatteryManager
// to apply hybrid logic (definitive for 0x0001/0xFFFF, RSOC delta for 0x0000).
void Lc709203f::poll() {
    if (!present_) return;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (last_poll_ms_ != 0 && (now - last_poll_ms_) < kPollIntervalMs) return;
    last_poll_ms_ = now;

    uint16_t raw_v = 0, raw_pct = 0, raw_dir = 0;
    if (!readWord(kCmdVoltage, raw_v) ||
        !readWord(kCmdRsoc, raw_pct) ||
        !readWord(kCmdCurrentDirection, raw_dir)) {
        data_valid_ = false;
        return;
    }

    const float v   = raw_v   / 1000.0f;
    const float pct = raw_pct / 10.0f;

    if (v < 2.5f || v > 4.5f) { data_valid_ = false; return; }

    voltage_v_   = v;
    percent_     = constrain(pct, 0.0f, 100.0f);
    current_dir_ = raw_dir;
    data_valid_  = true;
}

void Lc709203f::invalidate() { data_valid_ = false; }