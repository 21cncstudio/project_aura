#include <unity.h>

#include <deque>
#include <vector>

#include "web/WebResponseUtils.h"

namespace {

struct FakeRuntime {
    uint32_t now_ms = 0;
    uint32_t wifi_sta_connected_elapsed_ms = 0;
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
    String uri() const override { return "/test"; }

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
        return true;
    }

    int32_t writeStreamChunk(const uint8_t *, size_t, int &last_error) override {
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

    WebUpload upload() override { return {}; }

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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_send_html_stream_records_result_and_headers);
    RUN_TEST(test_send_html_stream_resilient_sets_pause_window);
    RUN_TEST(test_send_progmem_asset_sets_immutable_cache_headers);
    return UNITY_END();
}
