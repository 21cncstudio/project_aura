// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <time.h>

class Ds3231 {
public:
    enum class ProbeStrength : uint8_t {
        None = 0,
        Weak,
        Strong,
    };

    bool probe();
    ProbeStrength probeStrength();
    bool readProbeMeta(uint8_t out[4]);
    bool readProbeWrap(uint8_t out[4]);
    bool readProbeHead(uint8_t out[2]);
    static ProbeStrength classifyProbe(const uint8_t meta[4],
                                       const uint8_t wrap[4],
                                       const uint8_t head[2]);
    bool begin();
    bool readCalendar(tm &out, bool &valid);
    bool readStatus(uint8_t &status);
    bool readTime(tm &out, bool &osc_stop, bool &valid);
    bool writeCalendar(const tm &utc_tm);
    bool writeStatus(uint8_t status);
    bool writeTime(const tm &utc_tm);
    bool clearOscillatorStop();
    bool isBatteryLow(bool &low);
    static const char *label() { return "DS3231"; }

private:
    static uint8_t bcd2bin(uint8_t val);
    static uint8_t bin2bcd(uint8_t val);
    bool read(uint8_t reg, uint8_t *buf, size_t len);
    bool write(uint8_t reg, const uint8_t *buf, size_t len);
};
