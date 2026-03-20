// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDacUtils.h"

#include "config/AppConfig.h"

namespace WebDacUtils {

bool normalizeTimerSeconds(int32_t raw_seconds, uint32_t &out_seconds) {
    if (raw_seconds < 0) {
        return false;
    }
    const uint32_t seconds = static_cast<uint32_t>(raw_seconds);
    if (!Config::isDacTimerPresetSeconds(seconds)) {
        return false;
    }
    out_seconds = seconds;
    return true;
}

uint8_t outputPercent(uint16_t output_mv) {
    if (Config::DAC_VOUT_FULL_SCALE_MV == 0) {
        return 0;
    }
    uint32_t percent = static_cast<uint32_t>(output_mv) * 100u;
    percent = (percent + (Config::DAC_VOUT_FULL_SCALE_MV / 2u)) / Config::DAC_VOUT_FULL_SCALE_MV;
    if (percent > 100u) {
        percent = 100u;
    }
    return static_cast<uint8_t>(percent);
}

uint32_t remainingSeconds(bool running, uint32_t stop_at_ms, uint32_t now_ms) {
    if (!running || stop_at_ms == 0 || static_cast<int32_t>(now_ms - stop_at_ms) >= 0) {
        return 0;
    }
    return (stop_at_ms - now_ms + 999UL) / 1000UL;
}

const char *statusText(bool available, bool faulted, bool running) {
    if (faulted) {
        return "FAULT";
    }
    if (!available) {
        return "OFFLINE";
    }
    return running ? "RUNNING" : "STOPPED";
}

} // namespace WebDacUtils
