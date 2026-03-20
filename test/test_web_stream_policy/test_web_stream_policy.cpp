#include <unity.h>

#include "web/WebStreamPolicy.h"

void setUp() {}
void tearDown() {}

void test_effective_stream_chunk_size_keeps_fixed_html_chunk() {
    TEST_ASSERT_EQUAL_UINT32(1460, static_cast<uint32_t>(effective_stream_chunk_size(kHtmlStreamProfile, 0)));
    TEST_ASSERT_EQUAL_UINT32(1460, static_cast<uint32_t>(effective_stream_chunk_size(kHtmlStreamProfile, 200)));
}

void test_effective_stream_chunk_size_scales_shell_profile() {
    TEST_ASSERT_EQUAL_UINT32(512, static_cast<uint32_t>(effective_stream_chunk_size(kShellPageStreamProfile, 0)));
    TEST_ASSERT_EQUAL_UINT32(384, static_cast<uint32_t>(effective_stream_chunk_size(kShellPageStreamProfile, 4)));
    TEST_ASSERT_EQUAL_UINT32(256, static_cast<uint32_t>(effective_stream_chunk_size(kShellPageStreamProfile, 16)));
    TEST_ASSERT_EQUAL_UINT32(256, static_cast<uint32_t>(effective_stream_chunk_size(kShellPageStreamProfile, 48)));
}

void test_stream_retry_delay_uses_expected_buckets() {
    TEST_ASSERT_EQUAL_UINT32(2, stream_retry_delay_ms(kShellPageStreamProfile, 0));
    TEST_ASSERT_EQUAL_UINT32(2, stream_retry_delay_ms(kShellPageStreamProfile, 3));
    TEST_ASSERT_EQUAL_UINT32(12, stream_retry_delay_ms(kShellPageStreamProfile, 4));
    TEST_ASSERT_EQUAL_UINT32(12, stream_retry_delay_ms(kShellPageStreamProfile, 10));
    TEST_ASSERT_EQUAL_UINT32(50, stream_retry_delay_ms(kShellPageStreamProfile, 11));
    TEST_ASSERT_EQUAL_UINT32(50, stream_retry_delay_ms(kShellPageStreamProfile, 40));
    TEST_ASSERT_EQUAL_UINT32(100, stream_retry_delay_ms(kShellPageStreamProfile, 41));
    TEST_ASSERT_EQUAL_UINT32(100, stream_retry_delay_ms(kShellPageStreamProfile, 120));
    TEST_ASSERT_EQUAL_UINT32(150, stream_retry_delay_ms(kShellPageStreamProfile, 121));
}

void test_stream_abort_reason_text_maps_known_values() {
    TEST_ASSERT_EQUAL_STRING("none", stream_abort_reason_text(StreamAbortReason::None));
    TEST_ASSERT_EQUAL_STRING("client_disconnected", stream_abort_reason_text(StreamAbortReason::Disconnected));
    TEST_ASSERT_EQUAL_STRING("socket_write_error", stream_abort_reason_text(StreamAbortReason::SocketWriteError));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_effective_stream_chunk_size_keeps_fixed_html_chunk);
    RUN_TEST(test_effective_stream_chunk_size_scales_shell_profile);
    RUN_TEST(test_stream_retry_delay_uses_expected_buckets);
    RUN_TEST(test_stream_abort_reason_text_maps_known_values);
    return UNITY_END();
}
