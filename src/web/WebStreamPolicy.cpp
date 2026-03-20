// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebStreamPolicy.h"

const StreamProfile kHtmlStreamProfile = {
    1460,
    1460,
    2048,
    1,
    2,
    12,
    50,
    100,
    150,
    45000,
    10000,
    false,
    false
};

const StreamProfile kShellPageStreamProfile = {
    512,
    256,
    2048,
    1,
    2,
    12,
    50,
    100,
    150,
    45000,
    20000,
    true,
    true
};

const StreamProfile kImmutableAssetStreamProfile = {
    1024,
    256,
    2048,
    1,
    2,
    12,
    50,
    100,
    150,
    65000,
    20000,
    true,
    true
};

const char *stream_abort_reason_text(StreamAbortReason reason) {
    switch (reason) {
        case StreamAbortReason::Disconnected:
            return "client_disconnected";
        case StreamAbortReason::InvalidSocket:
            return "invalid_socket";
        case StreamAbortReason::ZeroWriteLimit:
            return "zero_write_limit";
        case StreamAbortReason::NoProgressTimeout:
            return "no_progress_timeout";
        case StreamAbortReason::TotalTimeout:
            return "total_timeout";
        case StreamAbortReason::SocketWriteError:
            return "socket_write_error";
        case StreamAbortReason::None:
        default:
            return "none";
    }
}

size_t effective_stream_chunk_size(const StreamProfile &profile, uint16_t zero_writes) {
    size_t chunk = profile.chunk_size;
    if (!profile.adaptive_chunking || profile.min_chunk_size == 0 || profile.min_chunk_size >= chunk) {
        return chunk;
    }

    if (zero_writes >= 48) {
        chunk = profile.min_chunk_size;
    } else if (zero_writes >= 16) {
        chunk /= 2;
    } else if (zero_writes >= 4) {
        chunk = (chunk * 3U) / 4U;
    }

    if (chunk < profile.min_chunk_size) {
        chunk = profile.min_chunk_size;
    }
    return chunk;
}

uint16_t stream_retry_delay_ms(const StreamProfile &profile, uint16_t zero_writes) {
    if (zero_writes <= 3) {
        return profile.retry_delay_fast_ms;
    }
    if (zero_writes <= 10) {
        return profile.retry_delay_medium_ms;
    }
    if (zero_writes <= 40) {
        return profile.retry_delay_slow_ms;
    }
    if (zero_writes <= 120) {
        return profile.retry_delay_very_slow_ms;
    }
    return profile.retry_delay_max_ms;
}
