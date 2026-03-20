// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebTransport.h"
#include "web/WebMultipart.h"
#include "web/WebQueryString.h"

#include <algorithm>
#include <errno.h>
#include <list>
#include <memory>
#include <vector>

#include <esp_http_server.h>

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
        explicit BufferedBodyReader(httpd_req_t *req)
            : req_(req), remaining_(req && req->content_len > 0 ? static_cast<size_t>(req->content_len) : 0) {
            buffer_.reserve(2048);
        }

        bool readLine(String &line) {
            while (true) {
                auto it = std::search(buffer_.begin(),
                                      buffer_.end(),
                                      kCrLf,
                                      kCrLf + 2);
                if (it != buffer_.end()) {
                    const size_t line_len = static_cast<size_t>(std::distance(buffer_.begin(), it));
                    line = String(reinterpret_cast<const char *>(buffer_.data()), line_len);
                    buffer_.erase(buffer_.begin(), it + 2);
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
            const size_t to_read = remaining_ < sizeof(chunk) ? remaining_ : sizeof(chunk);
            const int received = httpd_req_recv(req_, chunk, to_read);
            if (received <= 0) {
                return false;
            }

            buffer_.insert(buffer_.end(), chunk, chunk + received);
            remaining_ -= static_cast<size_t>(received);
            return true;
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

        httpd_req_t *req_ = nullptr;
        size_t remaining_ = 0;
        std::vector<uint8_t> buffer_{};
        static constexpr char kCrLf[2] = {'\r', '\n'};
    };

    void begin(httpd_req_t *req) {
        req_ = req;
        args_.clear();
        raw_body_ = "";
        upload_ = {};
        stream_open_ = false;
    }

    void reset() {
        req_ = nullptr;
        args_.clear();
        raw_body_ = "";
        upload_ = {};
        stream_open_ = false;
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
        return req_ != nullptr;
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

private:
    httpd_req_t *req_ = nullptr;
    std::vector<WebQueryArg> args_{};
    String raw_body_;
    WebUpload upload_{};
    bool stream_open_ = false;
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
    return httpd_register_uri_handler(static_cast<httpd_handle_t>(server_handle_),
                                      &route.descriptor) == ESP_OK;
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

        EspHttpRequest::BufferedBodyReader reader(req);
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
                        WebUpload aborted{};
                        aborted.status = WebUploadStatus::Aborted;
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
                request_->setUpload(start);
                route.upload_handler();

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
                    WebUpload aborted{};
                    aborted.status = WebUploadStatus::Aborted;
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
                    WebUpload aborted{};
                    aborted.status = WebUploadStatus::Aborted;
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
    config.max_uri_handlers = 32;
    config.max_resp_headers = 16;
    config.global_user_ctx = this;
    config.global_user_ctx_free_fn = nullptr;
    config.uri_match_fn = nullptr;

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
