// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace ScreenOnTouchPolicy {

// LVGL continues to call the input callback every 40 ms. This policy only
// decides whether that callback needs an I2C read from the touch controller.
constexpr uint32_t CALLBACK_INTERVAL_MS = 40U;
constexpr uint32_t FAST_READ_INTERVAL_MS = 40U;
constexpr uint32_t FAST_AFTER_RELEASE_MS = 350U;
constexpr uint32_t CALM_READ_INTERVAL_MS = 80U;
constexpr uint32_t IDLE_AFTER_RELEASE_MS = 1000U;
constexpr uint32_t IDLE_FALLBACK_INTERVAL_MS = 200U;

enum class Mode : uint8_t {
    FastInteraction = 0,
    ReleasedCalm,
    IdleIrq,
    PollingOnly,
};

enum class Action : uint8_t {
    None = 0,
    ReadFast,
    ReadCalm,
    RequestIdleIrq,
    ReadIdleIrq,
    ReadIdleFallback,
    ReadPollingOnly,
};

// NoData means that the controller did not publish a new status packet. It is
// deliberately distinct from an explicit zero-point release packet.
enum class Sample : uint8_t {
    NoData = 0,
    Pressed,
    Released,
    Error,
};

struct Decision {
    Mode mode;
    Action action;
};

struct ReadEffect {
    // The integration owns the GPIO/ISR operation. The pure policy only tells
    // it when an armed idle IRQ is no longer allowed to remain active.
    bool disarm_idle_irq;
    // True only when the 200 ms fallback found a press without a pending IRQ.
    bool missed_idle_irq;

    constexpr ReadEffect(bool disarm = false, bool missed = false)
        : disarm_idle_irq(disarm), missed_idle_irq(missed) {}
};

constexpr bool isReadAction(Action action) {
    return action == Action::ReadFast ||
           action == Action::ReadCalm ||
           action == Action::ReadIdleIrq ||
           action == Action::ReadIdleFallback ||
           action == Action::ReadPollingOnly;
}

// A fallback probe can race the physical edge: the callback may observe no
// pending latch, then the GT911 asserts INT before the status/full read finds
// the press. Treat a post-read latch as the IRQ which selected that press so a
// healthy edge path is not mislabeled as a missed interrupt.
constexpr Action reconcileIdleReadSource(Action scheduled,
                                         Sample sample,
                                         bool irq_observed_after_read) {
    return scheduled == Action::ReadIdleFallback &&
                   sample == Sample::Pressed &&
                   irq_observed_after_read
               ? Action::ReadIdleIrq
               : scheduled;
}

constexpr bool elapsed(uint32_t now_ms,
                       uint32_t start_ms,
                       uint32_t interval_ms) {
    return static_cast<uint32_t>(now_ms - start_ms) >= interval_ms;
}

class State {
public:
    void reset(uint32_t now_ms, bool idle_irq_available) {
        mode_ = idle_irq_available ? Mode::FastInteraction
                                   : Mode::PollingOnly;
        idle_irq_available_ = idle_irq_available;
        release_eligible_ = false;
        first_read_pending_ = true;
        last_read_ms_ = now_ms;
        explicit_release_ms_ = 0U;
        last_idle_probe_ms_ = 0U;
        missed_idle_irq_count_ = 0U;
    }

    // Integration contract:
    // - RequestIdleIrq must first be completed with
    //   recordIdleBoundarySample(), and only an allowed result may proceed to
    //   recordIdleIrqArm().
    // - Every Read* action must be completed with recordRead(), then the
    //   caller must honor ReadEffect::disarm_idle_irq in the same callback.
    Decision decide(uint32_t now_ms, bool idle_irq_pending) {
        updateReleasedMode(now_ms);

        switch (mode_) {
        case Mode::FastInteraction:
            return {mode_, readDue(now_ms, FAST_READ_INTERVAL_MS)
                               ? Action::ReadFast
                               : Action::None};

        case Mode::ReleasedCalm:
            if (release_eligible_ &&
                elapsed(now_ms, explicit_release_ms_, IDLE_AFTER_RELEASE_MS)) {
                return {mode_, Action::RequestIdleIrq};
            }
            return {mode_, readDue(now_ms, CALM_READ_INTERVAL_MS)
                               ? Action::ReadCalm
                               : Action::None};

        case Mode::IdleIrq:
            if (idle_irq_pending) {
                return {mode_, Action::ReadIdleIrq};
            }
            return {mode_, elapsed(now_ms,
                                   last_idle_probe_ms_,
                                   IDLE_FALLBACK_INTERVAL_MS)
                               ? Action::ReadIdleFallback
                               : Action::None};

        case Mode::PollingOnly:
            return {mode_, readDue(now_ms, FAST_READ_INTERVAL_MS)
                               ? Action::ReadPollingOnly
                               : Action::None};
        }

        return {Mode::PollingOnly, Action::ReadPollingOnly};
    }

    void recordIdleIrqArm(bool succeeded, uint32_t now_ms) {
        if (mode_ != Mode::ReleasedCalm) {
            return;
        }
        if (!succeeded || !idle_irq_available_) {
            usePollingOnly();
            return;
        }

        mode_ = Mode::IdleIrq;
        last_idle_probe_ms_ = now_ms;
    }

    // Close the interval between the last calm poll and IRQ arming. A press or
    // error returns to fast polling; NoData or a repeated explicit release may
    // proceed to the bounded arm sequence.
    bool recordIdleBoundarySample(Sample sample, uint32_t now_ms) {
        if (mode_ != Mode::ReleasedCalm || !release_eligible_) {
            return false;
        }
        (void)recordRead(Action::ReadCalm, sample, now_ms);
        return mode_ == Mode::ReleasedCalm && release_eligible_ &&
               (sample == Sample::NoData || sample == Sample::Released);
    }

    ReadEffect recordRead(Action source, Sample sample, uint32_t now_ms) {
        if (!isReadAction(source)) {
            return {};
        }

        first_read_pending_ = false;
        last_read_ms_ = now_ms;

        const bool idle_fallback_read = source == Action::ReadIdleFallback;
        if (source == Action::ReadIdleIrq || idle_fallback_read) {
            last_idle_probe_ms_ = now_ms;
            return recordIdleRead(source == Action::ReadIdleIrq,
                                  idle_fallback_read,
                                  sample);
        }

        if (mode_ == Mode::PollingOnly) {
            return {};
        }

        recordPollingSample(sample, now_ms);
        return {};
    }

    void usePollingOnly() {
        mode_ = Mode::PollingOnly;
        idle_irq_available_ = false;
        release_eligible_ = false;
    }

    Mode mode() const { return mode_; }
    bool releaseEligible() const { return release_eligible_; }
    uint32_t explicitReleaseMs() const { return explicit_release_ms_; }
    uint32_t missedIdleIrqCount() const { return missed_idle_irq_count_; }

private:
    bool readDue(uint32_t now_ms, uint32_t interval_ms) const {
        return first_read_pending_ || elapsed(now_ms, last_read_ms_, interval_ms);
    }

    void updateReleasedMode(uint32_t now_ms) {
        if (mode_ == Mode::FastInteraction &&
            release_eligible_ &&
            elapsed(now_ms, explicit_release_ms_, FAST_AFTER_RELEASE_MS)) {
            mode_ = Mode::ReleasedCalm;
        }
    }

    void resetIdleEligibility() {
        release_eligible_ = false;
        explicit_release_ms_ = 0U;
    }

    void recordPollingSample(Sample sample, uint32_t now_ms) {
        if (sample == Sample::Pressed || sample == Sample::Error) {
            mode_ = Mode::FastInteraction;
            resetIdleEligibility();
            return;
        }

        if (sample == Sample::Released && !release_eligible_) {
            mode_ = Mode::FastInteraction;
            release_eligible_ = true;
            explicit_release_ms_ = now_ms;
        }
        // NoData is not release evidence. A repeated explicit release also
        // must not move the original interaction deadline forward forever.
    }

    ReadEffect recordIdleRead(bool irq_read,
                              bool fallback_read,
                              Sample sample) {
        if (irq_read) {
            // The latch which selected this read has already been consumed.
            // Even an early/false edge must leave IRQ mode and wait for a new
            // explicit release before becoming eligible to arm again.
            mode_ = Mode::FastInteraction;
            resetIdleEligibility();
            return {true, false};
        }

        if (sample == Sample::Error) {
            mode_ = Mode::FastInteraction;
            resetIdleEligibility();
            return {true, false};
        }

        if (sample == Sample::Pressed) {
            resetIdleEligibility();
            if (fallback_read) {
                if (missed_idle_irq_count_ != UINT32_MAX) {
                    ++missed_idle_irq_count_;
                }
                usePollingOnly();
                return {true, true};
            }

            mode_ = Mode::FastInteraction;
            return {true, false};
        }

        // A successful fallback with NoData or Released confirms that the
        // controller remains idle. Keep its existing release qualification.
        return {};
    }

    Mode mode_ = Mode::FastInteraction;
    bool idle_irq_available_ = false;
    bool release_eligible_ = false;
    bool first_read_pending_ = true;
    uint32_t last_read_ms_ = 0U;
    uint32_t explicit_release_ms_ = 0U;
    uint32_t last_idle_probe_ms_ = 0U;
    uint32_t missed_idle_irq_count_ = 0U;
};

} // namespace ScreenOnTouchPolicy
