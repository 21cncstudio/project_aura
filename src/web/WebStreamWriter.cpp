// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebStreamWriter.h"

#include <errno.h>

namespace {

bool deadline_reached(uint32_t now_ms, uint32_t due_ms) {
    return static_cast<int32_t>(now_ms - due_ms) >= 0;
}

uint32_t runtime_now_ms(const WebStreamRuntime &runtime) {
    return runtime.nowMs ? runtime.nowMs(runtime.context) : 0;
}

void runtime_delay_ms(const WebStreamRuntime &runtime, uint16_t delay_ms) {
    if (runtime.delayMs) {
        runtime.delayMs(runtime.context, delay_ms);
    }
}

void runtime_kick_watchdog(const WebStreamRuntime &runtime) {
    if (runtime.kickWatchdog) {
        runtime.kickWatchdog(runtime.context);
    }
}

void maybe_service_connected_mqtt(const WebStreamRuntime &runtime,
                                  uint32_t now_ms,
                                  uint32_t &last_service_ms) {
    if (!runtime.serviceConnectedMqtt || runtime.mqttServiceIntervalMs == 0) {
        return;
    }
    if (last_service_ms != 0 &&
        static_cast<uint32_t>(now_ms - last_service_ms) < runtime.mqttServiceIntervalMs) {
        return;
    }
    runtime.serviceConnectedMqtt(runtime.context, now_ms);
    last_service_ms = now_ms;
}

}  // namespace

bool web_stream_client_bytes(WebRequest &server,
                             const uint8_t *data,
                             size_t size,
                             const StreamProfile &profile,
                             const WebStreamRuntime &runtime,
                             size_t &sent,
                             StreamAbortReason &abort_reason,
                             uint32_t &max_write_ms,
                             int &last_socket_errno) {
    sent = 0;
    abort_reason = StreamAbortReason::None;
    max_write_ms = 0;
    last_socket_errno = 0;
    if (!data || size == 0) {
        return true;
    }

    uint16_t zero_writes = 0;
    const uint32_t start_ms = runtime_now_ms(runtime);
    uint32_t last_progress_ms = start_ms;
    uint32_t last_mqtt_service_ms = start_ms;
    while (sent < size) {
        runtime_kick_watchdog(runtime);

        size_t to_send = size - sent;
        const size_t chunk_size = effective_stream_chunk_size(profile, zero_writes);
        if (to_send > chunk_size) {
            to_send = chunk_size;
        }

        const uint32_t write_start_ms = runtime_now_ms(runtime);
        const int32_t written = server.writeStreamChunk(data + sent, to_send, last_socket_errno);
        const uint32_t write_elapsed_ms = runtime_now_ms(runtime) - write_start_ms;
        if (write_elapsed_ms > max_write_ms) {
            max_write_ms = write_elapsed_ms;
        }

        const uint32_t now_ms = runtime_now_ms(runtime);
        maybe_service_connected_mqtt(runtime, now_ms, last_mqtt_service_ms);
        if (written > 0) {
            sent += static_cast<size_t>(written);
            zero_writes = 0;
            last_progress_ms = now_ms;
            last_socket_errno = 0;
            if (deadline_reached(now_ms, start_ms + profile.max_duration_ms)) {
                abort_reason = StreamAbortReason::TotalTimeout;
                server.stopClient();
                return false;
            }
            if (sent < size) {
                runtime_delay_ms(runtime, profile.yield_ms);
                runtime_kick_watchdog(runtime);
            }
            continue;
        }

        if (written == 0) {
            if (!server.clientConnected()) {
                abort_reason = StreamAbortReason::Disconnected;
                server.stopClient();
                return false;
            }
            last_socket_errno = 0;
        } else if (written < 0) {
            if (last_socket_errno == ENOTCONN
#ifdef EPIPE
                || last_socket_errno == EPIPE
#endif
#ifdef ECONNRESET
                || last_socket_errno == ECONNRESET
#endif
                ) {
                abort_reason = StreamAbortReason::Disconnected;
                server.stopClient();
                return false;
            }
            if (last_socket_errno != EAGAIN
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
                && last_socket_errno != EWOULDBLOCK
#endif
#ifdef ENOBUFS
                && last_socket_errno != ENOBUFS
#endif
                && last_socket_errno != EINTR) {
                abort_reason = StreamAbortReason::SocketWriteError;
                server.stopClient();
                return false;
            }
        }

        if (++zero_writes > profile.max_zero_writes) {
            abort_reason = StreamAbortReason::ZeroWriteLimit;
            server.stopClient();
            return false;
        }
        if (deadline_reached(now_ms, start_ms + profile.max_duration_ms)) {
            abort_reason = StreamAbortReason::TotalTimeout;
            server.stopClient();
            return false;
        }
        if (deadline_reached(now_ms, last_progress_ms + profile.no_progress_timeout_ms)) {
            abort_reason = StreamAbortReason::NoProgressTimeout;
            server.stopClient();
            return false;
        }
        const uint16_t retry_wait_ms = stream_retry_delay_ms(profile, zero_writes);
        const bool writable = server.waitUntilWritable(retry_wait_ms, last_socket_errno);
        if (!writable
            && last_socket_errno != 0
            && last_socket_errno != EAGAIN
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
            && last_socket_errno != EWOULDBLOCK
#endif
            && last_socket_errno != EINTR) {
            abort_reason = StreamAbortReason::SocketWriteError;
            server.stopClient();
            return false;
        }
        if (!writable && last_socket_errno == EINTR) {
            last_socket_errno = 0;
        }
        if (!writable) {
            runtime_delay_ms(runtime, profile.yield_ms);
        }
        runtime_kick_watchdog(runtime);
    }
    return true;
}
