// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>
#include <time.h>

class Pcf8523 {
public:
    bool probe();
    bool probeFallback();
    bool readProbeSignature(bool &matched);
    bool readProbeFallbackControl(uint8_t out[3]);
    bool readProbeFallbackTime(uint8_t out[7]);
    bool readProbeFallbackTimers(uint8_t out[5]);
    static bool probeFallbackMatches(const uint8_t control[3],
                                     const uint8_t time_regs[7],
                                     const uint8_t timer_regs[5]);
    bool begin();
    bool readTime(tm &out, bool &osc_stop, bool &valid);
    bool writeTime(const tm &utc_tm);
    bool clearOscillatorStop();
    bool isBatteryLow(bool &low);
    static const char *label() { return "PCF8523"; }

private:
    static uint8_t bcd2bin(uint8_t val);
    static uint8_t bin2bcd(uint8_t val);
    bool read(uint8_t reg, uint8_t *buf, size_t len);
    bool write(uint8_t reg, const uint8_t *buf, size_t len);
    bool readControl3(uint8_t &value);
};
