#include <unity.h>

#include "core/LvglWaitPolicy.h"

void setUp() {}
void tearDown() {}

void test_wait_timeouts_are_finite_and_positive() {
    TEST_ASSERT_GREATER_THAN_UINT32(0, LvglWaitPolicy::VSYNC_WAIT_TIMEOUT_MS);
    TEST_ASSERT_GREATER_THAN_INT(0, LvglWaitPolicy::SCREEN_FLIP_LOCK_TIMEOUT_MS);
}

void test_vsync_timeout_logging_is_rate_limited_by_power_of_two() {
    TEST_ASSERT_FALSE(LvglWaitPolicy::shouldLogVsyncTimeout(0));
    TEST_ASSERT_TRUE(LvglWaitPolicy::shouldLogVsyncTimeout(1));
    TEST_ASSERT_TRUE(LvglWaitPolicy::shouldLogVsyncTimeout(2));
    TEST_ASSERT_FALSE(LvglWaitPolicy::shouldLogVsyncTimeout(3));
    TEST_ASSERT_TRUE(LvglWaitPolicy::shouldLogVsyncTimeout(4));
    TEST_ASSERT_FALSE(LvglWaitPolicy::shouldLogVsyncTimeout(5));
    TEST_ASSERT_TRUE(LvglWaitPolicy::shouldLogVsyncTimeout(1024));
}

void test_vsync_ack_requires_a_strictly_new_counter_value() {
    TEST_ASSERT_FALSE(LvglWaitPolicy::hasNewVsync(41, 41));
    TEST_ASSERT_TRUE(LvglWaitPolicy::hasNewVsync(41, 42));
    TEST_ASSERT_TRUE(LvglWaitPolicy::hasNewVsync(UINT32_MAX, 0));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_wait_timeouts_are_finite_and_positive);
    RUN_TEST(test_vsync_timeout_logging_is_rate_limited_by_power_of_two);
    RUN_TEST(test_vsync_ack_requires_a_strictly_new_counter_value);
    return UNITY_END();
}
