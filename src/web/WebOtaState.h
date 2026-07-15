// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>

struct WebOtaSnapshot {
    bool upload_seen = false;
    bool active = false;
    bool success = false;
    bool reboot_pending = false;
    bool size_known = false;
    uint32_t session_id = 0;
    size_t expected_size = 0;
    size_t slot_size = 0;
    size_t written_size = 0;
    String error;
    uint32_t upload_start_ms = 0;
    uint32_t first_chunk_ms = 0;
    uint32_t last_chunk_ms = 0;
    uint32_t finalize_ms = 0;
    uint32_t chunk_count = 0;
    size_t chunk_min_size = 0;
    size_t chunk_max_size = 0;
    size_t chunk_sum_size = 0;
    bool first_chunk_seen = false;
    bool start_rssi_valid = false;
    int start_rssi = 0;
    uint32_t result_set_ms = 0;
    uint32_t result_ttl_ms = 0;

    uint32_t totalDurationMs(uint32_t now_ms) const;
    uint32_t firstChunkDelayMs() const;
    uint32_t lastChunkAgeMs(uint32_t now_ms) const;
    uint32_t transferPhaseMs() const;
    size_t avgChunkSize() const;
    bool hasError() const;
    bool hasTerminalResult(uint32_t now_ms) const;
};

class WebOtaState {
public:
    static constexpr uint32_t terminalResultTtlMs() {
        return 60000;
    }

    void reset();
    void beginUpload(uint32_t now_ms);
    bool isActive() const;
    bool isBusy() const;
    bool hasError() const;
    void setTotalTimeoutMs(uint32_t timeout_ms);
    bool totalTimeoutExceeded(uint32_t now_ms) const;
    void expireTerminalResult(uint32_t now_ms);
    void setStartRssi(int rssi);
    void setSlotSize(size_t slot_size);
    void setExpectedSize(bool known, size_t expected_size);
    bool noteChunk(size_t chunk_size, uint32_t now_ms);
    bool wouldExceedSlot(size_t chunk_size) const;
    void addWritten(size_t amount);
    bool writtenMatchesExpected() const;
    void markFinalizeDuration(uint32_t finalize_ms);
    void markSuccess(uint32_t now_ms);
    void markRebootPending();
    void setErrorOnce(const String &error, uint32_t now_ms);
    void clearBusy();
    WebOtaSnapshot snapshot() const;

private:
    void setTerminalResultDeadline(uint32_t now_ms);

    std::atomic<bool> active_{false};
    std::atomic<bool> busy_{false};
    bool upload_seen_ = false;
    bool success_ = false;
    bool reboot_pending_ = false;
    bool size_known_ = false;
    uint32_t session_id_ = 0;
    uint32_t next_session_id_ = 1;
    uint32_t total_timeout_ms_ = 0;
    size_t expected_size_ = 0;
    size_t slot_size_ = 0;
    size_t written_size_ = 0;
    String error_;
    uint32_t upload_start_ms_ = 0;
    uint32_t first_chunk_ms_ = 0;
    uint32_t last_chunk_ms_ = 0;
    uint32_t finalize_ms_ = 0;
    uint32_t chunk_count_ = 0;
    size_t chunk_min_size_ = 0;
    size_t chunk_max_size_ = 0;
    size_t chunk_sum_size_ = 0;
    bool first_chunk_seen_ = false;
    bool start_rssi_valid_ = false;
    int start_rssi_ = 0;
    uint32_t result_set_ms_ = 0;
    uint32_t result_ttl_ms_ = 0;
};
