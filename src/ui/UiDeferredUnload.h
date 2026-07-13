// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>

class UiDeferredUnload {
public:
    static constexpr size_t kCount = 11;

    void reset();
    void scheduleOnSwitch(int previous_screen_id, int current_screen_id, uint32_t now_ms);
    bool ready(size_t index, uint32_t now_ms, int pending_screen_id, int current_screen_id) const;
    int screenId(size_t index) const;
    void clear(size_t index);
    void retry(size_t index, uint32_t now_ms);
    size_t count() const { return kCount; }

private:
    static constexpr uint32_t kDelayMs = 300;
    static constexpr uint32_t kRetryMs = 100;

    uint32_t unload_at_ms_[kCount] = {};
    bool scheduled_[kCount] = {};
    static const int kScreenIds[kCount];
};

