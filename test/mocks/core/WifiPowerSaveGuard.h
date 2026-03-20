// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html

#pragma once

class WifiPowerSaveGuard {
public:
    WifiPowerSaveGuard() = default;
    ~WifiPowerSaveGuard() { restore(); }

    bool suspend() {
        active_ = true;
        return true;
    }

    void restore() {
        active_ = false;
    }

    bool isActive() const { return active_; }

private:
    bool active_ = false;
};
