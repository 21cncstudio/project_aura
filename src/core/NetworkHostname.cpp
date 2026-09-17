// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/NetworkHostname.h"

#include <cstdio>
#include <cstring>

namespace {
void increment(uint32_t &counter) {
    if (counter != UINT32_MAX) {
        ++counter;
    }
}
} // namespace

bool NetworkHostname::apply(const char *expected, void *context,
                            Setter set, Getter get, uint32_t now_ms) {
    const int32_t set_error = set(context, expected);
    observe(expected, context, get, now_ms);
    snapshot_.apply_attempted = true;
    snapshot_.apply_error = set_error != 0 ? set_error
        : snapshot_.read_error != 0 ? snapshot_.read_error
        : snapshot_.matches ? 0 : kVerificationFailed;
    if (snapshot_.apply_error != 0) {
        increment(snapshot_.apply_failures);
        snapshot_.last_failure_error = snapshot_.apply_error;
        return false;
    }
    return true;
}

void NetworkHostname::observe(const char *expected, void *context,
                              Getter get, uint32_t now_ms) {
    const char *actual = nullptr;
    const int32_t error = get(context, &actual);
    const bool available = error == 0 && actual != nullptr && actual[0] != '\0';
    const bool matches = available && std::strcmp(expected, actual) == 0;
    // Count a new mismatching name/episode, not every periodic observation.
    if (available && !matches &&
        (!snapshot_.available || snapshot_.matches ||
         std::strcmp(snapshot_.actual, actual) != 0)) {
        increment(snapshot_.mismatch_count);
    }
    std::snprintf(snapshot_.actual, sizeof(snapshot_.actual), "%s", available ? actual : "");
    snapshot_.available = available;
    snapshot_.matches = matches;
    snapshot_.read_error = error;
    snapshot_.checked = true;
    snapshot_.checked_at_ms = now_ms;
}

void NetworkHostname::clearLive() {
    snapshot_.actual[0] = '\0';
    snapshot_.available = false;
    snapshot_.matches = false;
    snapshot_.checked = false;
    snapshot_.checked_at_ms = 0;
    snapshot_.read_error = 0;
    // Keep apply results and failure history across interface recreation.
}
