// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Gt911DiagnosticPolicy {

enum class Severity { Info, Warning };
enum class ProbeRole { Configured, Opposite };
enum class ReadResult { Ok, GenericFailure, Timeout, OtherFailure };

constexpr Severity selectionSeverity(bool succeeded) {
    return succeeded ? Severity::Info : Severity::Warning;
}

constexpr bool configuredAddressHealthy(bool identity_valid, ReadResult config_result) {
    return identity_valid && config_result == ReadResult::Ok;
}

constexpr Severity identitySeverity(ProbeRole role,
                                    ReadResult result,
                                    bool identity_valid,
                                    bool configured_healthy) {
    if (role == ProbeRole::Configured) {
        return result == ReadResult::Ok && identity_valid
            ? Severity::Info : Severity::Warning;
    }
    // ESP_FAIL on the other address is expected only after the selected
    // address passed both reads. It is a generic API failure, not a measured NACK.
    if (configured_healthy && result == ReadResult::GenericFailure) {
        return Severity::Info;
    }
    // A second responder, timeout, or unknown failure still needs review.
    return Severity::Warning;
}

constexpr Severity configSeverity(ProbeRole role, ReadResult result) {
    return role == ProbeRole::Configured && result == ReadResult::Ok
        ? Severity::Info : Severity::Warning;
}

constexpr const char *severityText(Severity severity) {
    return severity == Severity::Info ? "info" : "warning";
}

constexpr const char *probeRoleText(ProbeRole role) {
    return role == ProbeRole::Configured ? "configured" : "opposite";
}

constexpr const char *readResultText(ReadResult result) {
    switch (result) {
        case ReadResult::Ok: return "ok";
        case ReadResult::GenericFailure: return "generic_failure";
        case ReadResult::Timeout: return "timeout";
        case ReadResult::OtherFailure: return "other_failure";
        default: return "other_failure";
    }
}

} // namespace Gt911DiagnosticPolicy
