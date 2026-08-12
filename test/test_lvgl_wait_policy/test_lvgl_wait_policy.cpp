#include <unity.h>

#include "core/LvglWaitPolicy.h"

void setUp() {}
void tearDown() {}

void test_wait_timeouts_are_finite_and_positive() {
    TEST_ASSERT_GREATER_THAN_UINT32(0, LvglWaitPolicy::VSYNC_WAIT_TIMEOUT_MS);
    TEST_ASSERT_GREATER_THAN_INT(0, LvglWaitPolicy::SCREEN_FLIP_LOCK_TIMEOUT_MS);
    TEST_ASSERT_GREATER_THAN_INT(0, LvglWaitPolicy::DEINIT_LOCK_TIMEOUT_MS);
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

void test_control_wake_keeps_original_vsync_deadline() {
    TEST_ASSERT_EQUAL_UINT32(
        250, LvglWaitPolicy::remainingWaitTicks(1000, 1000, 250));
    TEST_ASSERT_EQUAL_UINT32(
        243, LvglWaitPolicy::remainingWaitTicks(1000, 1007, 250));
    TEST_ASSERT_EQUAL_UINT32(
        1, LvglWaitPolicy::remainingWaitTicks(1000, 1249, 250));
    TEST_ASSERT_EQUAL_UINT32(
        0, LvglWaitPolicy::remainingWaitTicks(1000, 1250, 250));
    TEST_ASSERT_EQUAL_UINT32(
        0, LvglWaitPolicy::remainingWaitTicks(1000, 1300, 250));
}

void test_vsync_deadline_is_rollover_safe() {
    TEST_ASSERT_EQUAL_UINT32(
        3,
        LvglWaitPolicy::remainingWaitTicks(UINT32_MAX - 3U, 3, 10));
    TEST_ASSERT_EQUAL_UINT32(
        0,
        LvglWaitPolicy::remainingWaitTicks(UINT32_MAX - 3U, 6, 10));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_wait_timeouts_are_finite_and_positive);
    RUN_TEST(test_vsync_timeout_logging_is_rate_limited_by_power_of_two);
    RUN_TEST(test_vsync_ack_requires_a_strictly_new_counter_value);
    RUN_TEST(test_control_wake_keeps_original_vsync_deadline);
    RUN_TEST(test_vsync_deadline_is_rollover_safe);
    return UNITY_END();
}
