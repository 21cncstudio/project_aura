// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebTransport.h"
#include "web/WebMultipart.h"
#include "web/WebQueryString.h"

#include <algorithm>
#include <cstring>
#include <errno.h>
#include <list>
#include <memory>
#include <vector>

#include <esp_http_server.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>

#include "core/Logger.h"
#include "core/Watchdog.h"
#include "web/WebServerLimits.h"

namespace {

constexpr uint16_t kHttpServerRecvWaitTimeoutS = 10;
constexpr uint16_t kHttpServerSendWaitTimeoutS = 30;
constexpr uint32_t kMultipartReadIdleTimeoutMs = 90UL * 1000UL;
constexpr uint32_t kDrainRecvTimeoutMs = 200;

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
    void onPostUpload(const char *uri, WebHandlerFn handler, WebHandlerFn upload_handler) override;
    void onNotFound(WebHandlerFn handler) override;
    const char *name() const override;
    void begin() override;
    void stop() override;

    bool prepareRequest(RouteRegistration &route, void *req);
    void resetRequest();
    void finalizeRequest();
    bool hasNotFoundHandler() const;
    void dispatchNotFound(void *req);

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

bool read_request_body(httpd_req_t *req, String &out) {
    if (!req || req->content_len == 0) {
        out = "";
        return true;
    }

    out = "";
    out.reserve(req->content_len);
    size_t remaining = req->content_len;
    char buffer[512];
    while (remaining > 0) {
        const size_t chunk_size = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const int received = httpd_req_recv(req, buffer, chunk_size);
        if (received <= 0) {
            return false;
        }
        out.concat(buffer, static_cast<unsigned int>(received));
        remaining -= static_cast<size_t>(received);
    }
    return true;
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
              remaining_(req_ && req_->content_len > 0 ? static_cast<size_t>(req_->content_len) : 0),
              buffer_(allocateBuffer()) {}

        ~BufferedBodyReader() {
            if (buffer_) {
                heap_caps_free(buffer_);
            }
        }

        BufferedBodyReader(const BufferedBodyReader &) = delete;
        BufferedBodyReader &operator=(const BufferedBodyReader &) = delete;

        size_t remainingBytesOnSocket() const {
            return remaining_;
        }

        bool readLine(String &line) {
            if (!buffer_) {
                if (request_) {
                    request_->setUploadAbortReason(WebUploadAbortReason::SocketError);
                }
                return false;
            }

            while (true) {
                const uint8_t *begin = bufferData();
                const uint8_t *end = begin + buffer_size_;
                auto it = std::search(begin,
                                      end,
                                      kCrLf,
                                      kCrLf + 2);
                if (it != end) {
                    const size_t line_len = static_cast<size_t>(std::distance(begin, it));
                    line = String(reinterpret_cast<const char *>(begin), line_len);
                    consume(line_len + 2);
                    return true;
                }
                if (!receiveMore()) {
                    return false;
                }
            }
        }

        template <typename Callback>
        bool streamUntilBoundary(const String &boundary, Callback callback, bool &final_boundary) {
            final_boundary = false;
            if (!buffer_) {
                if (request_) {
                    request_->setUploadAbortReason(WebUploadAbortReason::SocketError);
                }
                return false;
            }

            const String marker_string = String("\r\n--") + boundary;
            const auto *marker = reinterpret_cast<const uint8_t *>(marker_string.c_str());
            const size_t marker_size = marker_string.length();

            while (true) {
                const uint8_t *begin = bufferData();
                const uint8_t *end = begin + buffer_size_;
                auto it = std::search(begin, end, marker, marker + marker_size);
                if (it != end) {
                    const size_t data_len = static_cast<size_t>(std::distance(begin, it));
                    if (data_len > 0 && !callback(begin, data_len)) {
                        return false;
                    }
                    consume(data_len + marker_size);
                    return consumeBoundarySuffix(final_boundary);
                }

                if (remaining_ == 0) {
                    return false;
                }

                const size_t keep_bytes = marker_size > 1 ? marker_size - 1 : 0;
                if (buffer_size_ > keep_bytes) {
                    const size_t flush_len = buffer_size_ - keep_bytes;
                    if (flush_len > 0 && !callback(bufferData(), flush_len)) {
                        return false;
                    }
                    consume(flush_len);
                }

                if (!receiveMore()) {
                    return false;
                }
            }
        }

    private:
        static uint8_t *allocateBuffer() {
            auto *buffer = static_cast<uint8_t *>(
                heap_caps_malloc(kBufferCapacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (buffer) {
                return buffer;
            }

            LOGW("Web", "multipart internal buffer allocation failed, using generic 8-bit heap");
            return static_cast<uint8_t *>(heap_caps_malloc(kBufferCapacity, MALLOC_CAP_8BIT));
        }

        uint8_t *bufferData() {
            return buffer_ ? buffer_ + buffer_head_ : nullptr;
        }

        const uint8_t *bufferData() const {
            return buffer_ ? buffer_ + buffer_head_ : nullptr;
        }

        size_t writableTailBytes() const {
            return buffer_ ? kBufferCapacity - (buffer_head_ + buffer_size_) : 0;
        }

        void compactBuffer() {
            if (!buffer_ || buffer_head_ == 0) {
                return;
            }
            if (buffer_size_ > 0) {
                memmove(buffer_, buffer_ + buffer_head_, buffer_size_);
            }
            buffer_head_ = 0;
        }

        void consume(size_t count) {
            if (count >= buffer_size_) {
                buffer_head_ = 0;
                buffer_size_ = 0;
                return;
            }

            buffer_head_ += count;
            buffer_size_ -= count;
            if (buffer_head_ > kBufferCapacity / 2 && buffer_head_ > buffer_size_) {
                compactBuffer();
            }
        }

        bool receiveMore() {
            if (!req_ || remaining_ == 0 || !buffer_) {
                if (request_ && !buffer_) {
                    request_->setUploadAbortReason(WebUploadAbortReason::SocketError);
                }
                return false;
            }

            while (remaining_ > 0) {
                if (writableTailBytes() == 0) {
                    compactBuffer();
                }
                const size_t writable = writableTailBytes();
                if (writable == 0) {
                    if (request_) {
                        request_->setUploadAbortReason(WebUploadAbortReason::SocketError);
                    }
                    return false;
                }

                const uint32_t now_ms = millis();
                if (request_ && request_->uploadDeadlineExceeded(now_ms)) {
                    request_->setUploadAbortReason(WebUploadAbortReason::TotalTimeout);
                    return false;
                }

                const size_t to_read = std::min(std::min(remaining_, kRecvChunkSize), writable);
                const int received =
                    httpd_req_recv(req_,
                                   reinterpret_cast<char *>(buffer_ + buffer_head_ + buffer_size_),
                                   to_read);
                if (received > 0) {
                    timeout_window_active_ = false;
                    timeout_window_start_ms_ = 0;
                    buffer_size_ += static_cast<size_t>(received);
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
            while (buffer_size_ < count) {
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

            if (bufferData()[0] == '-' && bufferData()[1] == '-') {
                final_boundary = true;
                consume(2);
                if (buffer_size_ < 2 && remaining_ > 0) {
                    receiveMore();
                }
                if (buffer_size_ >= 2 && bufferData()[0] == '\r' && bufferData()[1] == '\n') {
                    consume(2);
                }
                return true;
            }

            if (bufferData()[0] == '\r' && bufferData()[1] == '\n') {
                final_boundary = false;
                consume(2);
                return true;
            }

            return false;
        }

        EspHttpRequest *request_ = nullptr;
        httpd_req_t *req_ = nullptr;
        size_t remaining_ = 0;
        uint8_t *buffer_ = nullptr;
        size_t buffer_head_ = 0;
        size_t buffer_size_ = 0;
        bool timeout_window_active_ = false;
        uint32_t timeout_window_start_ms_ = 0;
        static constexpr size_t kBufferCapacity = 4096;
        static constexpr size_t kRecvChunkSize = 2048;
        static constexpr uint8_t kCrLf[2] = {'\r', '\n'};
    };

    void begin(httpd_req_t *req) {
        req_ = req;
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
        req_ = nullptr;
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
        return (req_ && req_->uri) ? String(req_->uri) : String();
    }

    void sendHeader(const char *name, const String &value, bool) override {
        if (req_ && name) {
            httpd_resp_set_hdr(req_, name, value.c_str());
        }
    }

    void send(int status_code, const char *content_type, const String &content) override {
        send(status_code, content_type, content.c_str());
    }

    void send(int status_code, const char *content_type, const char *content) override {
        if (!req_) {
            return;
        }
        const String status = status_line_for_code(status_code);
        httpd_resp_set_status(req_, status.c_str());
        if (content_type) {
            httpd_resp_set_type(req_, content_type);
        }
        httpd_resp_send(req_, content ? content : "", HTTPD_RESP_USE_STRLEN);
    }

    bool clientConnected() const override {
        return socketLikelyConnected();
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
            if (static_cast<int32_t>(millis() - deadline_ms) >= 0) {
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
        if (!req_) {
            return;
        }
        const int sockfd = httpd_req_to_sockfd(req_);
        if (sockfd >= 0) {
            httpd_sess_trigger_close(req_->handle, sockfd);
        }
    }

    bool beginStreamResponse(int status_code,
                             const char *content_type,
                             size_t,
                             bool gzip_encoded) override {
        if (!req_) {
            return false;
        }
        const String status = status_line_for_code(status_code);
        httpd_resp_set_status(req_, status.c_str());
        if (content_type) {
            httpd_resp_set_type(req_, content_type);
        }
        if (gzip_encoded) {
            httpd_resp_set_hdr(req_, "Content-Encoding", "gzip");
        }
        stream_open_ = true;
        return true;
    }

    int32_t writeStreamChunk(const uint8_t *data, size_t size, int &last_error) override {
        if (!req_ || !stream_open_) {
            last_error = EBADF;
            return -1;
        }
        const esp_err_t err =
            httpd_resp_send_chunk(req_, reinterpret_cast<const char *>(data), static_cast<ssize_t>(size));
        if (err != ESP_OK) {
            last_error = EIO;
            return -1;
        }
        last_error = 0;
        return static_cast<int32_t>(size);
    }

    bool waitUntilWritable(uint16_t, int &last_error) override {
        last_error = 0;
        return true;
    }

    void endStreamResponse() override {
        if (!req_ || !stream_open_) {
            return;
        }
        httpd_resp_send_chunk(req_, nullptr, 0);
        stream_open_ = false;
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
    httpd_req_t *req_ = nullptr;
    std::vector<WebQueryArg> args_{};
    String raw_body_;
    WebUpload upload_{};
    bool stream_open_ = false;
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
                                        WebHandlerFn upload_handler) {
    RouteRegistration route{};
    route.uri = uri ? uri : "";
    route.method = HTTP_POST;
    route.handler = handler;
    route.upload_handler = upload_handler;
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

    if (req->content_len == 0) {
        return true;
    }

    String body;
    String content_type = read_header_value(req, "Content-Type");
    String content_type_lc = content_type;
    content_type_lc.toLowerCase();

    if (route.upload_handler != nullptr) {
        if (content_type_lc.indexOf("multipart/form-data") < 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected multipart/form-data upload");
            return false;
        }

        const String boundary = WebMultipart::parseBoundary(content_type);
        if (boundary.isEmpty()) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing multipart boundary");
            return false;
        }

        EspHttpRequest::BufferedBodyReader reader(request_);
        String line;
        const String expected_first_boundary = String("--") + boundary;
        if (!reader.readLine(line) || line != expected_first_boundary) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed multipart body");
            return false;
        }

        bool saw_upload_part = false;
        bool final_boundary = false;
        while (!final_boundary) {
            String field_name;
            String filename;
            bool field_is_file = false;

            while (true) {
                if (!reader.readLine(line)) {
                    if (saw_upload_part) {
                        request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
                        WebUpload aborted{};
                        aborted.status = WebUploadStatus::Aborted;
                        aborted.abort_reason = request_->uploadAbortReason();
                        request_->setUpload(aborted);
                        route.upload_handler();
                        return true;
                    }
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed multipart headers");
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
                        return true;
                    },
                    final_boundary);

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
            const bool part_ok = reader.streamUntilBoundary(
                boundary,
                [&value](const uint8_t *data, size_t size) {
                    if (size == 0) {
                        return true;
                    }
                    value.concat(reinterpret_cast<const char *>(data), static_cast<unsigned int>(size));
                    return true;
                },
                final_boundary);

            if (!part_ok) {
                if (saw_upload_part) {
                    request_->setPendingBodyBytes(reader.remainingBytesOnSocket());
                    WebUpload aborted{};
                    aborted.status = WebUploadStatus::Aborted;
                    aborted.abort_reason = request_->uploadAbortReason();
                    request_->setUpload(aborted);
                    route.upload_handler();
                    return true;
                }
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed multipart content");
                return false;
            }

            if (!field_name.isEmpty()) {
                request_->setArg(field_name, value);
            }
        }

        return true;
    }

    if (!read_request_body(req, body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request body");
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

void EspHttpServerBackend::finalizeRequest() {
    if (request_) {
        request_->endStreamResponse();
        request_->reset();
    }
}

bool EspHttpServerBackend::hasNotFoundHandler() const {
    return not_found_handler_ != nullptr;
}

void EspHttpServerBackend::dispatchNotFound(void *raw_req) {
    if (!request_ || !not_found_handler_) {
        return;
    }
    auto *req = static_cast<httpd_req_t *>(raw_req);
    request_->begin(req);
    request_->appendArgsFromQuery();
    not_found_handler_();
}

void EspHttpServerBackend::begin() {
    if (server_handle_) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 12288;
    config.max_uri_handlers = WebServerLimits::kHttpServerMaxUriHandlers;
    config.max_resp_headers = 16;
    config.global_user_ctx = this;
    config.global_user_ctx_free_fn = nullptr;
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

    if (!route->backend->prepareRequest(*route, req)) {
        route->backend->resetRequest();
        return ESP_OK;
    }

    route->handler();
    route->backend->finalizeRequest();
    return ESP_OK;
}

static esp_err_t esp_not_found_dispatch(httpd_req_t *req, httpd_err_code_t) {
    if (!req || !req->handle) {
        return ESP_FAIL;
    }

    auto *backend =
        static_cast<EspHttpServerBackend *>(httpd_get_global_user_ctx(req->handle));
    if (!backend || !backend->hasNotFoundHandler()) {
        return httpd_resp_send_404(req);
    }

    backend->dispatchNotFound(req);
    backend->finalizeRequest();
    return ESP_OK;
}

std::unique_ptr<WebServerBackend> createDefaultWebServerBackend(uint16_t port) {
    return std::unique_ptr<WebServerBackend>(new EspHttpServerBackend(port));
}
