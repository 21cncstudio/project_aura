// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Network-owner command state. Radio reconfiguration waits for scan completion;
// the latest requested destination wins, but a credential erase is never lost.
class WifiScanTransition {
public:
    enum class Start { None, Sta, Ap };
    struct Request {
        bool shutdown = false;
        bool erase_credentials = false;
        Start start = Start::None;
        explicit operator bool() const { return shutdown || start != Start::None; }
    };
    bool pending() const { return static_cast<bool>(request_); }
    void shutdown(bool erase) {
        request_.shutdown = true;
        request_.erase_credentials |= erase;
        request_.start = Start::None;
    }
    void start(Start destination) { request_.start = destination; }
    Request take(bool scan_active) {
        if (scan_active) return {};
        const Request result = request_;
        request_ = {};
        return result;
    }
private:
    Request request_;
};
