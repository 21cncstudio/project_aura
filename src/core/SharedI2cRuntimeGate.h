// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

namespace SharedI2cRuntimeGate {

// One-way for the lifetime of a running firmware instance. The only reset is
// an explicit boot/manager initialization boundary.
class Gate {
public:
    class Access {
    public:
        Access() = default;
        Access(const Access &) = delete;
        Access &operator=(const Access &) = delete;

        Access(Access &&other) noexcept : gate_(other.gate_) {
            other.gate_ = nullptr;
        }

        Access &operator=(Access &&other) noexcept {
            if (this != &other) {
                release();
                gate_ = other.gate_;
                other.gate_ = nullptr;
            }
            return *this;
        }

        ~Access() { release(); }

        explicit operator bool() const { return gate_ != nullptr; }

    private:
        friend class Gate;
        explicit Access(Gate *gate) : gate_(gate) {}

        void release() {
            if (gate_ != nullptr) {
                gate_->release();
                gate_ = nullptr;
            }
        }

        Gate *gate_ = nullptr;
    };

    bool available() const {
        return (state_.load(std::memory_order_acquire) & kDisabledBit) == 0;
    }

    Access acquire() {
        uint32_t current = state_.load(std::memory_order_acquire);
        for (;;) {
            if ((current & kDisabledBit) != 0 ||
                (current & kActiveMask) == kActiveMask) {
                return Access{};
            }
            if (state_.compare_exchange_weak(
                    current,
                    current + 1U,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return Access(this);
            }
        }
    }

    bool disable() {
        const uint32_t previous =
            state_.fetch_or(kDisabledBit, std::memory_order_acq_rel);
        return (previous & kDisabledBit) == 0;
    }

    bool idle() const {
        return activeCount() == 0;
    }

    uint32_t activeCount() const {
        return state_.load(std::memory_order_acquire) & kActiveMask;
    }

    bool resetForBoot() {
        uint32_t current = state_.load(std::memory_order_acquire);
        for (;;) {
            if ((current & kActiveMask) != 0) {
                return false;
            }
            if ((current & kDisabledBit) == 0) {
                return true;
            }
            if (state_.compare_exchange_weak(
                    current,
                    0,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
    }

private:
    static constexpr uint32_t kDisabledBit = 0x80000000UL;
    static constexpr uint32_t kActiveMask = ~kDisabledBit;

    void release() {
        state_.fetch_sub(1U, std::memory_order_acq_rel);
    }

    std::atomic<uint32_t> state_{0};
};

} // namespace SharedI2cRuntimeGate
