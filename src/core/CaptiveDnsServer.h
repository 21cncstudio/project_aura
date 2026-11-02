// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// Owned exclusively by NetworkManager. Runtime calls must hold its existing
// WakePowerGuard activity lease; this object creates no task or async callback.
class CaptiveDnsServer {
public:
    CaptiveDnsServer() = default;
    ~CaptiveDnsServer() { stop(); }
    CaptiveDnsServer(const CaptiveDnsServer &) = delete;
    CaptiveDnsServer &operator=(const CaptiveDnsServer &) = delete;
    bool start(uint32_t address_network_order);
    void stop();
    void poll();
    bool running() const { return socket_ >= 0; }
private:
    int socket_ = -1;
    uint32_t address_ = 0;
    uint8_t packet_[513]{}; // Extra byte distinguishes oversized UDP datagrams.
};
