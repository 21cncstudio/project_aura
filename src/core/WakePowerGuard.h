// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace WakePowerGuard {

enum class Phase : uint8_t {
    Idle = 0,
    PreQuiet,
    Switching,
    Settle,
    RenderWait,
    FailClosed,
};

struct SwitchDecision {
    bool ready = false;
    bool wait_exceeded = false;
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

// Starts a cross-core quiet window. Only the UI/backlight owner advances the
// guarded phases; readers merely stop admitting new background activity.
bool request(uint32_t now_ms);

// A wake waits for both a short quiet interval and all tracked work to drain.
// wait_warning_ms is diagnostic only: exceeding it never permits a switch
// while tracked work is still active.
bool readyToSwitch(uint32_t now_ms,
                   uint32_t min_quiet_ms,
                   uint32_t wait_warning_ms);
SwitchDecision evaluateSwitch(uint32_t now_ms,
                              uint32_t min_quiet_ms,
                              uint32_t wait_warning_ms);

// Claims the non-expiring hardware-switch phase after evaluateSwitch() reports
// ready. Background admission stays closed through settle and the first
// post-wake render, until completeRenderWait() or cancel().
bool beginSwitch();
void beginSettle(uint32_t now_ms, uint32_t settle_ms);
bool settleReady(uint32_t now_ms);
// Only the UI/backlight owner may publish RenderWait after the settle deadline.
// This prevents the LVGL task from rendering before the owner records the
// command result and arms its completion baseline under the LVGL mutex.
bool beginRenderWait(uint32_t now_ms);
bool completeRenderWait();
// Permanently close admission for an unrecoverable owner failure. Existing
// Switching/Settle/RenderWait phases are preserved; Idle or PreQuiet becomes
// FailClosed until an explicit cancel after safe terminalization.
void latchFailClosed();
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
