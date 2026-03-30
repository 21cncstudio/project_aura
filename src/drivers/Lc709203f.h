// SPDX-FileCopyrightText: 2025-2026 netscout2001
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <Arduino.h>

// ── Low-level driver for the LC709203F LiPo fuel gauge ────────────────────────
// Uses raw ESP-IDF I2C (Config::I2C_PORT) like all other Aura drivers.
// No Adafruit Wire dependency -- no I2C conflict.
// I2C address: 0x0B (fixed, cannot be changed)
// Protocol: CRC8 (poly 0x07) on all reads and writes.
//
// Current Direction register (0x0A) per datasheet:
//   0x0001 = Charge Mode   → definitively charging
//   0xFFFF = Discharge Mode → definitively discharging
//   0x0000 = Auto Mode     → chip is undecided (BatteryManager resolves via RSOC delta)
class Lc709203f {
public:
    bool begin();
    bool start();
    void poll();

    bool     isPresent()      const { return present_; }
    bool     isDataValid()    const { return data_valid_; }
    float    cellVoltage()    const { return voltage_v_; }
    float    cellPercent()    const { return percent_; }
    uint16_t currentDir()     const { return current_dir_; }  // raw 0x0001/0x0000/0xFFFF
    void     invalidate();

private:
    bool writeWord(uint8_t cmd, uint16_t value);
    bool readWord(uint8_t cmd, uint16_t &value);
    bool readWordAveraged(uint8_t cmd, uint16_t &value);
    static uint8_t crc8(const uint8_t *data, size_t len);

    bool     present_      = false;
    bool     data_valid_   = false;
    float    voltage_v_    = 0.0f;
    float    percent_      = 0.0f;
    uint16_t current_dir_  = 0x0000;
    uint32_t last_poll_ms_ = 0;

    // Register commands
    static constexpr uint8_t kAddr                = 0x0B;
    static constexpr uint8_t kCmdPowerMode        = 0x15;
    static constexpr uint8_t kCmdApa              = 0x0B;
    static constexpr uint8_t kCmdTempMode         = 0x16;
    static constexpr uint8_t kCmdBattProf         = 0x12;
    static constexpr uint8_t kCmdVoltage          = 0x09;
    static constexpr uint8_t kCmdRsoc             = 0x0F;
    static constexpr uint8_t kCmdIcVersion        = 0x11;
    static constexpr uint8_t kCmdCurrentDirection = 0x0A;

    // Current Direction values
    static constexpr uint16_t kDirCharge    = 0x0001;
    static constexpr uint16_t kDirAuto      = 0x0000;
    static constexpr uint16_t kDirDischarge = 0xFFFF;

    // APA for ~2000 mAh
    static constexpr uint16_t kApa2000mAh    = 0x0036;

    // Poll interval
    static constexpr uint32_t kPollIntervalMs = 10000;

    // Startup averaging for initRSOC only (5 x 50ms = 200ms)
    static constexpr uint8_t  kAvgSamples    = 5;
    static constexpr uint32_t kAvgDelayMs    = 50;

    // IC init time per datasheet: 80ms from power on
    static constexpr uint32_t kInitSettleMs  = 80;
};