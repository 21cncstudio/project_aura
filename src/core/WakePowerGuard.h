// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace WakePowerGuard {

enum class Phase : uint8_t {
    Idle = 0,
    PreQuiet,
    Settle,
};

struct SwitchDecision {
    bool ready = false;
    bool forced_by_timeout = false;
    uint32_t elapsed_ms = 0;
    uint32_t active_operations = 0;
};

class Activity {
public:
    Activity() = default;
    Activity(const Activity &) = delete;
    Activity &operator=(const Activity &) = delete;
    Activity(Activity &&other) noexcept;
    Activity &operator=(Activity &&other) noexcept;
    ~Activity();

    explicit operator bool() const { return acquired_; }

private:
    friend Activity tryAcquireActivity(uint32_t now_ms);

    explicit Activity(bool acquired) : acquired_(acquired) {}
    void release();

    bool acquired_ = false;
};

// Starts a cross-core quiet window. Only the UI/backlight owner transitions
// PreQuiet to Settle; readers merely stop admitting new background activity.
bool request(uint32_t now_ms);

// A normal wake waits for both a short quiet interval and all tracked work to
// drain. The maximum bound keeps wake latency deterministic if an operation is
// already blocked in a transport owned outside Project Aura.
bool readyToSwitch(uint32_t now_ms,
                   uint32_t min_quiet_ms,
                   uint32_t max_wait_ms);
SwitchDecision evaluateSwitch(uint32_t now_ms,
                              uint32_t min_quiet_ms,
                              uint32_t max_wait_ms);

void beginSettle(uint32_t now_ms, uint32_t settle_ms);
void cancel();

bool backgroundPaused(uint32_t now_ms);
bool uiPaused(uint32_t now_ms);
Phase phase(uint32_t now_ms);
uint32_t activeOperations();

// Background tasks must hold this lease across the operation which should not
// overlap a backlight wake. Admission closes atomically when request() wins.
Activity tryAcquireActivity(uint32_t now_ms);

#ifdef UNIT_TEST
void resetForTest();
#endif

} // namespace WakePowerGuard
