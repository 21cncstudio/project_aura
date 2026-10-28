#include <unity.h>
#include <cstring>
#include "web/WebHttpSendCapture.h"

void setUp() {}
void tearDown() {}

void test_partial_chunk_preserves_errno_before_sdk_clobbers_it() {
    WebHttpSendCapture capture;
    const uint8_t body[] = {1, 2, 3, 4};
    TEST_ASSERT_FALSE(capture.chunk(WebStreamStage::Body, body, sizeof(body), [&]() {
        capture.socketSend("4\r\n", 3, []() { errno = EPIPE; return 3; });
        capture.socketSend(reinterpret_cast<const char *>(body), 4, []() { return 2; });
        capture.socketSend(reinterpret_cast<const char *>(body + 2), 2, []() { errno = ECONNRESET; return -1; });
        errno = EIO; // free/log/event below send() may change errno.
        return 0xB006;
    }));
    const auto error = capture.error();
    TEST_ASSERT_TRUE(error.errno_valid);
    TEST_ASSERT_TRUE(error.sdk_error_valid);
    TEST_ASSERT_EQUAL_INT(ECONNRESET, error.socket_errno);
    TEST_ASSERT_EQUAL_HEX32(0xB006, error.sdk_error);
    TEST_ASSERT_EQUAL_UINT32(5, error.wire_bytes);
    TEST_ASSERT_EQUAL_UINT32(2, error.partial_body_bytes);
    TEST_ASSERT_EQUAL(StreamAbortReason::ConnectionReset, web_stream_error_reason(error));
    int replay_calls = 0;
    TEST_ASSERT_FALSE(capture.chunk(WebStreamStage::Finish, nullptr, 0, [&]() { ++replay_calls; return 0; }));
    TEST_ASSERT_EQUAL(0, replay_calls);
    TEST_ASSERT_EQUAL(WebStreamStage::Body, capture.error().stage);
}

void test_internal_failure_does_not_read_stale_errno_and_request_reset_clears_fault() {
    WebHttpSendCapture capture;
    errno = ECONNRESET;
    TEST_ASSERT_FALSE(capture.chunk(WebStreamStage::Body, nullptr, 0, []() { return 0xB005; }));
    TEST_ASSERT_FALSE(capture.error().errno_valid);
    TEST_ASSERT_EQUAL(0, capture.error().socket_errno);
    TEST_ASSERT_EQUAL(StreamAbortReason::InternalError, web_stream_error_reason(capture.error()));
    capture.reset();
    TEST_ASSERT_TRUE(capture.chunk(WebStreamStage::Finish, nullptr, 0, [&]() {
        capture.socketSend("0\r\n\r\n", 5, []() { errno = ECONNRESET; return 5; });
        return 0;
    }));
    TEST_ASSERT_FALSE(capture.error().errno_valid);
    TEST_ASSERT_FALSE(capture.error().terminal);
    TEST_ASSERT_EQUAL_UINT32(5, capture.error().wire_bytes);
}

void test_timeout_finish_and_zero_send_have_distinct_evidence() {
    WebHttpSendCapture capture;
    TEST_ASSERT_FALSE(capture.chunk(WebStreamStage::Finish, nullptr, 0, [&]() {
        capture.socketSend("0\r\n", 3, []() { errno = EAGAIN; return -1; });
        return 0xB006;
    }));
    TEST_ASSERT_EQUAL(StreamAbortReason::SocketTimeout, web_stream_error_reason(capture.error()));
    TEST_ASSERT_EQUAL(WebStreamStage::Finish, capture.error().stage);
    capture.reset();
    TEST_ASSERT_FALSE(capture.chunk(WebStreamStage::Body, nullptr, 0, [&]() {
        TEST_ASSERT_EQUAL(-1, capture.socketSend("x", 1, []() { errno = EPIPE; return 0; }));
        return 0xB006;
    }));
    TEST_ASSERT_FALSE(capture.error().errno_valid);
    TEST_ASSERT_EQUAL(0, capture.error().send_result);
    TEST_ASSERT_EQUAL(StreamAbortReason::InternalError, web_stream_error_reason(capture.error()));
}

void test_request_text_is_bounded_single_line_and_omits_query() {
    WebStreamRequestInfo info;
    TEST_ASSERT_FALSE(web_stream_copy_text(info.path, sizeof(info.path), "/dashboard?token=secret#part", true));
    TEST_ASSERT_EQUAL_STRING("/dashboard", info.path);
    web_stream_copy_text(info.user_agent, sizeof(info.user_agent), "Client\r\nInjected: stuff");
    TEST_ASSERT_EQUAL_STRING("Client??Injected: stuff", info.user_agent);
    char long_agent[200]; memset(long_agent, 'a', sizeof(long_agent)); long_agent[199] = 0;
    TEST_ASSERT_TRUE(web_stream_copy_text(info.user_agent, sizeof(info.user_agent), long_agent));
    TEST_ASSERT_EQUAL_UINT32(79, strlen(info.user_agent));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_partial_chunk_preserves_errno_before_sdk_clobbers_it);
    RUN_TEST(test_internal_failure_does_not_read_stale_errno_and_request_reset_clears_fault);
    RUN_TEST(test_timeout_finish_and_zero_send_have_distinct_evidence);
    RUN_TEST(test_request_text_is_bounded_single_line_and_omits_query);
    return UNITY_END();
}
