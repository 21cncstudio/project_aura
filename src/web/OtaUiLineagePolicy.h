// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

namespace OtaUiLineagePolicy {

inline bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms) {
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

inline bool isNewerConfirmId(uint32_t candidate, uint32_t current) {
    return candidate != current &&
           static_cast<int32_t>(candidate - current) > 0;
}

inline uint32_t validatedUploadConfirmId(bool consumed, uint32_t confirm_id) {
    return consumed && confirm_id != 0 ? confirm_id : 0;
}

class ScreenLineage {
public:
    bool accept(uint32_t confirm_id) {
        if (confirm_id == 0) {
            return false;
        }
        if (latest_confirm_id_ == 0 || confirm_id == latest_confirm_id_) {
            latest_confirm_id_ = confirm_id;
            return true;
        }
        if (!isNewerConfirmId(confirm_id, latest_confirm_id_)) {
            return false;
        }
        latest_confirm_id_ = confirm_id;
        return true;
    }

    uint32_t latestConfirmId() const {
        return latest_confirm_id_;
    }

    void reset() {
        latest_confirm_id_ = 0;
    }

private:
    uint32_t latest_confirm_id_ = 0;
};

class PreflightLease {
public:
    bool arm(uint32_t confirm_id, uint32_t due_ms) {
        if (!lineage_.accept(confirm_id)) {
            return false;
        }
        confirm_id_ = confirm_id;
        due_ms_ = due_ms;
        armed_ = true;
        return true;
    }

    bool cancel(uint32_t confirm_id) {
        if (!armed_ || confirm_id == 0 || confirm_id != confirm_id_) {
            return false;
        }
        clearLease();
        return true;
    }

    uint32_t takeIfDue(uint32_t now_ms) {
        if (!armed_ || !deadlineReached(now_ms, due_ms_)) {
            return 0;
        }
        const uint32_t expired_confirm_id = confirm_id_;
        clearLease();
        return expired_confirm_id;
    }

    uint32_t confirmId() const {
        return armed_ ? confirm_id_ : 0;
    }

    void reset() {
        clearLease();
        lineage_.reset();
    }

private:
    void clearLease() {
        confirm_id_ = 0;
        due_ms_ = 0;
        armed_ = false;
    }

    ScreenLineage lineage_{};
    uint32_t confirm_id_ = 0;
    uint32_t due_ms_ = 0;
    bool armed_ = false;
};

}  // namespace OtaUiLineagePolicy
