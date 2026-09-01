// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace TouchReleaseGatePolicy {

enum class ProbeResult { Released, Pressed, Error };

constexpr ProbeResult classify(int read_result) {
    return read_result < 0 ? ProbeResult::Error
         : read_result > 0 ? ProbeResult::Pressed
                           : ProbeResult::Released;
}

// A transport error is not evidence that the finger was released.
constexpr bool keepWaiting(ProbeResult result) {
    return result != ProbeResult::Released;
}

// Both a press and a release prove that the transport is healthy. Only an
// actual read error is allowed to extend the consecutive-error streak.
constexpr bool readSucceeded(ProbeResult result) {
    return result != ProbeResult::Error;
}

} // namespace TouchReleaseGatePolicy
