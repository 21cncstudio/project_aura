// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

struct StreamProfile {
    size_t chunk_size;
    size_t min_chunk_size;
    uint16_t max_zero_writes;
    uint8_t yield_ms;
    uint16_t retry_delay_fast_ms;
    uint16_t retry_delay_medium_ms;
    uint16_t retry_delay_slow_ms;
    uint16_t retry_delay_very_slow_ms;
    uint16_t retry_delay_max_ms;
    uint32_t max_duration_ms;
    uint32_t no_progress_timeout_ms;
    bool adaptive_chunking;
    bool disable_wifi_power_save;
};

enum class StreamAbortReason : uint8_t {
    None = 0,
    Disconnected,
    InvalidSocket,
    ZeroWriteLimit,
    NoProgressTimeout,
    TotalTimeout,
    SocketWriteError
};

extern const StreamProfile kHtmlStreamProfile;
extern const StreamProfile kShellPageStreamProfile;
extern const StreamProfile kImmutableAssetStreamProfile;

const char *stream_abort_reason_text(StreamAbortReason reason);
size_t effective_stream_chunk_size(const StreamProfile &profile, uint16_t zero_writes);
uint16_t stream_retry_delay_ms(const StreamProfile &profile, uint16_t zero_writes);
