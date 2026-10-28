#include <unity.h>

#include <deque>
#include <errno.h>
#include <cstring>
#include "web/WebHttpSendCapture.h"
#include <vector>

#include "web/WebResponseUtils.h"

namespace {

struct FakeRuntime {
    uint32_t now_ms = 0;
    uint32_t wifi_sta_connected_elapsed_ms = 0;
    uint32_t environment_captures = 0;
};

uint32_t fake_now_ms(void *context) {
    return static_cast<FakeRuntime *>(context)->now_ms;
}

uint32_t fake_wifi_sta_connected_elapsed_ms(void *context) {
    return static_cast<FakeRuntime *>(context)->wifi_sta_connected_elapsed_ms;
}

struct WriteAction {
    int32_t result = 0;
    int err = 0;
    uint32_t advance_ms = 0;
    bool connected_after = true;
};

struct HeaderValue {
    String name;
    String value;
    bool first = false;
};

class FakeRequest final : public WebRequest {
public:
    explicit FakeRequest(FakeRuntime &runtime) : runtime_(runtime) {}

    bool hasArg(const char *) const override { return false; }
    String arg(const char *) const override { return ""; }
    String uri() const override { return "/test?token=must-not-appear"; }

    void sendHeader(const char *name, const String &value, bool first = false) override {
        headers_.push_back({String(name), value, first});
    }

    void send(int status_code, const char *content_type, const String &content) override {
        sent_status_code_ = status_code;
        sent_content_type_ = content_type;
        sent_content_ = content;
    }

    void send(int status_code, const char *content_type, const char *content) override {
        sent_status_code_ = status_code;
        sent_content_type_ = content_type;
        sent_content_ = content;
    }

    bool clientConnected() const override { return connected_; }
    void setUploadDeadlineMs(uint32_t) override {}
    void clearUploadDeadline() override {}
    void rejectUpload() override {}
    bool uploadRejected() const override { return false; }
    size_t pendingRequestBodyBytes() const override { return 0; }
    size_t drainPendingRequestBody(size_t, uint32_t) override { return 0; }

    void stopClient() override {
        stop_called_ = true;
        connected_ = false;
    }

    bool beginStreamResponse(int status_code,
                             const char *content_type,
                             size_t content_length,
                             bool gzip_encoded = false) override {
        begin_called_ = true;
        begin_status_code_ = status_code;
        begin_content_type_ = content_type;
        begin_content_length_ = content_length;
        begin_gzip_ = gzip_encoded;
        return begin_ok;
    }

    int32_t writeStreamChunk(const uint8_t *, size_t, int &last_error) override {
        ++write_calls;
        if (writes_.empty()) {
            last_error = 0;
            return 0;
        }
        const WriteAction action = writes_.front();
        writes_.pop_front();
        runtime_.now_ms += action.advance_ms;
        last_error = action.err;
        connected_ = action.connected_after;
        return action.result;
    }

    bool waitUntilWritable(uint16_t, int &last_error) override {
        last_error = 0;
        return true;
    }

    void endStreamResponse() override {
        end_called_ = true;
    }

    bool finishStreamResponse(int &last_error) override {
        end_called_ = true;
        runtime_.now_ms += finish_delay_ms;
        if (!finish_ok) {
            error.stage = WebStreamStage::Finish;
            error.sdk_error = 0xB006;
            error.sdk_error_valid = true;
            error.socket_errno = EPIPE;
            error.errno_valid = true;
            error.terminal = true;
        }
        last_error = error.socket_errno;
        return finish_ok;
    }
    WebStreamError streamError() const override { return error; }
    WebUpload upload() override { return {}; }
    bool begin_ok = true;
    bool finish_ok = true;
    uint32_t finish_delay_ms = 0;
    uint32_t write_calls = 0;
    WebStreamError error{};

    void enqueueWrite(const WriteAction &action) {
        writes_.push_back(action);
    }

    String headerValue(const char *name) const {
        for (const HeaderValue &header : headers_) {
            if (header.name == name) {
                return header.value;
            }
        }
        return "";
    }

    bool beginCalled() const { return begin_called_; }
    bool endCalled() const { return end_called_; }
    int beginStatusCode() const { return begin_status_code_; }
    const String &beginContentType() const { return begin_content_type_; }
    size_t beginContentLength() const { return begin_content_length_; }
    bool beginGzip() const { return begin_gzip_; }
    bool stopCalled() const { return stop_called_; }

private:
    FakeRuntime &runtime_;
    std::deque<WriteAction> writes_;
    std::vector<HeaderValue> headers_;
    bool connected_ = true;
    bool stop_called_ = false;
    bool begin_called_ = false;
    bool end_called_ = false;
    int begin_status_code_ = 0;
    String begin_content_type_;
    size_t begin_content_length_ = 0;
    bool begin_gzip_ = false;
    int sent_status_code_ = 0;
    String sent_content_type_;
    String sent_content_;
};

WebResponseUtils::StreamContext make_context(FakeRuntime &runtime, WebStreamState &state) {
    WebResponseUtils::StreamContext context;
    context.context = &runtime;
    context.stream_state = &state;
    static WebStreamRuntime stream_runtime = {
        nullptr,
        fake_now_ms,
        nullptr,
        nullptr,
        nullptr,
        0,
    };
    stream_runtime.context = &runtime;
    context.stream_runtime = &stream_runtime;
    context.slow_write_warn_ms = 200;
    context.nowMs = fake_now_ms;
    context.wifiStaConnectedElapsedMs = fake_wifi_sta_connected_elapsed_ms;
    context.captureFailureEnvironment = [](void *ctx, WebStreamEnvironment &out) {
        ++static_cast<FakeRuntime *>(ctx)->environment_captures;
        out.memory_valid = true;
        out.internal_free = 60000;
        out.internal_min_free = 50000;
        out.network_valid = true;
        out.network_age_ms = 125;
        out.wifi_connected = true;
        out.mqtt_enabled = true;
        out.mqtt_connected = false;
    };
    return context;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_send_html_stream_records_result_and_headers() {
    FakeRuntime runtime;
    WebStreamState state;
    FakeRequest request(runtime);
    request.enqueueWrite({2, 0, 3, true});
    request.enqueueWrite({3, 0, 4, true});
    const WebResponseUtils::StreamContext context = make_context(runtime, state);

    const bool ok = WebResponseUtils::sendHtmlStream(request, "hello", context);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(request.beginCalled());
    TEST_ASSERT_TRUE(request.endCalled());
    TEST_ASSERT_EQUAL(200, request.beginStatusCode());
    TEST_ASSERT_EQUAL_STRING("text/html; charset=utf-8", request.beginContentType().c_str());
    TEST_ASSERT_EQUAL_UINT32(5, static_cast<uint32_t>(request.beginContentLength()));
    TEST_ASSERT_EQUAL_STRING("no-store, no-cache, must-revalidate, max-age=0",
                             request.headerValue("Cache-Control").c_str());
    TEST_ASSERT_FALSE(request.stopCalled());
    TEST_ASSERT_FALSE(WebResponseUtils::shouldPauseMqttForTransfer(context));

    const WebTransferSnapshot snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.ok_count);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.stats.abort_count);
    TEST_ASSERT_EQUAL_UINT32(5, static_cast<uint32_t>(snapshot.stats.last_total));
    TEST_ASSERT_EQUAL_UINT32(5, static_cast<uint32_t>(snapshot.stats.last_sent));
    TEST_ASSERT_EQUAL_STRING("/test", snapshot.stats.last_uri.c_str());
}

void test_send_html_stream_resilient_sets_pause_window() {
    FakeRuntime runtime;
    runtime.wifi_sta_connected_elapsed_ms = 5000;
    WebStreamState state;
    FakeRequest request(runtime);
    request.enqueueWrite({5, 0, 2, true});
    const WebResponseUtils::StreamContext context = make_context(runtime, state);

    const bool ok = WebResponseUtils::sendHtmlStreamResilient(request, "hello", context);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(request.beginCalled());
    TEST_ASSERT_TRUE(request.endCalled());
    TEST_ASSERT_TRUE(WebResponseUtils::shouldPauseMqttForTransfer(context));

    const WebTransferSnapshot snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.ok_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0, snapshot.mqtt_pause_remaining_ms);
}

void test_send_progmem_asset_sets_immutable_cache_headers() {
    FakeRuntime runtime;
    WebStreamState state;
    FakeRequest request(runtime);
    const uint8_t asset[] = {1, 2, 3};
    request.enqueueWrite({3, 0, 1, true});
    const WebResponseUtils::StreamContext context = make_context(runtime, state);

    const bool ok = WebResponseUtils::sendProgmemAsset(request,
                                                       "text/css; charset=utf-8",
                                                       asset,
                                                       sizeof(asset),
                                                       true,
                                                       WebResponseUtils::AssetCacheMode::Immutable,
                                                       context);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(request.beginCalled());
    TEST_ASSERT_TRUE(request.endCalled());
    TEST_ASSERT_TRUE(request.beginGzip());
    TEST_ASSERT_EQUAL_STRING("public, max-age=31536000, immutable",
                             request.headerValue("Cache-Control").c_str());

    const WebTransferSnapshot snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.ok_count);
    TEST_ASSERT_TRUE(WebResponseUtils::shouldPauseMqttForTransfer(context));
}

void test_finish_failure_is_recorded_even_after_all_body_bytes_or_empty_body() {
    for (const char *body : {"hello", ""}) {
        FakeRuntime runtime;
        WebStreamState state;
        FakeRequest request(runtime);
        request.finish_ok = false;
        request.finish_delay_ms = 500;
        if (*body) request.enqueueWrite({5, 0, 10, true});
        const auto context = make_context(runtime, state);
        TEST_ASSERT_FALSE(WebResponseUtils::sendHtmlStream(request, body, context));
        TEST_ASSERT_TRUE(request.stopCalled());
        const auto snapshot = state.snapshot(runtime.now_ms);
        TEST_ASSERT_EQUAL_UINT32(0, snapshot.stats.ok_count);
        TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.abort_count);
        TEST_ASSERT_EQUAL_UINT32(1, snapshot.failure_count);
        TEST_ASSERT_EQUAL(WebStreamStage::Finish, snapshot.failures[0].error.stage);
        TEST_ASSERT_EQUAL(StreamAbortReason::Disconnected, snapshot.failures[0].reason);
        TEST_ASSERT_EQUAL_UINT32(strlen(body), snapshot.failures[0].sent);
        TEST_ASSERT_EQUAL_UINT32(500, snapshot.failures[0].max_write_ms);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(500, snapshot.failures[0].duration_ms);
        TEST_ASSERT_EQUAL_STRING("/test", snapshot.failures[0].request.path);
        TEST_ASSERT_EQUAL_UINT32(1, runtime.environment_captures);
        TEST_ASSERT_EQUAL_UINT32(60000, snapshot.failures[0].environment.internal_free);
    }
}

void test_partial_http_timeout_is_not_replayed_or_finished() {
    FakeRuntime runtime;
    WebStreamState state;
    FakeRequest request(runtime);
    request.error.terminal = true;
    request.error.errno_valid = true;
    request.error.socket_errno = EAGAIN;
    request.error.sdk_error = 0xB006;
    request.error.sdk_error_valid = true;
    request.error.stage = WebStreamStage::Body;
    request.error.partial_body_bytes = 2;
    request.enqueueWrite({-1, EAGAIN, 30000, true});
    request.enqueueWrite({5, 0, 1, true}); // Must not run.
    const auto context = make_context(runtime, state);
    TEST_ASSERT_FALSE(WebResponseUtils::sendHtmlStream(request, "hello", context));
    TEST_ASSERT_EQUAL_UINT32(1, request.write_calls);
    TEST_ASSERT_FALSE(request.endCalled());
    const auto snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL(StreamAbortReason::SocketTimeout, snapshot.failures[0].reason);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.failures[0].sent);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.failures[0].error.partial_body_bytes);
    TEST_ASSERT_EQUAL_UINT32(30000, snapshot.failures[0].duration_ms);
}

void test_setup_failure_stops_before_body_and_success_preserves_history() {
    FakeRuntime runtime;
    WebStreamState state;
    FakeRequest request(runtime);
    request.begin_ok = false;
    const auto context = make_context(runtime, state);
    TEST_ASSERT_FALSE(WebResponseUtils::sendHtmlStream(request, "hello", context));
    TEST_ASSERT_EQUAL_UINT32(0, request.write_calls);
    TEST_ASSERT_FALSE(request.endCalled());
    TEST_ASSERT_EQUAL_UINT32(1, runtime.environment_captures);
    FakeRequest success(runtime);
    success.enqueueWrite({5, 0, 250, true}); // Slow but complete, no failure snapshot.
    TEST_ASSERT_TRUE(WebResponseUtils::sendHtmlStream(success, "hello", context));
    const auto snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.ok_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.abort_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.slow_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.failure_count);
    TEST_ASSERT_EQUAL(StreamAbortReason::InternalError, snapshot.failures[0].reason);
    TEST_ASSERT_EQUAL_UINT32(1, runtime.environment_captures);
}

void test_repeated_failure_logs_do_not_hide_abort_counts() {
    FakeRuntime runtime;
    WebStreamState state;
    const auto context = make_context(runtime, state);
    for (int i = 0; i < 3; ++i) {
        FakeRequest request(runtime);
        request.finish_ok = false;
        request.enqueueWrite({5, 0, 10, true});
        TEST_ASSERT_FALSE(WebResponseUtils::sendHtmlStream(request, "hello", context));
    }
    const auto snapshot = state.snapshot(runtime.now_ms);
    TEST_ASSERT_EQUAL_UINT32(3, snapshot.stats.abort_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.failure_count);
    TEST_ASSERT_EQUAL_UINT32(3, snapshot.failures[0].count);
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.suppressed_failure_logs);
    TEST_ASSERT_EQUAL_UINT32(3, runtime.environment_captures);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_repeated_failure_logs_do_not_hide_abort_counts);
    RUN_TEST(test_finish_failure_is_recorded_even_after_all_body_bytes_or_empty_body);
    RUN_TEST(test_partial_http_timeout_is_not_replayed_or_finished);
    RUN_TEST(test_setup_failure_stops_before_body_and_success_preserves_history);
    RUN_TEST(test_send_html_stream_records_result_and_headers);
    RUN_TEST(test_send_html_stream_resilient_sets_pause_window);
    RUN_TEST(test_send_progmem_asset_sets_immutable_cache_headers);
    return UNITY_END();
}
