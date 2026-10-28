// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include "web/WebStreamPolicy.h"

enum class WebStreamStage : uint8_t { Setup, Body, Finish };

struct WebStreamRequestInfo {
    char path[96] = {};
    char peer[48] = {};
    char user_agent[80] = {};
    bool path_truncated = false;
    bool user_agent_truncated = false;
};

// errno is valid only when captured immediately after a failed socket send.
// sdk_error is the untouched httpd API return, never an errno surrogate.
struct WebStreamError {
    int32_t sdk_error = 0;
    int socket_errno = 0;
    int send_result = 0;
    uint32_t wire_bytes = 0;  // Includes headers and chunk framing, accepted by send().
    uint32_t partial_body_bytes = 0; // Body in the failed chunk, not included in sent.
    WebStreamStage stage = WebStreamStage::Setup;
    bool sdk_error_valid = false;
    bool errno_valid = false;
    bool terminal = false;  // Never replay an HTTP chunk after an SDK failure.
};

struct WebStreamEnvironment {
    uint32_t internal_free = 0;
    uint32_t internal_min_free = 0;
    uint32_t network_age_ms = 0;
    int32_t wifi_state = 0;
    int32_t wifi_sta_status = 0;
    int32_t rssi = 0;
    uint8_t wifi_retry_count = 0;
    uint8_t mqtt_retry_stage = 0;
    bool memory_valid = false;
    bool network_valid = false;
    bool wifi_enabled = false;
    bool wifi_connected = false;
    bool ap_mode = false;
    bool scan_in_progress = false;
    bool has_rssi = false;
    bool mqtt_enabled = false;
    bool mqtt_connected = false;
    bool mqtt_tls_waiting_for_time = false;
};

struct WebStreamFailure {
    WebStreamRequestInfo request;
    WebStreamError error;
    WebStreamEnvironment environment;
    uint32_t first_ms = 0;
    uint32_t last_ms = 0;
    uint32_t count = 1;
    uint32_t sent = 0; // Only fully accepted chunks (not proof of peer receipt).
    uint32_t total = 0;
    uint32_t duration_ms = 0;
    uint32_t max_write_ms = 0;
    StreamAbortReason reason = StreamAbortReason::None;
};

constexpr size_t kWebStreamFailureCapacity = 4;
static_assert(sizeof(WebStreamFailure) <= 384, "Keep the four-entry HTTP history bounded");

inline bool web_stream_copy_text(char *out, size_t capacity, const char *in, bool path = false) {
    if (!out || !capacity) return false;
    size_t i = 0;
    if (in) {
        for (; i + 1 < capacity && in[i] && !(path && (in[i] == '?' || in[i] == '#')); ++i) {
            const unsigned char c = static_cast<unsigned char>(in[i]);
            out[i] = c >= 32 && c < 127 ? static_cast<char>(c) : '?';
        }
    }
    out[i] = 0;
    return in && in[i] && !(path && (in[i] == '?' || in[i] == '#'));
}

inline const char *web_stream_stage_text(WebStreamStage stage) {
    switch (stage) {
        case WebStreamStage::Setup: return "setup";
        case WebStreamStage::Finish: return "finish";
        default: return "body";
    }
}

inline StreamAbortReason web_stream_error_reason(const WebStreamError &error) {
    if (!error.errno_valid) return StreamAbortReason::InternalError;
    switch (error.socket_errno) {
        case ECONNRESET: return StreamAbortReason::ConnectionReset;
        case EPIPE:
        case ENOTCONN:
        case ECONNABORTED: return StreamAbortReason::Disconnected;
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
        case ETIMEDOUT: return StreamAbortReason::SocketTimeout;
        default: return StreamAbortReason::SocketWriteError;
    }
}
