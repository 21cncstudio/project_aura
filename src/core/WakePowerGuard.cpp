// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/WakePowerGuard.h"

#include <atomic>

namespace WakePowerGuard {
namespace {

constexpr uint32_t kPreQuietFailsafeMs = 2000U;

std::atomic<Phase> g_phase{Phase::Idle};
std::atomic<uint32_t> g_phase_started_ms{0U};
std::atomic<uint32_t> g_quiet_started_ms{0U};
std::atomic<uint32_t> g_settle_until_ms{0U};
std::atomic<uint32_t> g_active_operations{0U};
std::atomic<bool> g_waiting_for_drain{false};

bool deadlinePending(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) < 0;
}

void refresh(uint32_t now_ms) {
    const Phase current = g_phase.load(std::memory_order_acquire);
    if (current == Phase::Settle) {
        // Settle completion is owner-advanced by beginRenderWait(). A reader
        // such as the LVGL task must never publish RenderWait on its own.
        return;
    }

    if (current == Phase::PreQuiet) {
        const uint32_t started = g_phase_started_ms.load(std::memory_order_acquire);
        if (static_cast<uint32_t>(now_ms - started) >= kPreQuietFailsafeMs) {
            Phase expected = Phase::PreQuiet;
            // Publish Idle last. Auxiliary state is intentionally left alone:
            // request() overwrites it before the next PreQuiet transition. A
            // cleanup after the CAS could otherwise erase a new request won by
            // the other core in the small Idle hand-off window.
            (void)g_phase.compare_exchange_strong(expected,
                                                  Phase::Idle,
                                                  std::memory_order_acq_rel);
        }
    }
}

void releaseActivity() {
    g_active_operations.fetch_sub(1U, std::memory_order_release);
}

} // namespace

Activity::Activity(Activity &&other) noexcept : acquired_(other.acquired_) {
    other.acquired_ = false;
}

Activity &Activity::operator=(Activity &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    acquired_ = other.acquired_;
    other.acquired_ = false;
    return *this;
}

Activity::~Activity() {
    release();
}

void Activity::release() {
    if (!acquired_) {
        return;
    }
    releaseActivity();
    acquired_ = false;
}

bool request(uint32_t now_ms) {
    refresh(now_ms);
    const Phase current = g_phase.load(std::memory_order_acquire);
    if (current == Phase::PreQuiet) {
        return true;
    }
    if (current != Phase::Idle) {
        return false;
    }
    g_phase_started_ms.store(now_ms, std::memory_order_release);
    g_quiet_started_ms.store(now_ms, std::memory_order_release);
    Phase expected = Phase::Idle;
    if (!g_phase.compare_exchange_strong(expected,
                                         Phase::PreQuiet,
                                         std::memory_order_acq_rel)) {
        return expected == Phase::PreQuiet;
    }
    // Always establish the quiet baseline from the owner's first evaluation.
    // An already-admitted Activity may drain after the Idle -> PreQuiet CAS but
    // before we can sample g_active_operations. Sampling zero here would lose
    // that overlap and could count part of the Activity as quiet time.
    g_waiting_for_drain.store(true, std::memory_order_release);
    g_settle_until_ms.store(0U, std::memory_order_release);
    return true;
}

SwitchDecision evaluateSwitch(uint32_t now_ms,
                              uint32_t min_quiet_ms,
                              uint32_t wait_warning_ms) {
    refresh(now_ms);
    SwitchDecision decision{};
    if (g_phase.load(std::memory_order_acquire) != Phase::PreQuiet) {
        return decision;
    }
    decision.elapsed_ms = static_cast<uint32_t>(
        now_ms - g_phase_started_ms.load(std::memory_order_acquire));
    decision.active_operations =
        g_active_operations.load(std::memory_order_acquire);
    decision.wait_exceeded = decision.active_operations != 0U &&
                             decision.elapsed_ms >= wait_warning_ms;
    if (decision.active_operations != 0U) {
        // The quiet interval begins only after the last tracked operation has
        // drained. Merely waiting min_quiet_ms since the original request is
        // insufficient when HTTP, MQTT, Hub or Wi-Fi work was still active.
        g_waiting_for_drain.store(true, std::memory_order_release);
        g_quiet_started_ms.store(now_ms, std::memory_order_release);
        return decision;
    }
    if (g_waiting_for_drain.exchange(false, std::memory_order_acq_rel)) {
        g_quiet_started_ms.store(now_ms, std::memory_order_release);
    }
    const uint32_t quiet_elapsed_ms = static_cast<uint32_t>(
        now_ms - g_quiet_started_ms.load(std::memory_order_acquire));
    decision.ready = quiet_elapsed_ms >= min_quiet_ms;
    return decision;
}

bool readyToSwitch(uint32_t now_ms,
                   uint32_t min_quiet_ms,
                   uint32_t wait_warning_ms) {
    return evaluateSwitch(now_ms, min_quiet_ms, wait_warning_ms).ready;
}

bool beginSwitch() {
    if (g_active_operations.load(std::memory_order_acquire) != 0U) {
        return false;
    }
    Phase expected = Phase::PreQuiet;
    const bool switched = g_phase.compare_exchange_strong(
        expected, Phase::Switching, std::memory_order_acq_rel);
    if (switched) {
        g_waiting_for_drain.store(false, std::memory_order_release);
    }
    return switched;
}

void beginSettle(uint32_t now_ms, uint32_t settle_ms) {
    g_phase_started_ms.store(now_ms, std::memory_order_release);
    g_settle_until_ms.store(now_ms + settle_ms, std::memory_order_release);
    g_phase.store(Phase::Settle, std::memory_order_release);
}

bool settleReady(uint32_t now_ms) {
    if (g_phase.load(std::memory_order_acquire) != Phase::Settle) {
        return false;
    }
    const uint32_t deadline = g_settle_until_ms.load(std::memory_order_acquire);
    return !deadlinePending(now_ms, deadline);
}

bool beginRenderWait(uint32_t now_ms) {
    const Phase current = g_phase.load(std::memory_order_acquire);
    if (current == Phase::RenderWait) {
        return true;
    }
    if (current != Phase::Settle || !settleReady(now_ms)) {
        return false;
    }
    Phase expected = Phase::Settle;
    return g_phase.compare_exchange_strong(expected,
                                           Phase::RenderWait,
                                           std::memory_order_acq_rel);
}

bool completeRenderWait() {
    Phase expected = Phase::RenderWait;
    return g_phase.compare_exchange_strong(expected,
                                           Phase::Idle,
                                           std::memory_order_acq_rel);
}

void latchFailClosed() {
    Phase current = g_phase.load(std::memory_order_acquire);
    while (current == Phase::Idle || current == Phase::PreQuiet) {
        if (g_phase.compare_exchange_weak(current,
                                          Phase::FailClosed,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
            return;
        }
    }
}

void cancel() {
    g_settle_until_ms.store(0U, std::memory_order_release);
    g_phase_started_ms.store(0U, std::memory_order_release);
    g_quiet_started_ms.store(0U, std::memory_order_release);
    g_waiting_for_drain.store(false, std::memory_order_release);
    g_phase.store(Phase::Idle, std::memory_order_release);
}

bool backgroundPaused(uint32_t now_ms) {
    return phase(now_ms) != Phase::Idle;
}

bool uiPaused(uint32_t now_ms) {
    const Phase current = phase(now_ms);
    return current != Phase::Idle && current != Phase::RenderWait;
}

Phase phase(uint32_t now_ms) {
    refresh(now_ms);
    return g_phase.load(std::memory_order_acquire);
}

uint32_t activeOperations() {
    return g_active_operations.load(std::memory_order_acquire);
}

Activity tryAcquireActivity(uint32_t now_ms) {
    refresh(now_ms);
    if (g_phase.load(std::memory_order_acquire) != Phase::Idle) {
        return Activity{};
    }

    g_active_operations.fetch_add(1U, std::memory_order_acq_rel);
    if (g_phase.load(std::memory_order_acquire) != Phase::Idle) {
        releaseActivity();
        return Activity{};
    }
    return Activity{true};
}

#ifdef UNIT_TEST
void resetForTest() {
    g_active_operations.store(0U, std::memory_order_release);
    cancel();
}
#endif

} // namespace WakePowerGuard
