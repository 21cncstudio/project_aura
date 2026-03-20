// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebOtaState.h"

uint32_t WebOtaSnapshot::totalDurationMs(uint32_t now_ms) const {
    return upload_start_ms == 0 ? 0 : (now_ms - upload_start_ms);
}

uint32_t WebOtaSnapshot::firstChunkDelayMs() const {
    return first_chunk_seen ? (first_chunk_ms - upload_start_ms) : 0;
}

uint32_t WebOtaSnapshot::lastChunkAgeMs(uint32_t now_ms) const {
    return last_chunk_ms == 0 ? 0 : (now_ms - last_chunk_ms);
}

uint32_t WebOtaSnapshot::transferPhaseMs() const {
    return first_chunk_seen && last_chunk_ms >= first_chunk_ms ? (last_chunk_ms - first_chunk_ms) : 0;
}

size_t WebOtaSnapshot::avgChunkSize() const {
    return chunk_count > 0 ? (chunk_sum_size / chunk_count) : 0;
}

bool WebOtaSnapshot::hasError() const {
    return error.length() > 0;
}

void WebOtaState::reset() {
    upload_seen_ = false;
    active_.store(false, std::memory_order_release);
    success_ = false;
    size_known_ = false;
    expected_size_ = 0;
    slot_size_ = 0;
    written_size_ = 0;
    error_ = "";
    upload_start_ms_ = 0;
    first_chunk_ms_ = 0;
    last_chunk_ms_ = 0;
    finalize_ms_ = 0;
    chunk_count_ = 0;
    chunk_min_size_ = 0;
    chunk_max_size_ = 0;
    chunk_sum_size_ = 0;
    first_chunk_seen_ = false;
    start_rssi_valid_ = false;
    start_rssi_ = 0;
}

void WebOtaState::beginUpload(uint32_t now_ms) {
    reset();
    upload_seen_ = true;
    active_.store(true, std::memory_order_release);
    upload_start_ms_ = now_ms;
}

bool WebOtaState::isActive() const {
    return active_.load(std::memory_order_acquire);
}

void WebOtaState::setStartRssi(int rssi) {
    start_rssi_valid_ = true;
    start_rssi_ = rssi;
}

void WebOtaState::setSlotSize(size_t slot_size) {
    slot_size_ = slot_size;
}

void WebOtaState::setExpectedSize(bool known, size_t expected_size) {
    size_known_ = known;
    expected_size_ = known ? expected_size : 0;
}

bool WebOtaState::noteChunk(size_t chunk_size, uint32_t now_ms) {
    const bool first_chunk = !first_chunk_seen_;
    if (first_chunk) {
        first_chunk_seen_ = true;
        first_chunk_ms_ = now_ms;
    }
    last_chunk_ms_ = now_ms;
    if (chunk_count_ == 0 || chunk_size < chunk_min_size_) {
        chunk_min_size_ = chunk_size;
    }
    if (chunk_size > chunk_max_size_) {
        chunk_max_size_ = chunk_size;
    }
    chunk_sum_size_ += chunk_size;
    chunk_count_++;
    return first_chunk;
}

bool WebOtaState::wouldExceedSlot(size_t chunk_size) const {
    return written_size_ + chunk_size > slot_size_;
}

void WebOtaState::addWritten(size_t amount) {
    written_size_ += amount;
}

bool WebOtaState::writtenMatchesExpected() const {
    return !size_known_ || expected_size_ == 0 || written_size_ == expected_size_;
}

void WebOtaState::markFinalizeDuration(uint32_t finalize_ms) {
    finalize_ms_ = finalize_ms;
}

void WebOtaState::markSuccess() {
    success_ = true;
    active_.store(false, std::memory_order_release);
}

void WebOtaState::setErrorOnce(const String &error) {
    if (error_.length() == 0) {
        error_ = error;
    }
    success_ = false;
    active_.store(false, std::memory_order_release);
}

WebOtaSnapshot WebOtaState::snapshot() const {
    WebOtaSnapshot snapshot;
    snapshot.upload_seen = upload_seen_;
    snapshot.active = active_.load(std::memory_order_acquire);
    snapshot.success = success_;
    snapshot.size_known = size_known_;
    snapshot.expected_size = expected_size_;
    snapshot.slot_size = slot_size_;
    snapshot.written_size = written_size_;
    snapshot.error = error_;
    snapshot.upload_start_ms = upload_start_ms_;
    snapshot.first_chunk_ms = first_chunk_ms_;
    snapshot.last_chunk_ms = last_chunk_ms_;
    snapshot.finalize_ms = finalize_ms_;
    snapshot.chunk_count = chunk_count_;
    snapshot.chunk_min_size = chunk_min_size_;
    snapshot.chunk_max_size = chunk_max_size_;
    snapshot.chunk_sum_size = chunk_sum_size_;
    snapshot.first_chunk_seen = first_chunk_seen_;
    snapshot.start_rssi_valid = start_rssi_valid_;
    snapshot.start_rssi = start_rssi_;
    return snapshot;
}
