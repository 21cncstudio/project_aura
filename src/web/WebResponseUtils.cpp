// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebResponseUtils.h"

#include "core/Logger.h"
#include "core/WifiPowerSaveGuard.h"

namespace {

uint32_t context_now_ms(const WebResponseUtils::StreamContext &context) {
    return context.nowMs ? context.nowMs(context.context) : 0;
}

uint32_t context_wifi_sta_connected_elapsed_ms(const WebResponseUtils::StreamContext &context) {
    return context.wifiStaConnectedElapsedMs ? context.wifiStaConnectedElapsedMs(context.context) : 0;
}

void send_no_store_headers(WebRequest &server) {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
}

void send_immutable_headers(WebRequest &server) {
    server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
}

struct TransferGuard {
    TransferGuard(const WebResponseUtils::StreamContext &context, bool enabled)
        : context_(context), enabled_(enabled && context.stream_state != nullptr) {
        if (enabled_) {
            context.stream_state->beginTransfer(context_now_ms(context));
        }
    }

    ~TransferGuard() {
        if (enabled_) {
            context_.stream_state->endTransfer(context_now_ms(context_));
        }
    }

private:
    const WebResponseUtils::StreamContext &context_;
    bool enabled_ = false;
};

void apply_asset_cache_headers(WebRequest &server, WebResponseUtils::AssetCacheMode cache_mode) {
    if (cache_mode == WebResponseUtils::AssetCacheMode::Immutable) {
        send_immutable_headers(server);
        return;
    }
    send_no_store_headers(server);
}

void record_web_stream_result(const WebResponseUtils::StreamContext &context,
                              const String &uri,
                              size_t total_size,
                              size_t sent,
                              bool ok,
                              StreamAbortReason abort_reason,
                              uint32_t max_write_ms,
                              int last_socket_errno) {
    if (!context.stream_state) {
        return;
    }
    context.stream_state->recordStreamResult(uri,
                                             total_size,
                                             sent,
                                             ok,
                                             abort_reason,
                                             max_write_ms,
                                             last_socket_errno,
                                             context.slow_write_warn_ms);
}

bool stream_response_body(WebRequest &server,
                          const uint8_t *data,
                          size_t body_size,
                          const StreamProfile &profile,
                          const WebResponseUtils::StreamContext &context,
                          bool response_started) {
    const uint32_t start_ms = context_now_ms(context);
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::None;
    uint32_t max_write_ms = 0;
    int last_socket_errno = 0;
    bool ok = response_started;
    if (ok && body_size) {
        ok = data && context.stream_runtime &&
            web_stream_client_bytes(server, data, body_size, profile, *context.stream_runtime,
                                    sent, abort_reason, max_write_ms, last_socket_errno);
    }
    if (!ok && abort_reason == StreamAbortReason::None) {
        abort_reason = web_stream_error_reason(server.streamError());
    }
    if (ok) {
        const uint32_t finish_start_ms = context_now_ms(context);
        ok = server.finishStreamResponse(last_socket_errno);
        const uint32_t finish_ms = context_now_ms(context) - finish_start_ms;
        if (finish_ms > max_write_ms) max_write_ms = finish_ms;
        if (!ok) abort_reason = web_stream_error_reason(server.streamError());
    }

    WebStreamRequestInfo request{};
    server.streamRequestInfo(request);
    if (!ok) {
        WebStreamFailure failure{};
        failure.request = request;
        failure.error = server.streamError();
        failure.first_ms = failure.last_ms = context_now_ms(context);
        failure.sent = static_cast<uint32_t>(sent);
        failure.total = static_cast<uint32_t>(body_size);
        failure.duration_ms = failure.last_ms - start_ms;
        failure.max_write_ms = max_write_ms;
        failure.reason = abort_reason;
        if (context.captureFailureEnvironment) {
            context.captureFailureEnvironment(context.context, failure.environment);
        }
        const bool log = !context.stream_state || context.stream_state->recordFailure(failure);
        server.stopClient();
        if (log) {
            Logger::log(Logger::Warn, "Web",
                        "HTTP interrupted: path=%s sent=%u/%u %s/%s sdk=%ld errno=%d valid=%u n=%lu",
                        request.path, static_cast<unsigned>(sent), static_cast<unsigned>(body_size),
                        web_stream_stage_text(failure.error.stage), stream_abort_reason_text(abort_reason),
                        static_cast<long>(failure.error.sdk_error), failure.error.socket_errno,
                        failure.error.errno_valid ? 1U : 0U, static_cast<unsigned long>(failure.count));
        }
    } else if (max_write_ms >= context.slow_write_warn_ms) {
        Logger::log(Logger::Warn, "Web", "HTTP slow write: path=%s size=%u max_write_ms=%u",
                    request.path, static_cast<unsigned>(body_size), static_cast<unsigned>(max_write_ms));
    }
    record_web_stream_result(context, request.path, body_size, sent, ok, abort_reason,
                             max_write_ms, last_socket_errno);
    return ok;
}

}  // namespace

namespace WebResponseUtils {

void sendNoStoreHeaders(WebRequest &server) {
    send_no_store_headers(server);
}

void sendNoStoreText(WebRequest &server, int status_code, const char *message) {
    send_no_store_headers(server);
    server.send(status_code, "text/plain", message);
}

void noteShellPriority(const StreamContext &context) {
    if (!context.stream_state) {
        return;
    }
    context.stream_state->noteShellPriority(
        context_now_ms(context), context_wifi_sta_connected_elapsed_ms(context));
}

bool shouldPauseMqttForTransfer(const StreamContext &context) {
    return context.stream_state && context.stream_state->shouldPauseMqtt(context_now_ms(context));
}

uint32_t transferPauseRemainingMs(const StreamContext &context) {
    if (!context.stream_state) {
        return 0;
    }
    return context.stream_state->pauseRemainingMs(context_now_ms(context));
}

bool sendHtmlStream(WebRequest &server, const String &html, const StreamContext &context) {
    send_no_store_headers(server);
    const bool started = server.beginStreamResponse(200, "text/html; charset=utf-8", html.length());
    return stream_response_body(server,
                                reinterpret_cast<const uint8_t *>(html.c_str()),
                                html.length(),
                                kHtmlStreamProfile,
                                context,
                                started);
}

bool sendHtmlStreamResilient(WebRequest &server, const String &html, const StreamContext &context) {
    noteShellPriority(context);
    TransferGuard transfer_guard(context, true);
    WifiPowerSaveGuard wifi_ps_guard;
    wifi_ps_guard.suspend();
    send_no_store_headers(server);
    const bool started = server.beginStreamResponse(200, "text/html; charset=utf-8", html.length());
    return stream_response_body(server,
                                reinterpret_cast<const uint8_t *>(html.c_str()),
                                html.length(),
                                kShellPageStreamProfile,
                                context,
                                started);
}

bool sendProgmemAsset(WebRequest &server,
                      const char *content_type,
                      const uint8_t *content,
                      size_t content_size,
                      bool gzip_encoded,
                      AssetCacheMode cache_mode,
                      const StreamContext &context,
                      const StreamProfile *profile_override) {
    const StreamProfile &profile =
        profile_override ? *profile_override :
                           ((cache_mode == AssetCacheMode::Immutable) ? kImmutableAssetStreamProfile
                                                                      : kHtmlStreamProfile);
    TransferGuard transfer_guard(
        context, profile_override != nullptr || cache_mode == AssetCacheMode::Immutable);
    WifiPowerSaveGuard wifi_ps_guard;
    if (profile.disable_wifi_power_save) {
        wifi_ps_guard.suspend();
    }
    apply_asset_cache_headers(server, cache_mode);
    const bool started = server.beginStreamResponse(200, content_type, content_size, gzip_encoded);
    return stream_response_body(
        server, content, content_size, profile, context, started);
}

bool sendHtmlStreamProgmem(WebRequest &server,
                           const uint8_t *content,
                           size_t content_size,
                           bool gzip_encoded,
                           const StreamContext &context) {
    noteShellPriority(context);
    return sendProgmemAsset(server,
                            "text/html; charset=utf-8",
                            content,
                            content_size,
                            gzip_encoded,
                            AssetCacheMode::NoStore,
                            context,
                            &kShellPageStreamProfile);
}

}  // namespace WebResponseUtils
