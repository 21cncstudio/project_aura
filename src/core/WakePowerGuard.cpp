// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/WakePowerGuard.h"

#include <atomic>

namespace WakePowerGuard {
namespace {

constexpr uint32_t kPreQuietFailsafeMs = 2000U;

std::atomic<Phase> g_phase{Phase::Idle};
std::atomic<uint32_t> g_phase_started_ms{0U};
std::atomic<uint32_t> g_settle_until_ms{0U};
std::atomic<uint32_t> g_active_operations{0U};

bool deadlinePending(uint32_t now_ms, uint32_t deadline_ms) {
    return deadline_ms != 0U &&
           static_cast<int32_t>(now_ms - deadline_ms) < 0;
}

void refresh(uint32_t now_ms) {
    const Phase current = g_phase.load(std::memory_order_acquire);
    if (current == Phase::Settle) {
        const uint32_t deadline = g_settle_until_ms.load(std::memory_order_acquire);
        if (!deadlinePending(now_ms, deadline)) {
            Phase expected = Phase::Settle;
            if (g_phase.compare_exchange_strong(expected,
                                                Phase::Idle,
                                                std::memory_order_acq_rel)) {
                g_phase_started_ms.store(0U, std::memory_order_release);
                g_settle_until_ms.store(0U, std::memory_order_release);
            }
        }
        return;
    }

    if (current == Phase::PreQuiet) {
        const uint32_t started = g_phase_started_ms.load(std::memory_order_acquire);
        if (static_cast<uint32_t>(now_ms - started) >= kPreQuietFailsafeMs) {
            Phase expected = Phase::PreQuiet;
            if (g_phase.compare_exchange_strong(expected,
                                                Phase::Idle,
                                                std::memory_order_acq_rel)) {
                g_phase_started_ms.store(0U, std::memory_order_release);
            }
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
    g_phase_started_ms.store(now_ms, std::memory_order_release);
    Phase expected = Phase::Idle;
    if (!g_phase.compare_exchange_strong(expected,
                                         Phase::PreQuiet,
                                         std::memory_order_acq_rel)) {
        return expected == Phase::PreQuiet;
    }
    g_settle_until_ms.store(0U, std::memory_order_release);
    return true;
}

bool readyToSwitch(uint32_t now_ms,
                   uint32_t min_quiet_ms,
                   uint32_t max_wait_ms) {
    refresh(now_ms);
    if (g_phase.load(std::memory_order_acquire) != Phase::PreQuiet) {
        return false;
    }
    const uint32_t elapsed = static_cast<uint32_t>(
        now_ms - g_phase_started_ms.load(std::memory_order_acquire));
    if (elapsed >= max_wait_ms) {
        return true;
    }
    return elapsed >= min_quiet_ms &&
           g_active_operations.load(std::memory_order_acquire) == 0U;
}

void beginSettle(uint32_t now_ms, uint32_t settle_ms) {
    g_phase_started_ms.store(now_ms, std::memory_order_release);
    g_settle_until_ms.store(now_ms + settle_ms, std::memory_order_release);
    g_phase.store(settle_ms == 0U ? Phase::Idle : Phase::Settle,
                  std::memory_order_release);
}

void cancel() {
    g_settle_until_ms.store(0U, std::memory_order_release);
    g_phase_started_ms.store(0U, std::memory_order_release);
    g_phase.store(Phase::Idle, std::memory_order_release);
}

bool backgroundPaused(uint32_t now_ms) {
    return phase(now_ms) != Phase::Idle;
}

bool uiPaused(uint32_t now_ms) {
    return backgroundPaused(now_ms);
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
