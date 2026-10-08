// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Lives for one boot. Save must be synchronous; a failed save never reboots.
class ScreenRotationRestartPolicy {
public:
    template<class Save, class Restart>
    bool request(bool &orientation, Save save, Restart restart) {
        if(pending_) return false;
        const bool previous = orientation;
        orientation = !previous;
        if(!save()) {
            orientation = previous;
            return false;
        }
        pending_ = true;
        restart();
        return true;
    }
    bool pending() const { return pending_; }
private:
    bool pending_ = false;
};
