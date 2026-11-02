// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebTransport.h"
#include "web/WebHttpSendCapture.h"
#include "web/WebHttpResponseStorage.h"
#include "web/WebMultipart.h"
#include "web/WebQueryString.h"

#include <algorithm>
#include <errno.h>
#include <list>
#include <memory>
#include <vector>

#include <esp_http_server.h>
#include <esp_log.h>
#include <lwip/sockets.h>

#include "core/Logger.h"
#include "core/WakePowerGuard.h"
#include "core/Watchdog.h"
#include "web/WebServerLimits.h"

namespace {

constexpr uint16_t kHttpServerRecvWaitTimeoutS = 10;
// Bound one synchronous chunk/plain response below the tightest stream idle
// budget (10 s). Partial SDK sends share this deadline; they cannot restart it.
constexpr uint16_t kHttpServerSendWaitTimeoutS = 5;
constexpr uint32_t kHttpWriteBudgetMs = kHttpServerSendWaitTimeoutS * 1000UL;
constexpr uint16_t kHttpServerMaxOpenSockets = 10;
static_assert(kHttpServerMaxOpenSockets + 3 <= CONFIG_LWIP_MAX_SOCKETS,
              "HTTP client sockets plus three internal sockets must fit LWIP_MAX_SOCKETS");
constexpr uint32_t kMultipartReadIdleTimeoutMs = 90UL * 1000UL;
constexpr uint32_t kDrainRecvTimeoutMs = 200;
// Chromium may finish writing a buffered upload before reading an early
// response. Allow useful progress, with independent time/byte/idle bounds.
// The measured 4.3-inch AP can sustain only 34-40 KiB/s. Give a current
// ~3.4 MB rejected image margin at 32 KiB/s, while retaining the independent
// progress/byte bounds and cooperative wake yielding. This is per teardown.
constexpr uint32_t kCloseDrainTimeoutMs = 120000;
// Real AP packet captures contain TCP retransmission recovery longer than
// 1.5 s even at hundreds of KiB/s. Keep a useful-progress quota, allowing
// recovery without resetting the independent 120 s / 8 MiB limits.
constexpr uint32_t kCloseProgressTimeoutMs = 5000;
constexpr size_t kCloseProgressBytes = 32 * 1024;
constexpr size_t kCloseMaxDrainBytes = 8 * 1024 * 1024;
constexpr uint32_t kCloseRecvSliceMs = 50;
constexpr uint32_t kWakeActivityAdmissionTimeoutMs = 3000;

WakePowerGuard::Activity wait_for_web_activity_admission(
    bool &wait_exceeded) {
    const uint32_t started_ms = millis();
    wait_exceeded = false;
    while (true) {
        WakePowerGuard::Activity activity =
            WakePowerGuard::tryAcquireActivity(millis());
        if (activity) {
            return activity;
        }
        if (static_cast<uint32_t>(millis() - started_ms) >=
            kWakeActivityAdmissionTimeoutMs) {
            wait_exceeded = true;
            // Preserve the hardware quiet window. A timeout only changes the
            // eventual HTTP response; even FIN/RST must wait for admission.
        }
        delay(1);
    }
}

enum class RequestBodyReadResult : uint8_t {
    Ok,
    TooLarge,
    ReadError,
    AllocationFailed,
};

bool socket_likely_connected(int sockfd) {
    if (sockfd < 0) {
        return false;
    }

    uint8_t probe = 0;
    const int received = recv(sockfd,
                              reinterpret_cast<char *>(&probe),
                              1,
                              MSG_PEEK | MSG_DONTWAIT);
    if (received > 0) {
        return true;
    }
    if (received == 0) {
        return false;
    }

    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}

} // namespace

static esp_err_t esp_route_dispatch(httpd_req_t *req);
static esp_err_t esp_not_found_dispatch(httpd_req_t *req, httpd_err_code_t error);

class EspHttpServerBackend final : public WebServerBackend {
public:
    class EspHttpRequest;
    struct RouteRegistration;

    explicit EspHttpServerBackend(uint16_t port);
    ~EspHttpServerBackend() override;

    WebRequest &request() override;
    void onGet(const char *uri, WebHandlerFn handler) override;
    void onPost(const char *uri, WebHandlerFn handler) override;
    void onPostUpload(const char *uri,
                      WebHandlerFn handler,
                      WebHandlerFn upload_handler,
                      WebHandlerFn request_preflight_handler) override;
    void onNotFound(WebHandlerFn handler) override;
    const char *name() const override;
    void begin() override;
    void stop() override;

    int socketSend(int sockfd, const char *buf, size_t size, int flags);
    bool prepareRequest(RouteRegistration &route, void *req);
    void resetRequest();
    esp_err_t finalizeRequest(WakePowerGuard::Activity &activity, bool rejected = false);
    esp_err_t rejectRequest(void *req, int status, const char *message, WakePowerGuard::Activity &activity);
    bool hasNotFoundHandler() const;
    void dispatchError(void *req, httpd_err_code_t error);

private:
    bool registerRoute(RouteRegistration &route);

    uint16_t port_ = 80;
    void *server_handle_ = nullptr;
    EspHttpRequest *request_ = nullptr;
    void *routes_ = nullptr;
    WebHandlerFn not_found_handler_ = nullptr;
};

namespace {

String status_line_for_code(int status_code) {
    switch (status_code) {
        case 200: return "200 OK";
        case 202: return "202 Accepted";
        case 204: return "204 No Content";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 409: return "409 Conflict";
        case 413: return "413 Payload Too Large";
        case 499: return "499 Client Closed Request";
        case 500: return "500 Internal Server Error";
        case 501: return "501 Not Implemented";
        case 503: return "503 Service Unavailable";
        default: {
            String status = String(status_code);
            status += " Unknown";
            return status;
        }
    }
}

String read_header_value(httpd_req_t *req, const char *field) {
    if (!req || !field) {
        return String();
    }
    const size_t len = httpd_req_get_hdr_value_len(req, field);
    if (len == 0) {
        return String();
    }
    std::unique_ptr<char[]> buffer(new char[len + 1]);
    buffer[0] = '\0';
    if (httpd_req_get_hdr_value_str(req, field, buffer.get(), len + 1) != ESP_OK) {
        return String();
    }
    return String(buffer.get());
}

RequestBodyReadResult read_request_body(httpd_req_t *req, String &out) {
    if (!req || req->content_len == 0) {
        out = "";
        return RequestBodyReadResult::Ok;
    }
    if (!WebServerLimits::requestBodySizeAllowed(req->content_len)) {
        out = "";
        return RequestBodyReadResult::TooLarge;
    }

    out = "";
    if (!out.reserve(req->content_len)) {
        return RequestBodyReadResult::AllocationFailed;
    }
    size_t remaining = req->content_len;
    char buffer[512];
    while (remaining > 0) {
        const size_t chunk_size = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const int received = httpd_req_recv(req, buffer, chunk_size);
        if (received <= 0) {
            return RequestBodyReadResult::ReadError;
        }
        if (!out.concat(buffer, static_cast<unsigned int>(received))) {
            return RequestBodyReadResult::AllocationFailed;
        }
        remaining -= static_cast<size_t>(received);
    }
    return RequestBodyReadResult::Ok;
}

std::list<EspHttpServerBackend::RouteRegistration> &backend_routes(void *storage) {
    return *static_cast<std::list<EspHttpServerBackend::RouteRegistration> *>(storage);
}

} // namespace

struct EspHttpServerBackend::RouteRegistration {
    String uri;
    httpd_method_t method = HTTP_GET;
    WebHandlerFn handler = nullptr;
    WebHandlerFn upload_handler = nullptr;
    WebHandlerFn request_preflight_handler = nullptr;
    EspHttpServerBackend *backend = nullptr;
    httpd_uri_t descriptor = {};
};

class EspHttpServerBackend::EspHttpRequest final : public WebRequest {
public:
    class BufferedBodyReader {
    public:
        explicit BufferedBodyReader(EspHttpRequest *request)
            : request_(request),
              req_(request ? request->req_ : nullptr),
              remaining_(req_ && req_->content_len > 0 ? static_cast<size_t>(req_->content_len) : 0) {
            buffer_.reserve(2048);
        }

        size_t remainingBytesOnSocket() const {
            return remaining_;
        }

        bool readLine(String &line) {
            while (true) {
                auto it = std::search(buffer_.begin(),
                                      buffer_.end(),
                                      kCrLf,
                                      kCrLf + 2);
                if (it != buffer_.end()) {
                    const size_t line_len = static_cast<size_t>(std::distance(buffer_.begin(), it));
                    if (!WebServerLimits::multipartHeaderLineSizeAllowed(line_len)) {
                        limit_exceeded_ = true;
                        return false;
                    }
                    line = String(reinterpret_cast<const char *>(buffer_.data()), line_len);
                    buffer_.erase(buffer_.begin(), it + 2);
                    return true;
                }
                if (!WebServerLimits::multipartHeaderLineSizeAllowed(buffer_.size())) {
                    limit_exceeded_ = true;
                    return false;
                }
                if (!receiveMore()) {
                    return false;
                }
            }
        }

        bool limitExceeded() const {
            return limit_exceeded_;
        }

        template <typename Callback>
        bool streamUntilBoundary(const String &boundary, Callback callback, bool &final_boundary) {
            final_boundary = false;
            const String marker_string = String("\r\n--") + boundary;
            std::vector<uint8_t> marker(marker_string.length());
            for (size_t i = 0; i < marker.size(); ++i) {
                marker[i] = static_cast<uint8_t>(marker_string[i]);
            }

            while (true) {
                auto it = std::search(buffer_.begin(), buffer_.end(), marker.begin(), marker.end());
                if (it != buffer_.end()) {
                    const size_t data_len = static_cast<size_t>(std::distance(buffer_.begin(), it));
                    if (data_len > 0 && !callback(buffer_.data(), data_len)) {
                        return false;
                    }
                    buffer_.erase(buffer_.begin(), it + static_cast<ptrdiff_t>(marker.size()));
                    return consumeBoundarySuffix(final_boundary);
                }

                if (remaining_ == 0) {
                    return false;
                }

                const size_t keep_bytes = marker.size() > 1 ? marker.size() - 1 : 0;
                if (buffer_.size() > keep_bytes) {
                    const size_t flush_len = buffer_.size() - keep_bytes;
                    if (flush_len > 0 && !callback(buffer_.data(), flush_len)) {
                        return false;
                    }
                    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(flush_len));
                }

                if (!receiveMore()) {
                    return false;
                }
            }
        }

    private:
        bool receiveMore() {
            if (!req_ || remaining_ == 0) {
                return false;
            }

            char chunk[2048];
            while (remaining_ > 0) {
                const uint32_t now_ms = millis();
                if (request_ && request_->uploadDeadlineExceeded(now_ms)) {
                    request_->setUploadAbortReason(WebUploadAbortReason::TotalTimeout);
                    return false;
                }

                const size_t to_read = remaining_ < sizeof(chunk) ? remaining_ : sizeof(chunk);
                const int received = httpd_req_recv(req_, chunk, to_read);
                if (received > 0) {
                    timeout_window_active_ = false;
                    timeout_window_start_ms_ = 0;
                    buffer_.insert(buffer_.end(), chunk, chunk + received);
                    remaining_ -= static_cast<size_t>(received);
                    return true;
                }
                if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                    if (!timeout_window_active_) {
                        timeout_window_active_ = true;
                        timeout_window_start_ms_ = now_ms;
                    }
                    if (request_ && request_->uploadDeadlineExceeded(now_ms)) {
                        request_->setUploadAbortReason(WebUploadAbortReason::TotalTimeout);
                        return false;
                    }
                    if (static_cast<uint32_t>(now_ms - timeout_window_start_ms_) >=
                        kMultipartReadIdleTimeoutMs) {
                        if (request_) {
                            request_->setUploadAbortReason(WebUploadAbortReason::IdleTimeout);
                        }
                        return false;
                    }
                    Watchdog::kick();
                    delay(1);
                    continue;
                }
                if (request_) {
                    request_->setUploadAbortReason(request_->socketLikelyConnected()
                                                       ? WebUploadAbortReason::SocketError
                                                       : WebUploadAbortReason::ClientDisconnected);
                }
                return false;
            }
            return false;
        }

        bool ensureAvailable(size_t count) {
            while (buffer_.size() < count) {
                if (!receiveMore()) {
                    return false;
                }
            }
            return true;
        }

        bool consumeBoundarySuffix(bool &final_boundary) {
            if (!ensureAvailable(2)) {
                return false;
            }

            if (buffer_[0] == '-' && buffer_[1] == '-') {
                final_boundary = true;
                buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
                if (buffer_.size() < 2 && remaining_ > 0) {
                    receiveMore();
                }
                if (buffer_.size() >= 2 && buffer_[0] == '\r' && buffer_[1] == '\n') {
                    buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
                }
                return true;
            }

            if (buffer_[0] == '\r' && buffer_[1] == '\n') {
                final_boundary = false;
                buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
                return true;
            }

            return false;
        }

        EspHttpRequest *request_ = nullptr;
        httpd_req_t *req_ = nullptr;
        size_t remaining_ = 0;
        std::vector<uint8_t> buffer_{};
        bool timeout_window_active_ = false;
        uint32_t timeout_window_start_ms_ = 0;
        bool limit_exceeded_ = false;
        static constexpr char kCrLf[2] = {'\r', '\n'};
    };

    void begin(httpd_req_t *req) {
        resetUploadResponseState();
        req_ = req;
        close_requested_ = false;
        response_bytes_sent_ = false;
        socket_send_failed_ = false;
        write_budget_active_ = false;
        send_capture_.reset();
        simple_failure_pending_ = false;
        request_info_ = {};
        request_info_.path_truncated = web_stream_copy_text(
            request_info_.path, sizeof(request_info_.path), req ? req->uri : nullptr, true);
        if (req) {
            const size_t ua_length = httpd_req_get_hdr_value_len(req, "User-Agent");
            const esp_err_t ua_result = httpd_req_get_hdr_value_str(
                req, "User-Agent", request_info_.user_agent, sizeof(request_info_.user_agent));
            if (ua_result != ESP_OK && ua_result != ESP_ERR_HTTPD_RESULT_TRUNC) {
                request_info_.user_agent[0] = 0;
            }
            request_info_.user_agent_truncated = ua_length >= sizeof(request_info_.user_agent);
            web_stream_copy_text(request_info_.user_agent, sizeof(request_info_.user_agent),
                                 request_info_.user_agent);
            sockaddr_storage peer{};
            socklen_t peer_size = sizeof(peer);
            const int fd = httpd_req_to_sockfd(req);
            if (getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &peer_size) == 0) {
                if (peer.ss_family == AF_INET) {
                    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(&peer)->sin_addr,
                              request_info_.peer, sizeof(request_info_.peer));
                } else if (peer.ss_family == AF_INET6) {
                    inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6 *>(&peer)->sin6_addr,
                              request_info_.peer, sizeof(request_info_.peer));
                }
            }
            // This backend is plain HTTP, with one synchronous request per server task.
            // The hook remains on the socket; capture is active only inside chunk().
            const esp_err_t hook_result = httpd_sess_set_send_override(req->handle, fd,
                [](httpd_handle_t hd, int socket, const char *buf, size_t len, int flags) -> int {
                    auto *backend = static_cast<EspHttpServerBackend *>(httpd_get_global_user_ctx(hd));
                    return backend->socketSend(socket, buf, len, flags);
                });
            if (hook_result != ESP_OK) send_capture_.failSetup(hook_result);
        }
        response_storage_.reset();
        args_.clear();
        raw_body_ = "";
        upload_ = {};
        stream_open_ = false;
        pending_body_bytes_ = 0;
        upload_deadline_ms_ = 0;
        upload_abort_reason_ = WebUploadAbortReason::None;
        upload_rejected_ = false;
    }

    void reset() {
        resetUploadResponseState();
        req_ = nullptr;
        close_requested_ = false;
        response_bytes_sent_ = false;
        socket_send_failed_ = false;
        write_budget_active_ = false;
        send_capture_.reset();
        simple_failure_pending_ = false;
        request_info_ = {};
        response_storage_.reset();
        args_.clear();
        raw_body_ = "";
        upload_ = {};
        stream_open_ = false;
        pending_body_bytes_ = 0;
        upload_deadline_ms_ = 0;
        upload_abort_reason_ = WebUploadAbortReason::None;
        upload_rejected_ = false;
    }

    void appendArgsFromQuery() {
        if (!req_) {
            return;
        }
        const size_t query_len = httpd_req_get_url_query_len(req_);
        if (query_len == 0) {
            return;
        }
        std::unique_ptr<char[]> buffer(new char[query_len + 1]);
        if (httpd_req_get_url_query_str(req_, buffer.get(), query_len + 1) != ESP_OK) {
            return;
        }
        WebQueryString::parseArgs(String(buffer.get()), args_);
    }

    void appendArgsFromFormBody(const String &body) {
        WebQueryString::parseArgs(body, args_);
    }

    void setRawBody(const String &body) {
        raw_body_ = body;
    }

    void setArg(const String &name, const String &value) {
        for (WebQueryArg &arg : args_) {
            if (arg.key == name) {
                arg.value = value;
                return;
            }
        }
        WebQueryArg arg{};
        arg.key = name;
        arg.value = value;
        args_.push_back(arg);
    }

    void setUpload(const WebUpload &upload) {
        upload_ = upload;
    }

    void setPendingBodyBytes(size_t pending_body_bytes) {
        pending_body_bytes_ = pending_body_bytes;
    }

    bool hasArg(const char *name) const override {
        if (!name) {
            return false;
        }
        if (strcmp(name, "plain") == 0) {
            return !raw_body_.isEmpty();
        }
        for (const WebQueryArg &arg : args_) {
            if (arg.key == name) {
                return true;
            }
        }
        return false;
    }

    String arg(const char *name) const override {
        if (!name) {
            return String();
        }
        if (strcmp(name, "plain") == 0) {
            return raw_body_;
        }
        for (const WebQueryArg &arg : args_) {
            if (arg.key == name) {
                return arg.value;
            }
        }
        return String();
    }

    String uri() const override {
        return req_ ? String(req_->uri) : String();
    }

    void sendHeader(const char *name, const String &value, bool) override {
        if (!req_ || !name) {
            return;
        }

        WebHttpResponseStorage::HeaderView stored;
        if (!response_storage_.storeHeader(name, value, stored)) {
            send_capture_.failSetup(0, false);
            LOGW("WEB", "response header storage failed: %s", name);
            return;
        }
        const esp_err_t err = httpd_resp_set_hdr(req_, stored.name, stored.value);
        if (err != ESP_OK) {
            send_capture_.failSetup(err);
            LOGW("WEB", "response header rejected: %s (%s)", name, esp_err_to_name(err));
        }
    }

    void send(int status_code, const char *content_type, const String &content) override {
        send(status_code, content_type, content.c_str());
    }

    void send(int status_code, const char *content_type, const char *content) override {
        if (!req_) return;
        const char *body = content ? content : "";
        const size_t length = strlen(body);
        const uint32_t started = millis();
        const auto failed = [&] {
            simple_total_ = req_->method == HTTP_HEAD ? 0 : length;
            simple_duration_ms_ = millis() - started;
            simple_failure_pending_ = true;
            stopClient();
        };
        if (close_requested_ || send_capture_.error().terminal) {
            if (send_capture_.error().terminal) failed();
            return;
        }
        const char *stored_status = nullptr;
        if (!response_storage_.storeStatus(status_line_for_code(status_code), stored_status)) {
            send_capture_.failSetup(0, false); failed(); return;
        }
        const esp_err_t status_err = httpd_resp_set_status(req_, stored_status);
        if (status_err != ESP_OK) { send_capture_.failSetup(status_err); failed(); return; }
        if (content_type) {
            const char *stored_content_type = nullptr;
            if (!response_storage_.storeContentType(content_type, stored_content_type)) {
                send_capture_.failSetup(0, false); failed(); return;
            }
            const esp_err_t type_err = httpd_resp_set_type(req_, stored_content_type);
            if (type_err != ESP_OK) { send_capture_.failSetup(type_err); failed(); return; }
        }
        beginWriteBudget();
        const uint8_t *wire_body = req_->method == HTTP_HEAD ? nullptr : reinterpret_cast<const uint8_t *>(body);
        const bool ok = send_capture_.chunk(WebStreamStage::Body, wire_body, length, [&] {
            // Preserve GET's Content-Length on HEAD, but send no body.
            return httpd_resp_send(req_, reinterpret_cast<const char *>(wire_body), length);
        });
        write_budget_active_ = false;
        if (!ok) failed();
    }

    bool takeSimpleResponseFailure(WebStreamFailure &out) override {
        if (!simple_failure_pending_) return false;
        simple_failure_pending_ = false;
        out = {};
        out.request = request_info_;
        out.error = send_capture_.error();
        out.total = static_cast<uint32_t>(simple_total_);
        // A simple response is one SDK operation: partial body bytes are in
        // error.partial_body_bytes, not counted as a fully accepted chunk.
        out.duration_ms = out.max_write_ms = simple_duration_ms_;
        out.reason = web_stream_error_reason(out.error);
        return true;
    }

    void sendRejection(int status, const char *message) {
        sendHeader("Connection", "close", false);
        send(status, "text/plain", message);
    }

    bool clientConnected() const override {
        return !close_requested_ && socketLikelyConnected();
    }

    void setUploadDeadlineMs(uint32_t timeout_ms) override {
        if (timeout_ms == 0) {
            upload_deadline_ms_ = 0;
            return;
        }
        upload_deadline_ms_ = millis() + timeout_ms;
    }

    void clearUploadDeadline() override {
        upload_deadline_ms_ = 0;
    }

    void rejectUpload() override {
        upload_rejected_ = true;
        clearUploadDeadline();
    }

    bool uploadRejected() const override {
        return upload_rejected_;
    }

    size_t pendingRequestBodyBytes() const override {
        return pending_body_bytes_;
    }

    size_t drainPendingRequestBody(size_t max_bytes, uint32_t max_time_ms) override {
        if (!req_ || pending_body_bytes_ == 0 || max_bytes == 0 || max_time_ms == 0) {
            return 0;
        }

        const int sockfd = httpd_req_to_sockfd(req_);
        if (sockfd < 0) {
            return 0;
        }

        timeval recv_timeout{};
        recv_timeout.tv_sec = static_cast<long>(kDrainRecvTimeoutMs / 1000UL);
        recv_timeout.tv_usec =
            static_cast<long>((kDrainRecvTimeoutMs % 1000UL) * 1000UL);
        setsockopt(sockfd,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   &recv_timeout,
                   sizeof(recv_timeout));

        const uint32_t deadline_ms = millis() + max_time_ms;
        size_t drained = 0;
        uint8_t drain_buffer[1024];
        while (pending_body_bytes_ > 0 && drained < max_bytes) {
            if (WakePowerGuard::backgroundPaused(millis()) || static_cast<int32_t>(millis() - deadline_ms) >= 0) {
                break;
            }

            const size_t remaining_budget = max_bytes - drained;
            const size_t read_size =
                std::min(sizeof(drain_buffer),
                         std::min(remaining_budget, pending_body_bytes_));
            const int received =
                recv(sockfd, reinterpret_cast<char *>(drain_buffer), read_size, 0);
            if (received > 0) {
                drained += static_cast<size_t>(received);
                pending_body_bytes_ -= static_cast<size_t>(received);
                continue;
            }
            if (received == 0) {
                pending_body_bytes_ = 0;
                break;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
        return drained;
    }

    void stopClient() override {
        stream_open_ = false; // Finalization must not send after an aborted chunk.
        // Requests execute synchronously on the HTTP task. Returning an error
        // from dispatch lets the SDK close exactly this session before reuse.
        // Repeated stop requests are idempotent and cannot exhaust queue_work.
        close_requested_ = true;
    }

    void prepareSessionClose(WakePowerGuard::Activity &activity) {
        if (!req_) return;
        if (!response_bytes_sent_ || socket_send_failed_ || send_capture_.error().terminal) {
            Logger::log(Logger::Info, "Web", "HTTP close drain: reason=%s bytes=0",
                        socket_send_failed_ || send_capture_.error().terminal
                            ? "send_failed" : "no_response");
            return;
        }
        const int fd = httpd_req_to_sockfd(req_);
        // RFC 9112 9.6: let the peer consume the final response before a close
        // with unread request data can cause RST. Keep one synchronous owner;
        // neither queue_work nor a second session close is used here.
        if (fd < 0 || shutdown(fd, SHUT_WR) != 0) {
            const int error = fd < 0 ? EBADF : errno;
            Logger::log(Logger::Warn, "Web", "HTTP close drain: reason=shutdown_failed bytes=0 errno=%d", error);
            return;
        }
        const uint32_t started = millis();
        char discard[1024];
        size_t bytes_since_yield = 0;
        size_t drained = 0;
        size_t progress_bytes = 0;
        uint32_t progress_at = started;
        const char *reason = "byte_limit";
        int socket_error = 0;
        while (drained < kCloseMaxDrainBytes) {
            if (static_cast<uint32_t>(millis() - started) >= kCloseDrainTimeoutMs) {
                reason = "time_limit"; break;
            }
            if (WakePowerGuard::backgroundPaused(millis())) {
                // No app socket I/O while the backlight owner switches,
                // settles and renders. Retain the same SDK session and resume
                // draining afterwards: an early close would lose the reply.
                const uint32_t paused_at = millis();
                activity = {};
                bool unused = false;
                activity = wait_for_web_activity_admission(unused);
                progress_at += static_cast<uint32_t>(millis() - paused_at);
            }
            if (static_cast<uint32_t>(millis() - progress_at) >= kCloseProgressTimeoutMs) {
                reason = "progress_timeout"; break;
            }
            const uint32_t elapsed = static_cast<uint32_t>(millis() - started);
            if (elapsed >= kCloseDrainTimeoutMs) { reason = "time_limit"; break; }
            const uint32_t left = kCloseDrainTimeoutMs - elapsed;
            const uint32_t slice = std::min(left, kCloseRecvSliceMs);
            if (!slice) { reason = "time_limit"; break; }
            timeval timeout{};
            timeout.tv_usec = slice * 1000;
            if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
                socket_error = errno; reason = "socket_option_failed"; break;
            }
            // The SDK won't run req_delete after ESP_FAIL. Drain the socket
            // directly, including bytes beyond a dishonest Content-Length.
            const int count = recv(fd, discard,
                                   std::min(sizeof(discard), kCloseMaxDrainBytes - drained), 0);
            if (count == 0) { reason = "peer_fin"; break; }
            if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                socket_error = errno; reason = "receive_failed"; break;
            }
            // Yield per batch, not per kilobyte: throttling this drain to
            // ~1 MiB/s makes a browser's queued multi-megabyte upload hit the
            // close deadline and lose its response despite a healthy link.
            if (count > 0) {
                const size_t received = static_cast<size_t>(count);
                drained += received;
                progress_bytes += received;
                bytes_since_yield += received;
                if (progress_bytes >= kCloseProgressBytes) {
                    progress_at = millis();
                    progress_bytes = 0;
                }
            }
            if (count < 0 || bytes_since_yield >= 32 * 1024) {
                delay(1);
                bytes_since_yield = 0;
            }
        }
        // No URI, headers or body: rejected uploads may contain private data.
        Logger::log(Logger::Info, "Web",
                    "HTTP close drain: reason=%s bytes=%u elapsed_ms=%lu progress_bytes=%u progress_ms=%lu errno=%d",
                    reason, static_cast<unsigned>(drained),
                    static_cast<unsigned long>(millis() - started),
                    static_cast<unsigned>(progress_bytes),
                    static_cast<unsigned long>(millis() - progress_at), socket_error);
    }

    esp_err_t result() const {
        return close_requested_ || send_capture_.error().terminal ? ESP_FAIL : ESP_OK;
    }

    bool beginStreamResponse(int status_code,
                             const char *content_type,
                             size_t,
                             bool gzip_encoded) override {
        if (!req_ || close_requested_ || send_capture_.error().terminal) {
            if (!req_) send_capture_.failSetup(0, false);
            return false;
        }
        const char *stored_status = nullptr;
        if (!response_storage_.storeStatus(status_line_for_code(status_code), stored_status)) {
            send_capture_.failSetup(0, false);
            return false;
        }
        esp_err_t result = httpd_resp_set_status(req_, stored_status);
        if (result != ESP_OK) {
            send_capture_.failSetup(result);
            return false;
        }
        if (content_type) {
            const char *stored_content_type = nullptr;
            if (!response_storage_.storeContentType(content_type, stored_content_type)) {
                send_capture_.failSetup(0, false);
                return false;
            }
            result = httpd_resp_set_type(req_, stored_content_type);
            if (result != ESP_OK) {
                send_capture_.failSetup(result);
                return false;
            }
        }
        if (gzip_encoded) sendHeader("Content-Encoding", "gzip", false);
        if (send_capture_.error().terminal) return false;
        stream_open_ = true;
        return true;
    }

    int32_t writeStreamChunk(const uint8_t *data, size_t size, int &last_error) override {
        if (!req_ || !stream_open_) {
            if (!send_capture_.error().terminal) send_capture_.failSetup(0, false);
            last_error = send_capture_.error().socket_errno;
            return -1;
        }
        beginWriteBudget();
        const bool ok = send_capture_.chunk(WebStreamStage::Body, data, size, [&]() {
            return httpd_resp_send_chunk(req_, reinterpret_cast<const char *>(data),
                                         static_cast<ssize_t>(size));
        });
        write_budget_active_ = false;
        last_error = send_capture_.error().socket_errno;
        if (!ok) stream_open_ = false;
        return ok ? static_cast<int32_t>(size) : -1;
    }

    bool waitUntilWritable(uint16_t, int &last_error) override {
        last_error = 0;
        return true;
    }

    bool finishStreamResponse(int &last_error) override {
        if (!req_ || !stream_open_) {
            last_error = send_capture_.error().socket_errno;
            return !send_capture_.error().terminal;
        }
        stream_open_ = false;
        beginWriteBudget();
        const bool ok = send_capture_.chunk(WebStreamStage::Finish, nullptr, 0, [&]() {
            return httpd_resp_send_chunk(req_, nullptr, 0);
        });
        write_budget_active_ = false;
        last_error = send_capture_.error().socket_errno;
        if (!ok) stopClient();
        return ok;
    }

    void endStreamResponse() override {
        int ignored = 0;
        finishStreamResponse(ignored);
    }

    WebStreamError streamError() const override { return send_capture_.error(); }
    void streamRequestInfo(WebStreamRequestInfo &info) const override { info = request_info_; }

    int socketSend(int sockfd, const char *buf, size_t size, int flags) {
        const auto raw_send = [&]() {
            if (write_budget_active_) {
                const uint32_t elapsed = millis() - write_budget_started_ms_;
                if (elapsed >= kHttpWriteBudgetMs) { errno = ETIMEDOUT; return -1; }
                const uint32_t remaining = kHttpWriteBudgetMs - elapsed;
                timeval timeout{};
                timeout.tv_sec = remaining / 1000;
                timeout.tv_usec = (remaining % 1000) * 1000;
                if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) return -1;
            }
            return static_cast<int>(::send(sockfd, buf, size, flags));
        };
        const bool matching_request = req_ && httpd_req_to_sockfd(req_) == sockfd;
        const bool captured = matching_request && send_capture_.active();
        const int result = matching_request
            ? send_capture_.socketSend(buf, size, raw_send) : raw_send();
        const int socket_errno = result < 0 ? errno : 0;
        if (matching_request) {
            if (result > 0) response_bytes_sent_ = true;
            if (result < 0) socket_send_failed_ = true;
        }
        if (result >= 0) return result;
        // The hook survives keep-alive reuse. Preserve the SDK warning for
        // ordinary (non-streamed) replies/errors; those have no failure record.
        if (!captured) ESP_LOGW("httpd_txrx", "error in send : %d", socket_errno);
        // Match ESP-IDF's default send error categories; keep the raw errno above.
        switch (socket_errno) {
            case EAGAIN:
            case EINTR: return HTTPD_SOCK_ERR_TIMEOUT;
            case EINVAL:
            case EBADF:
            case EFAULT:
            case ENOTSOCK: return HTTPD_SOCK_ERR_INVALID;
            default: return HTTPD_SOCK_ERR_FAIL;
        }
    }

    WebUpload upload() override {
        return upload_;
    }

    bool uploadDeadlineExceeded(uint32_t now_ms) const {
        return upload_deadline_ms_ != 0 &&
               static_cast<int32_t>(now_ms - upload_deadline_ms_) >= 0;
    }

    bool socketLikelyConnected() const {
        if (!req_) {
            return false;
        }
        return socket_likely_connected(httpd_req_to_sockfd(req_));
    }

    void setUploadAbortReason(WebUploadAbortReason reason) {
        upload_abort_reason_ = reason;
    }

    WebUploadAbortReason uploadAbortReason() const {
        return upload_abort_reason_;
    }

private:
    void beginWriteBudget() {
        write_budget_started_ms_ = millis();
        write_budget_active_ = true;
    }

    uint32_t write_budget_started_ms_ = 0;
    bool write_budget_active_ = false;
    httpd_req_t *req_ = nullptr;
    WebHttpSendCapture send_capture_;
    bool simple_failure_pending_ = false;
    size_t simple_total_ = 0;
    uint32_t simple_duration_ms_ = 0;
    WebStreamRequestInfo request_info_;
    WebHttpResponseStorage::Storage response_storage_{};
    std::vector<WebQueryArg> args_{};
    String raw_body_;
    WebUpload upload_{};
    bool stream_open_ = false;
    bool close_requested_ = false;
    bool response_bytes_sent_ = false;
    bool socket_send_failed_ = false;
    size_t pending_body_bytes_ = 0;
    uint32_t upload_deadline_ms_ = 0;
    WebUploadAbortReason upload_abort_reason_ = WebUploadAbortReason::None;
    bool upload_rejected_ = false;
};

EspHttpServerBackend::EspHttpServerBackend(uint16_t port) : port_(port) {
    routes_ = new std::list<RouteRegistration>();
    request_ = new EspHttpRequest();
}

EspHttpServerBackend::~EspHttpServerBackend() {
    stop();
    delete request_;
    request_ = nullptr;
    delete &backend_routes(routes_);
    routes_ = nullptr;
}

int EspHttpServerBackend::socketSend(int sockfd, const char *buf, size_t size, int flags) {
    return request_->socketSend(sockfd, buf, size, flags);
}

WebRequest &EspHttpServerBackend::request() {
    return *request_;
}

void EspHttpServerBackend::onGet(const char *uri, WebHandlerFn handler) {
    RouteRegistration route{};
    route.uri = uri ? uri : "";
    route.method = HTTP_GET;
    route.handler = handler;
    backend_routes(routes_).push_back(route);
    if (server_handle_) {
        registerRoute(backend_routes(routes_).back());
    }
}

void EspHttpServerBackend::onPost(const char *uri, WebHandlerFn handler) {
    RouteRegistration route{};
    route.uri = uri ? uri : "";
    route.method = HTTP_POST;
    route.handler = handler;
    backend_routes(routes_).push_back(route);
    if (server_handle_) {
        registerRoute(backend_routes(routes_).back());
    }
}

void EspHttpServerBackend::onPostUpload(const char *uri,
                                        WebHandlerFn handler,
                                        WebHandlerFn upload_handler,
                                        WebHandlerFn request_preflight_handler) {
    RouteRegistration route{};
    route.uri = uri ? uri : "";
    route.method = HTTP_POST;
    route.handler = handler;
    route.upload_handler = upload_handler;
    route.request_preflight_handler = request_preflight_handler;
    backend_routes(routes_).push_back(route);
    if (server_handle_) {
        registerRoute(backend_routes(routes_).back());
    }
}

void EspHttpServerBackend::onNotFound(WebHandlerFn handler) {
    not_found_handler_ = handler;
}

const char *EspHttpServerBackend::name() const {
    return "esp_http_server";
}

bool EspHttpServerBackend::registerRoute(RouteRegistration &route) {
    if (!server_handle_) {
        return false;
    }

    route.backend = this;
    route.descriptor = {};
    route.descriptor.uri = route.uri.c_str();
    route.descriptor.method = route.method;
    route.descriptor.handler = esp_route_dispatch;
    route.descriptor.user_ctx = &route;
    const esp_err_t err =
        httpd_register_uri_handler(static_cast<httpd_handle_t>(server_handle_),
                                   &route.descriptor);
    if (err != ESP_OK) {
        LOGW("WEB", "route registration failed (method=%d uri=%s err=%d)",
             static_cast<int>(route.method),
             route.uri.c_str(),
             static_cast<int>(err));
        return false;
    }
    return true;
}

bool EspHttpServerBackend::prepareRequest(RouteRegistration &route, void *raw_req) {
    auto *req = static_cast<httpd_req_t *>(raw_req);
    request_->begin(req);
    request_->appendArgsFromQuery();

    if (route.request_preflight_handler != nullptr) {
        route.request_preflight_handler();
        if (request_->uploadRejected()) {
            request_->setPendingBodyBytes(req->content_len);
            return true;
        }
    }

    if (req->content_len == 0) {
        return true;
    }

    String body;
    String content_type = read_header_value(req, "Content-Type");
    String content_type_lc = content_type;
    content_type_lc.toLowerCase();

    if (route.upload_handler != nullptr) {
        if (content_type_lc.indexOf("multipart/form-data") < 0) {
            request_->sendRejection(400, "Expected multipart/form-data upload");
            return false;
        }

        const String boundary = WebMultipart::parseBoundary(content_type);
        if (boundary.isEmpty()) {
            request_->sendRejection(400, "Missing multipart boundary");
            return false;
        }
        if (!WebServerLimits::multipartBoundarySizeAllowed(boundary.length())) {
            request_->sendRejection(413,
                                         "Multipart boundary is too large");
            return false;
        }

        EspHttpRequest::BufferedBodyReader reader(request_);
        String line;
        const String expected_first_boundary = String("--") + boundary;
        if (!reader.readLine(line) || line != expected_first_boundary) {
            if (reader.limitExceeded()) {
                request_->sendRejection(413,
                                             "Multipart header is too large");
                return false;
            }
            request_->sendRejection(400, "Malformed multipart body");
            return false;
        }

        bool saw_upload_part = false;
        bool final_boundary = false;
        size_t part_count = 0;
        auto abort_started_upload = [&]() {
            if (!saw_upload_part) {
                return false;
            }
            const WebUpload previous = request_->upload();
            request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
            WebUpload aborted{};
            aborted.status = WebUploadStatus::Aborted;
            aborted.abort_reason = request_->uploadAbortReason();
            aborted.filename = previous.filename;
            aborted.totalSize = previous.totalSize;
            request_->setUpload(aborted);
            route.upload_handler();
            return true;
        };
        while (!final_boundary) {
            if (++part_count > WebServerLimits::kMaxMultipartParts) {
                if (abort_started_upload()) {
                    return true;
                }
                request_->sendRejection(413,
                                             "Too many multipart fields");
                return false;
            }
            String field_name;
            String filename;
            bool field_is_file = false;

            while (true) {
                if (!reader.readLine(line)) {
                    if (abort_started_upload()) {
                        return true;
                    }
                    if (reader.limitExceeded()) {
                        request_->sendRejection(413,
                                                     "Multipart header is too large");
                        return false;
                    }
                    request_->sendRejection(400, "Malformed multipart headers");
                    return false;
                }
                if (line.isEmpty()) {
                    break;
                }

                String part_name;
                String part_filename;
                if (WebMultipart::parseContentDisposition(line, part_name, part_filename)) {
                    field_name = part_name;
                    filename = part_filename;
                    field_is_file = !filename.isEmpty();
                }
            }

            if (field_is_file) {
                if (saw_upload_part) {
                    // Upload routes own one streaming file. Converting a
                    // second file part into Aborted guarantees the route's
                    // final handler releases OTA state and admission gates.
                    abort_started_upload();
                    return true;
                }
                saw_upload_part = true;

                WebUpload start{};
                start.status = WebUploadStatus::Start;
                start.filename = filename;
                start.totalSize = 0;
                start.abort_reason = WebUploadAbortReason::None;
                request_->setUpload(start);
                route.upload_handler();
                if (request_->uploadRejected()) {
                    request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
                    return true;
                }

                size_t uploaded_size = 0;
                const bool stream_ok = reader.streamUntilBoundary(
                    boundary,
                    [this, &route, &filename, &uploaded_size](const uint8_t *data, size_t size) {
                        if (size == 0) {
                            return true;
                        }
                        WebUpload write{};
                        write.status = WebUploadStatus::Write;
                        write.filename = filename;
                        write.totalSize = uploaded_size + size;
                        write.currentSize = size;
                        write.buf = const_cast<uint8_t *>(data);
                        request_->setUpload(write);
                        route.upload_handler();
                        uploaded_size += size;
                        return !request_->uploadRejected();
                    },
                    final_boundary);

                if (request_->uploadRejected()) {
                    // A prefix/identity rejection is terminal. Preserve its
                    // reason for the route's response; session close then drains
                    // the unread body within the independent bounded budget.
                    request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
                    return true;
                }
                if (!stream_ok) {
                    request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
                    WebUpload aborted{};
                    aborted.status = WebUploadStatus::Aborted;
                    aborted.abort_reason = request_->uploadAbortReason();
                    aborted.filename = filename;
                    aborted.totalSize = uploaded_size;
                    request_->setUpload(aborted);
                    route.upload_handler();
                    return true;
                }

                WebUpload end{};
                end.status = WebUploadStatus::End;
                end.filename = filename;
                end.totalSize = uploaded_size;
                end.abort_reason = WebUploadAbortReason::None;
                request_->setUpload(end);
                route.upload_handler();
                continue;
            }

            String value;
            bool field_too_large = false;
            bool field_allocation_failed = false;
            const bool part_ok = reader.streamUntilBoundary(
                boundary,
                [&value, &field_too_large, &field_allocation_failed](const uint8_t *data,
                                                                     size_t size) {
                    if (size == 0) {
                        return true;
                    }
                    if (!WebServerLimits::multipartFieldAppendAllowed(value.length(), size)) {
                        field_too_large = true;
                        return false;
                    }
                    if (!value.concat(reinterpret_cast<const char *>(data),
                                      static_cast<unsigned int>(size))) {
                        field_allocation_failed = true;
                        return false;
                    }
                    return true;
                },
                final_boundary);

            if (!part_ok) {
                if (abort_started_upload()) {
                    return true;
                }
                if (field_too_large || field_allocation_failed) {
                    request_->sendRejection(413,
                                                 field_too_large
                                                     ? "Multipart field is too large"
                                                     : "Insufficient memory for multipart field");
                    return false;
                }
                request_->sendRejection(400, "Malformed multipart content");
                return false;
            }

            if (!field_name.isEmpty()) {
                request_->setArg(field_name, value);
            }
        }

        return true;
    }

    const RequestBodyReadResult body_result = read_request_body(req, body);
    if (body_result != RequestBodyReadResult::Ok) {
        if (body_result == RequestBodyReadResult::TooLarge ||
            body_result == RequestBodyReadResult::AllocationFailed) {
            request_->sendRejection(413,
                                         body_result == RequestBodyReadResult::TooLarge
                                             ? "Request body is too large"
                                             : "Insufficient memory for request body");
        } else {
            request_->sendRejection(400, "Failed to read request body");
        }
        return false;
    }

    if (content_type_lc.indexOf("application/x-www-form-urlencoded") >= 0) {
        request_->appendArgsFromFormBody(body);
    } else if (!body.isEmpty()) {
        request_->setRawBody(body);
    }

    return true;
}

void EspHttpServerBackend::resetRequest() {
    if (request_) {
        request_->reset();
    }
}

esp_err_t EspHttpServerBackend::rejectRequest(void *raw_req, int status, const char *message, WakePowerGuard::Activity &activity) {
    request_->begin(static_cast<httpd_req_t *>(raw_req));
    request_->sendRejection(status, message);
    return finalizeRequest(activity, true);
}

esp_err_t EspHttpServerBackend::finalizeRequest(WakePowerGuard::Activity &activity, bool rejected) {
    if (!request_) return ESP_FAIL;
    if (rejected) request_->stopClient();
    request_->endStreamResponse();
    const esp_err_t result = request_->result();
    if (result != ESP_OK) request_->prepareSessionClose(activity);
    request_->reset();
    return result;
}

bool EspHttpServerBackend::hasNotFoundHandler() const {
    return not_found_handler_ != nullptr;
}

void EspHttpServerBackend::dispatchError(void *raw_req, httpd_err_code_t error) {
    if (!request_ || (error != HTTPD_405_METHOD_NOT_ALLOWED && !not_found_handler_)) {
        return;
    }
    auto *req = static_cast<httpd_req_t *>(raw_req);
    request_->begin(req);
    request_->appendArgsFromQuery();
    if (error == HTTPD_405_METHOD_NOT_ALLOWED) {
        // HEAD must never carry the SDK's default error body. Close after a
        // method rejection, including requests with an unread POST body.
        request_->sendHeader("Connection", "close", false);
        request_->send(405, "text/plain", "");
        request_->stopClient();
    } else {
        not_found_handler_();
    }
}

void EspHttpServerBackend::begin() {
    if (server_handle_) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 12288;
    // Keep HTTP handlers on the network core instead of allowing an
    // unpinned/FPU-using server task to settle on the Arduino/LVGL core.
    config.core_id = 0;
    config.max_open_sockets = kHttpServerMaxOpenSockets;
    config.lru_purge_enable = true;
    config.max_uri_handlers = WebServerLimits::kHttpServerMaxUriHandlers;
    config.max_resp_headers = WebHttpResponseStorage::Storage::kMaxHeaders;
    config.global_user_ctx = this;
    // The SDK borrows this pointer. Its default nullptr deleter calls free(),
    // but the C++ backend is owned by the application's unique_ptr.
    config.global_user_ctx_free_fn = [](void *) {};
    // SDK remains the sole session owner. Its synchronous close hook also
    // covers a wake arriving between dispatch return and session deletion.
    config.close_fn = [](httpd_handle_t, int fd) {
        bool unused = false;
        auto activity = wait_for_web_activity_admission(unused);
        close(fd);
    };
    config.uri_match_fn = nullptr;
    // Keep the per-recv timeout moderate and let the multipart reader own
    // the longer no-progress budget so a single socket timeout does not
    // immediately abort OTA.
    config.recv_wait_timeout = kHttpServerRecvWaitTimeoutS;
    config.send_wait_timeout = kHttpServerSendWaitTimeoutS;

    httpd_handle_t handle = nullptr;
    if (httpd_start(&handle, &config) != ESP_OK) {
        return;
    }
    server_handle_ = handle;

    for (RouteRegistration &route : backend_routes(routes_)) {
        registerRoute(route);
    }
    httpd_register_err_handler(handle, HTTPD_404_NOT_FOUND, esp_not_found_dispatch);
    httpd_register_err_handler(handle, HTTPD_405_METHOD_NOT_ALLOWED, esp_not_found_dispatch);
}

void EspHttpServerBackend::stop() {
    if (!server_handle_) {
        return;
    }
    httpd_stop(static_cast<httpd_handle_t>(server_handle_));
    server_handle_ = nullptr;
}

static esp_err_t esp_route_dispatch(httpd_req_t *req) {
    auto *route = static_cast<EspHttpServerBackend::RouteRegistration *>(req ? req->user_ctx : nullptr);
    if (!route || !route->backend || !route->handler) {
        return ESP_FAIL;
    }

    bool wake_wait_exceeded = false;
    WakePowerGuard::Activity wake_activity =
        wait_for_web_activity_admission(wake_wait_exceeded);
    if (wake_wait_exceeded) {
        // Even the timeout response is sent only after acquiring an activity
        // lease. It therefore cannot overlap the protected CH422G switch.
        return route->backend->rejectRequest(req, 503, "Display wake in progress", wake_activity);
    }

    if (!route->backend->prepareRequest(*route, req)) {
        // Preserve the error reply before the SDK closes the current session.
        return route->backend->finalizeRequest(wake_activity, true);
    }

    route->handler();
    return route->backend->finalizeRequest(wake_activity);
}

static esp_err_t esp_not_found_dispatch(httpd_req_t *req, httpd_err_code_t error) {
    if (!req || !req->handle) {
        return ESP_FAIL;
    }

    bool wake_wait_exceeded = false;
    WakePowerGuard::Activity wake_activity =
        wait_for_web_activity_admission(wake_wait_exceeded);
    if (wake_wait_exceeded) {
        auto *backend = static_cast<EspHttpServerBackend *>(httpd_get_global_user_ctx(req->handle));
        return backend ? backend->rejectRequest(req, 503, "Display wake in progress", wake_activity) : ESP_FAIL;
    }

    auto *backend =
        static_cast<EspHttpServerBackend *>(httpd_get_global_user_ctx(req->handle));
    if (!backend || (error != HTTPD_405_METHOD_NOT_ALLOWED && !backend->hasNotFoundHandler())) {
        return httpd_resp_send_404(req);
    }

    backend->dispatchError(req, error);
    return backend->finalizeRequest(wake_activity);
}

std::unique_ptr<WebServerBackend> createDefaultWebServerBackend(uint16_t port) {
    return std::unique_ptr<WebServerBackend>(new EspHttpServerBackend(port));
}
