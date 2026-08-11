#include <unity.h>

#include "core/BacklightStatePolicy.h"

void setUp() {}
void tearDown() {}

void test_successful_on_transition_disables_wake_probe() {
    const auto result = BacklightStatePolicy::resolve(false, true, true);

    TEST_ASSERT_TRUE(result.command_succeeded);
    TEST_ASSERT_TRUE(result.actual_on);
    TEST_ASSERT_FALSE(result.wake_probe_enabled);
}

void test_failed_on_transition_keeps_off_state_and_wake_probe() {
    const auto result = BacklightStatePolicy::resolve(false, true, false);

    TEST_ASSERT_FALSE(result.command_succeeded);
    TEST_ASSERT_FALSE(result.actual_on);
    TEST_ASSERT_TRUE(result.wake_probe_enabled);
}

void test_failed_off_transition_keeps_on_state_without_wake_probe() {
    const auto result = BacklightStatePolicy::resolve(true, false, false);

    TEST_ASSERT_FALSE(result.command_succeeded);
    TEST_ASSERT_TRUE(result.actual_on);
    TEST_ASSERT_FALSE(result.wake_probe_enabled);
}

void test_successful_off_transition_enables_wake_probe() {
    const auto result = BacklightStatePolicy::resolve(true, false, true);

    TEST_ASSERT_TRUE(result.command_succeeded);
    TEST_ASSERT_FALSE(result.actual_on);
    TEST_ASSERT_TRUE(result.wake_probe_enabled);
}

void test_retry_delay_uses_bounded_exponential_backoff() {
    TEST_ASSERT_EQUAL_UINT32(0, BacklightStatePolicy::retryDelayMs(0));
    TEST_ASSERT_EQUAL_UINT32(500, BacklightStatePolicy::retryDelayMs(1));
    TEST_ASSERT_EQUAL_UINT32(1000, BacklightStatePolicy::retryDelayMs(2));
    TEST_ASSERT_EQUAL_UINT32(4000, BacklightStatePolicy::retryDelayMs(4));
    TEST_ASSERT_EQUAL_UINT32(8000, BacklightStatePolicy::retryDelayMs(5));
    TEST_ASSERT_EQUAL_UINT32(8000, BacklightStatePolicy::retryDelayMs(UINT8_MAX));
}

void test_retry_gate_handles_deadline_and_millisecond_wraparound() {
    TEST_ASSERT_TRUE(BacklightStatePolicy::retryReady(100, 0, false));
    TEST_ASSERT_FALSE(BacklightStatePolicy::retryReady(999, 1000, true));
    TEST_ASSERT_TRUE(BacklightStatePolicy::retryReady(1000, 1000, true));

    const uint32_t deadline = UINT32_MAX - 100;
    TEST_ASSERT_FALSE(BacklightStatePolicy::retryReady(deadline - 1, deadline, true));
    TEST_ASSERT_TRUE(BacklightStatePolicy::retryReady(deadline + 1, deadline, true));
    TEST_ASSERT_FALSE(BacklightStatePolicy::retryReady(UINT32_MAX - 1, 0, true));
    TEST_ASSERT_TRUE(BacklightStatePolicy::retryReady(0, 0, true));
}

void test_failed_command_is_latched_until_retry_deadline() {
    BacklightStatePolicy::PendingCommand pending;
    pending.recordFailure(true, 100);

    TEST_ASSERT_TRUE(pending.active());
    TEST_ASSERT_TRUE(pending.targetOn());
    TEST_ASSERT_EQUAL_UINT8(1, pending.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(600, pending.retryNotBeforeMs());
    TEST_ASSERT_FALSE(pending.ready(599));
    TEST_ASSERT_TRUE(pending.ready(600));
}

void test_repeated_failure_uses_backoff_and_opposite_command_replaces_target() {
    BacklightStatePolicy::PendingCommand pending;
    pending.recordFailure(true, 100);
    pending.recordFailure(true, 600);

    TEST_ASSERT_EQUAL_UINT8(2, pending.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(1600, pending.retryNotBeforeMs());

    pending.recordFailure(false, 700);
    TEST_ASSERT_TRUE(pending.active());
    TEST_ASSERT_FALSE(pending.targetOn());
    TEST_ASSERT_EQUAL_UINT8(1, pending.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(1200, pending.retryNotBeforeMs());
}

void test_clear_cancels_pending_command_and_backoff() {
    BacklightStatePolicy::PendingCommand pending;
    pending.recordFailure(false, 100);
    pending.clear();

    TEST_ASSERT_FALSE(pending.active());
    TEST_ASSERT_EQUAL_UINT8(0, pending.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(0, pending.retryNotBeforeMs());
    TEST_ASSERT_FALSE(pending.ready(1000));
}

void test_input_activity_is_detected_by_inactivity_timer_reset() {
    TEST_ASSERT_TRUE(BacklightStatePolicy::inputActivitySince(30000, 10));
    TEST_ASSERT_FALSE(BacklightStatePolicy::inputActivitySince(30000, 30001));
    TEST_ASSERT_FALSE(BacklightStatePolicy::inputActivitySince(0, 0));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_successful_on_transition_disables_wake_probe);
    RUN_TEST(test_failed_on_transition_keeps_off_state_and_wake_probe);
    RUN_TEST(test_failed_off_transition_keeps_on_state_without_wake_probe);
    RUN_TEST(test_successful_off_transition_enables_wake_probe);
    RUN_TEST(test_retry_delay_uses_bounded_exponential_backoff);
    RUN_TEST(test_retry_gate_handles_deadline_and_millisecond_wraparound);
    RUN_TEST(test_failed_command_is_latched_until_retry_deadline);
    RUN_TEST(test_repeated_failure_uses_backoff_and_opposite_command_replaces_target);
    RUN_TEST(test_clear_cancels_pending_command_and_backoff);
    RUN_TEST(test_input_activity_is_detected_by_inactivity_timer_reset);
    return UNITY_END();
}
