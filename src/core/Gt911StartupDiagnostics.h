// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Gt911AddressSelect.h"
#include "core/Gt911DiagnosticPolicy.h"

namespace Gt911StartupDiagnostics {

constexpr uint16_t kProductIdRegister = 0x8140;
constexpr uint16_t kConfigVersionRegister = 0x8047;
constexpr size_t kProbeCount = 2;

struct Probe {
    Gt911DiagnosticPolicy::ProbeRole role =
        Gt911DiagnosticPolicy::ProbeRole::Configured;
    uint8_t address = 0;
    int port = -1;

    bool identity_attempted = false;
    int32_t identity_error = 0;
    Gt911DiagnosticPolicy::ReadResult identity_result =
        Gt911DiagnosticPolicy::ReadResult::OtherFailure;
    uint8_t product_id[3] = {};
    bool identity_valid = false;
    Gt911DiagnosticPolicy::Severity identity_severity =
        Gt911DiagnosticPolicy::Severity::Warning;

    bool config_attempted = false;
    int32_t config_error = 0;
    Gt911DiagnosticPolicy::ReadResult config_result =
        Gt911DiagnosticPolicy::ReadResult::OtherFailure;
    uint8_t config_version = 0;
    Gt911DiagnosticPolicy::Severity config_severity =
        Gt911DiagnosticPolicy::Severity::Warning;
};

// A fixed-size snapshot of the reads already performed during GT911 startup.
// It deliberately does not own or trigger any bus operation.
struct Snapshot {
    bool captured = false;
    uint8_t requested_address = 0;
    int int_gpio = -1;
    bool int_level_high = false;
    int reset_exio = -1;
    bool pin_levels_measured = false;
    bool selection_succeeded = false;
    Gt911AddressSelect::Failure selection_failure =
        Gt911AddressSelect::Failure::None;
    Gt911DiagnosticPolicy::Severity selection_severity =
        Gt911DiagnosticPolicy::Severity::Warning;
    Probe probes[kProbeCount]{};
    size_t probe_count = 0;
};

} // namespace Gt911StartupDiagnostics
