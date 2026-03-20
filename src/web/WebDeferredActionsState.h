// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

struct WebDeferredActionsDue {
    bool wifi_start_sta = false;
    bool mqtt_sync = false;
};

class WebDeferredActionsState {
public:
    WebDeferredActionsState();

    void reset();
    void scheduleWifiStartSta(uint32_t now_ms, uint32_t delay_ms);
    void scheduleMqttSync(uint32_t now_ms, uint32_t delay_ms);
    WebDeferredActionsDue pollDue(uint32_t now_ms);

private:
    struct State {
        bool wifi_start_sta = false;
        bool mqtt_sync = false;
        uint32_t wifi_start_sta_due_ms = 0;
        uint32_t mqtt_sync_due_ms = 0;
    };

    static bool deadlineReached(uint32_t now_ms, uint32_t due_ms);
    void lock() const;
    void unlock() const;

#ifdef UNIT_TEST
    mutable std::mutex mutex_;
#else
    mutable StaticSemaphore_t mutex_buffer_{};
    mutable SemaphoreHandle_t mutex_ = nullptr;
#endif
    State state_{};
};
