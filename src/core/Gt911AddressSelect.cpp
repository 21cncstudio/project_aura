// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Gt911AddressSelect.h"

namespace Gt911AddressSelect {
namespace {

void bestEffortRelease(const Ops &ops, bool reset_asserted) {
    if (reset_asserted) {
        (void)ops.set_reset_level(ops.context, true);
    }
    (void)ops.release_int(ops.context);
}

} // namespace

Result selectBackupAddress(const Ops &ops) {
    if (ops.set_int_output == nullptr || ops.set_int_level == nullptr ||
        ops.set_reset_level == nullptr || ops.release_int == nullptr ||
        ops.delay_ms == nullptr) {
        return {Failure::InvalidOps};
    }

    if (!ops.set_int_output(ops.context)) {
        return {Failure::IntOutput};
    }
    if (!ops.set_int_level(ops.context, false)) {
        bestEffortRelease(ops, false);
        return {Failure::IntLow};
    }
    ops.delay_ms(ops.context, 50U);

    if (!ops.set_reset_level(ops.context, false)) {
        bestEffortRelease(ops, false);
        return {Failure::ResetLow};
    }
    ops.delay_ms(ops.context, 50U);

    if (!ops.set_int_level(ops.context, true)) {
        bestEffortRelease(ops, true);
        return {Failure::IntHigh};
    }
    ops.delay_ms(ops.context, 5U);

    if (!ops.set_reset_level(ops.context, true)) {
        bestEffortRelease(ops, true);
        return {Failure::ResetHigh};
    }
    ops.delay_ms(ops.context, 350U);
    ops.delay_ms(ops.context, 150U);

    if (!ops.release_int(ops.context)) {
        return {Failure::IntRelease};
    }
    return {};
}

const char *failureText(Failure failure) {
    switch (failure) {
        case Failure::None: return "none";
        case Failure::InvalidOps: return "invalid_ops";
        case Failure::IntOutput: return "int_output";
        case Failure::IntLow: return "int_low";
        case Failure::ResetLow: return "reset_low";
        case Failure::IntHigh: return "int_high";
        case Failure::ResetHigh: return "reset_high";
        case Failure::IntRelease: return "int_release";
        default: return "unknown";
    }
}

} // namespace Gt911AddressSelect
