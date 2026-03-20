#include <unity.h>

#include <errno.h>
#include <deque>
#include <string>
#include <vector>

#include "web/WebStreamWriter.h"

namespace {

struct FakeRuntime {
    uint32_t now_ms = 0;
    uint32_t delay_calls = 0;
    uint32_t watchdog_kicks = 0;
    uint32_t mqtt_service_calls = 0;
    std::vector<uint32_t> mqtt_service_times;
};

uint32_t fake_now_ms(void *context) {
    return static_cast<FakeRuntime *>(context)->now_ms;
}

void fake_delay_ms(void *context, uint16_t delay_ms) {
    FakeRuntime &runtime = *static_cast<FakeRuntime *>(context);
    runtime.now_ms += delay_ms;
    runtime.delay_calls++;
}

void fake_kick_watchdog(void *context) {
    static_cast<FakeRuntime *>(context)->watchdog_kicks++;
}

void fake_service_connected_mqtt(void *context, uint32_t now_ms) {
    FakeRuntime &runtime = *static_cast<FakeRuntime *>(context);
    runtime.mqtt_service_calls++;
    runtime.mqtt_service_times.push_back(now_ms);
}

struct WriteAction {
    int32_t result = 0;
    int err = 0;
    uint32_t advance_ms = 0;
    bool connected_after = true;
};

struct WaitAction {
    bool writable = false;
    int err = 0;
    uint32_t advance_ms = 0;
};

class FakeRequest final : public WebRequest {
public:
    explicit FakeRequest(FakeRuntime &runtime) : runtime_(runtime) {}

    bool hasArg(const char *) const override { return false; }
    String arg(const char *) const override { return ""; }
    String uri() const override { return "/test"; }
    void sendHeader(const char *, const String &, bool = false) override {}
    void send(int, const char *, const String &) override {}
    void send(int, const char *, const char *) override {}
    bool clientConnected() const override { return connected_; }
    void stopClient() override {
        stop_called_ = true;
        connected_ = false;
    }
    bool beginStreamResponse(int, const char *, size_t, bool = false) override { return true; }

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
        if (waits_.empty()) {
            last_error = 0;
            return true;
        }
        const WaitAction action = waits_.front();
        waits_.pop_front();
        runtime_.now_ms += action.advance_ms;
        last_error = action.err;
        return action.writable;
    }

    void endStreamResponse() override {}
    WebUpload upload() override { return {}; }

    void enqueueWrite(const WriteAction &action) { writes_.push_back(action); }
    void enqueueWait(const WaitAction &action) { waits_.push_back(action); }

    bool stopCalled() const { return stop_called_; }

private:
    FakeRuntime &runtime_;
    std::deque<WriteAction> writes_;
    std::deque<WaitAction> waits_;
    bool connected_ = true;
    bool stop_called_ = false;
};

WebStreamRuntime make_runtime(FakeRuntime &runtime) {
    WebStreamRuntime ops;
    ops.context = &runtime;
    ops.nowMs = fake_now_ms;
    ops.delayMs = fake_delay_ms;
    ops.kickWatchdog = fake_kick_watchdog;
    ops.serviceConnectedMqtt = fake_service_connected_mqtt;
    ops.mqttServiceIntervalMs = 250;
    return ops;
}

StreamProfile make_test_profile() {
    StreamProfile profile = kShellPageStreamProfile;
    profile.chunk_size = 64;
    profile.min_chunk_size = 16;
    profile.max_zero_writes = 4;
    profile.yield_ms = 1;
    profile.max_duration_ms = 1000;
    profile.no_progress_timeout_ms = 100;
    return profile;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_stream_writer_sends_all_bytes_and_records_timing() {
    FakeRuntime runtime;
    FakeRequest request(runtime);
    request.enqueueWrite({2, 0, 3, true});
    request.enqueueWrite({3, 0, 5, true});

    const WebStreamRuntime ops = make_runtime(runtime);
    const StreamProfile profile = make_test_profile();
    const uint8_t data[] = {1, 2, 3, 4, 5};
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::SocketWriteError;
    uint32_t max_write_ms = 0;
    int last_socket_errno = -1;

    const bool ok = web_stream_client_bytes(
        request, data, sizeof(data), profile, ops, sent, abort_reason, max_write_ms, last_socket_errno);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), static_cast<uint32_t>(sent));
    TEST_ASSERT_EQUAL(StreamAbortReason::None, abort_reason);
    TEST_ASSERT_EQUAL_UINT32(5, max_write_ms);
    TEST_ASSERT_EQUAL_INT(0, last_socket_errno);
    TEST_ASSERT_FALSE(request.stopCalled());
    TEST_ASSERT_GREATER_THAN_UINT32(0, runtime.watchdog_kicks);
    TEST_ASSERT_EQUAL_UINT32(1, runtime.delay_calls);
    TEST_ASSERT_EQUAL_UINT32(1, runtime.mqtt_service_calls);
    TEST_ASSERT_EQUAL_UINT32(3, runtime.mqtt_service_times.front());
}

void test_stream_writer_retries_after_eagain_and_finishes() {
    FakeRuntime runtime;
    FakeRequest request(runtime);
    request.enqueueWrite({0, 0, 0, true});
    request.enqueueWait({false, EAGAIN, 2});
    request.enqueueWrite({4, 0, 1, true});

    StreamProfile profile = make_test_profile();
    profile.max_zero_writes = 2;
    const WebStreamRuntime ops = make_runtime(runtime);
    const uint8_t data[] = {1, 2, 3, 4};
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::SocketWriteError;
    uint32_t max_write_ms = 0;
    int last_socket_errno = -1;

    const bool ok = web_stream_client_bytes(
        request, data, sizeof(data), profile, ops, sent, abort_reason, max_write_ms, last_socket_errno);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), static_cast<uint32_t>(sent));
    TEST_ASSERT_EQUAL(StreamAbortReason::None, abort_reason);
    TEST_ASSERT_FALSE(request.stopCalled());
}

void test_stream_writer_aborts_when_zero_write_limit_is_exceeded() {
    FakeRuntime runtime;
    FakeRequest request(runtime);
    request.enqueueWrite({0, 0, 0, true});
    request.enqueueWrite({0, 0, 0, true});

    StreamProfile profile = make_test_profile();
    profile.max_zero_writes = 1;
    const WebStreamRuntime ops = make_runtime(runtime);
    const uint8_t data[] = {1, 2};
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::None;
    uint32_t max_write_ms = 0;
    int last_socket_errno = 0;

    const bool ok = web_stream_client_bytes(
        request, data, sizeof(data), profile, ops, sent, abort_reason, max_write_ms, last_socket_errno);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(StreamAbortReason::ZeroWriteLimit, abort_reason);
    TEST_ASSERT_TRUE(request.stopCalled());
}

void test_stream_writer_aborts_on_non_retryable_socket_error() {
    FakeRuntime runtime;
    FakeRequest request(runtime);
    request.enqueueWrite({-1, EINVAL, 4, true});

    const WebStreamRuntime ops = make_runtime(runtime);
    const StreamProfile profile = make_test_profile();
    const uint8_t data[] = {1, 2, 3};
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::None;
    uint32_t max_write_ms = 0;
    int last_socket_errno = 0;

    const bool ok = web_stream_client_bytes(
        request, data, sizeof(data), profile, ops, sent, abort_reason, max_write_ms, last_socket_errno);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(StreamAbortReason::SocketWriteError, abort_reason);
    TEST_ASSERT_TRUE(request.stopCalled());
    TEST_ASSERT_EQUAL_INT(EINVAL, last_socket_errno);
}

void test_stream_writer_aborts_when_no_progress_timeout_is_hit() {
    FakeRuntime runtime;
    FakeRequest request(runtime);
    request.enqueueWrite({0, 0, 0, true});
    request.enqueueWait({false, 0, 0});
    request.enqueueWrite({0, 0, 0, true});

    StreamProfile profile = make_test_profile();
    profile.no_progress_timeout_ms = 5;
    profile.yield_ms = 6;
    const WebStreamRuntime ops = make_runtime(runtime);
    const uint8_t data[] = {1, 2, 3};
    size_t sent = 0;
    StreamAbortReason abort_reason = StreamAbortReason::None;
    uint32_t max_write_ms = 0;
    int last_socket_errno = 0;

    const bool ok = web_stream_client_bytes(
        request, data, sizeof(data), profile, ops, sent, abort_reason, max_write_ms, last_socket_errno);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(StreamAbortReason::NoProgressTimeout, abort_reason);
    TEST_ASSERT_TRUE(request.stopCalled());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_stream_writer_sends_all_bytes_and_records_timing);
    RUN_TEST(test_stream_writer_retries_after_eagain_and_finishes);
    RUN_TEST(test_stream_writer_aborts_when_zero_write_limit_is_exceeded);
    RUN_TEST(test_stream_writer_aborts_on_non_retryable_socket_error);
    RUN_TEST(test_stream_writer_aborts_when_no_progress_timeout_is_hit);
    return UNITY_END();
}
