#include <unity.h>

#include "web/WebServerLimits.h"

void setUp() {}
void tearDown() {}

void test_request_body_limit_accepts_boundary_and_rejects_larger_body() {
    TEST_ASSERT_TRUE(WebServerLimits::requestBodySizeAllowed(
        WebServerLimits::kMaxRequestBodyBytes));
    TEST_ASSERT_FALSE(WebServerLimits::requestBodySizeAllowed(
        WebServerLimits::kMaxRequestBodyBytes + 1U));
}

void test_multipart_boundary_and_header_limits_are_bounded() {
    TEST_ASSERT_FALSE(WebServerLimits::multipartBoundarySizeAllowed(0));
    TEST_ASSERT_TRUE(WebServerLimits::multipartBoundarySizeAllowed(
        WebServerLimits::kMaxMultipartBoundaryBytes));
    TEST_ASSERT_FALSE(WebServerLimits::multipartBoundarySizeAllowed(
        WebServerLimits::kMaxMultipartBoundaryBytes + 1U));
    TEST_ASSERT_TRUE(WebServerLimits::multipartHeaderLineSizeAllowed(
        WebServerLimits::kMaxMultipartHeaderLineBytes));
    TEST_ASSERT_FALSE(WebServerLimits::multipartHeaderLineSizeAllowed(
        WebServerLimits::kMaxMultipartHeaderLineBytes + 1U));
}

void test_multipart_field_append_limit_handles_overflow_safely() {
    TEST_ASSERT_TRUE(WebServerLimits::multipartFieldAppendAllowed(0, 1));
    TEST_ASSERT_TRUE(WebServerLimits::multipartFieldAppendAllowed(
        WebServerLimits::kMaxMultipartFieldBytes - 1U, 1));
    TEST_ASSERT_FALSE(WebServerLimits::multipartFieldAppendAllowed(
        WebServerLimits::kMaxMultipartFieldBytes, 1));
    TEST_ASSERT_FALSE(WebServerLimits::multipartFieldAppendAllowed(
        WebServerLimits::kMaxMultipartFieldBytes + 1U, 0));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_request_body_limit_accepts_boundary_and_rejects_larger_body);
    RUN_TEST(test_multipart_boundary_and_header_limits_are_bounded);
    RUN_TEST(test_multipart_field_append_limit_handles_overflow_safely);
    return UNITY_END();
}
