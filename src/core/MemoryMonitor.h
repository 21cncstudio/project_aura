// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>

class MemoryMonitor {
public:
    void begin(uint32_t interval_ms);
    void poll(uint32_t now_ms);
    void logNow(const char *reason);

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

private:
    uint32_t interval_ms_ = 0;
    uint32_t last_log_ms_ = 0;
    bool enabled_ = true;
};
