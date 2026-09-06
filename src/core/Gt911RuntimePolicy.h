// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace Gt911RuntimePolicy {

constexpr uint16_t MODULE_SWITCH_1_REGISTER = 0x804D;
constexpr uint16_t COORDINATE_STATUS_REGISTER = 0x814E;
constexpr uint8_t DATA_READY_MASK = 0x80;
constexpr uint8_t POINT_COUNT_MASK = 0x0F;
constexpr uint8_t MAX_POINT_COUNT = 5;

enum class InterruptMode : uint8_t {
    RisingEdge = 0,
    FallingEdge = 1,
    LowLevel = 2,
    HighLevel = 3,
};

struct InterruptConfig {
    InterruptMode mode = InterruptMode::FallingEdge;
    bool active_high = false;
    bool positive_edge = false;
    bool direct_irq_supported = true;
};

constexpr InterruptConfig decodeInterruptConfig(uint8_t module_switch_1) {
    const InterruptMode mode =
        static_cast<InterruptMode>(module_switch_1 & 0x03U);
    const bool active_high =
        mode == InterruptMode::RisingEdge || mode == InterruptMode::HighLevel;
    const bool direct_irq_supported =
        mode == InterruptMode::RisingEdge || mode == InterruptMode::FallingEdge;
    return {mode, active_high, active_high, direct_irq_supported};
}

constexpr bool directIrqAvailable(bool config_verified,
                                  int8_t config_mode,
                                  bool fail_safe,
                                  bool isr_registered,
                                  bool handle_present) {
    const bool edge_mode =
        config_mode == static_cast<int8_t>(InterruptMode::RisingEdge) ||
        config_mode == static_cast<int8_t>(InterruptMode::FallingEdge);
    return config_verified && edge_mode && !fail_safe && isr_registered &&
           handle_present;
}

enum class FrameKind : uint8_t {
    NoFrame = 0,
    Released,
    Pressed,
    Malformed,
};

struct FrameStatus {
    FrameKind kind = FrameKind::NoFrame;
    uint8_t point_count = 0;
};

constexpr FrameStatus decodeFrameStatus(uint8_t coordinate_status) {
    if ((coordinate_status & DATA_READY_MASK) == 0) {
        return {FrameKind::NoFrame, 0};
    }

    const uint8_t point_count = coordinate_status & POINT_COUNT_MASK;
    if (point_count == 0) {
        return {FrameKind::Released, 0};
    }
    if (point_count <= MAX_POINT_COUNT) {
        return {FrameKind::Pressed, point_count};
    }
    return {FrameKind::Malformed, point_count};
}

enum class VendorReadAction : uint8_t {
    Skip = 0,
    ReadFrame,
    ReadForCleanup,
};

constexpr VendorReadAction vendorReadAction(FrameStatus status) {
    if (status.kind == FrameKind::NoFrame) {
        return VendorReadAction::Skip;
    }
    if (status.kind == FrameKind::Malformed) {
        return VendorReadAction::ReadForCleanup;
    }
    return VendorReadAction::ReadFrame;
}

constexpr VendorReadAction vendorReadAction(uint8_t coordinate_status) {
    return vendorReadAction(decodeFrameStatus(coordinate_status));
}

enum class ReconciledFrameKind : uint8_t {
    NoData = 0,
    Released,
    Pressed,
    Error,
};

struct FrameReconciliation {
    ReconciledFrameKind kind = ReconciledFrameKind::NoData;
    VendorReadAction vendor_read_action = VendorReadAction::Skip;
    bool preserve_cache_on_error = false;
};

// Reconcile the raw GT911 status with the vendor driver's result. The caller
// first uses vendorReadAction() to decide whether the full read is required,
// then passes its result here. vendor_result is ignored for a non-ready frame.
constexpr FrameReconciliation reconcileFrame(uint8_t coordinate_status,
                                              int vendor_result = 0) {
    const FrameStatus status = decodeFrameStatus(coordinate_status);
    const VendorReadAction read_action = vendorReadAction(status);

    if (status.kind == FrameKind::NoFrame) {
        return {ReconciledFrameKind::NoData, read_action, false};
    }

    // A malformed ready frame still needs a vendor read to perform the normal
    // GT911 ready/status cleanup, but it is never accepted as input.
    if (status.kind == FrameKind::Malformed) {
        return {ReconciledFrameKind::Error, read_action, false};
    }

    if (vendor_result < 0) {
        return {ReconciledFrameKind::Error, read_action, false};
    }

    if (vendor_result > 0) {
        // The frame may advance between the raw status read and the vendor
        // read. A point returned after a raw release is the newer sample.
        return {ReconciledFrameKind::Pressed, read_action, false};
    }

    if (status.kind == FrameKind::Pressed) {
        // The raw status promised points but the vendor read observed none and
        // completed the normal status-clear path. Treat the newer vendor
        // result as release so a previous cached press cannot become stuck.
        return {ReconciledFrameKind::Released, read_action, false};
    }

    return {ReconciledFrameKind::Released, read_action, false};
}

} // namespace Gt911RuntimePolicy
