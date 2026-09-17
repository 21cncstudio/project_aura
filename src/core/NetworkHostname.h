// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

struct NetworkHostnameSnapshot {
    char actual[33]{};
    bool checked = false;
    bool available = false;
    bool matches = false;
    bool apply_attempted = false;
    int32_t apply_error = 0;
    int32_t read_error = 0;
    int32_t last_failure_error = 0;
    uint32_t apply_failures = 0;
    uint32_t mismatch_count = 0;
    uint32_t checked_at_ms = 0;
};

// Owned by the network task. Callbacks adapt the live netif or a test double;
// snapshots contain copies, never pointers into the network stack.
class NetworkHostname {
public:
    using Setter = int32_t (*)(void *, const char *);
    using Getter = int32_t (*)(void *, const char **);
    static constexpr int32_t kVerificationFailed = -1;

    bool apply(const char *expected, void *context, Setter set, Getter get, uint32_t now_ms);
    void observe(const char *expected, void *context, Getter get, uint32_t now_ms);
    void clearLive();
    const NetworkHostnameSnapshot &snapshot() const { return snapshot_; }

private:
    NetworkHostnameSnapshot snapshot_{};
};
