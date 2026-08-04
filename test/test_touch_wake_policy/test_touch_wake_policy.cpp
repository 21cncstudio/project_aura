#include <unity.h>

#include "core/TouchWakePolicy.h"

using TouchWakePolicy::Sample;
using TouchWakePolicy::StateMachine;

void setUp() {}
void tearDown() {}

void test_disabled_policy_never_probes_or_wakes() {
    StateMachine policy;

    TEST_ASSERT_FALSE(policy.shouldProbe(false, true, 100));
    policy.recordProbe(Sample::Pressed, 100);
    TEST_ASSERT_FALSE(policy.takePendingWake());
}

void test_fresh_interrupt_and_valid_point_wake_immediately() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);

    TEST_ASSERT_TRUE(policy.shouldProbe(true, true, 120));
    policy.recordProbe(Sample::Pressed, 120);

    TEST_ASSERT_TRUE(policy.hasPendingWake());
    TEST_ASSERT_TRUE(policy.takePendingWake());
    TEST_ASSERT_FALSE(policy.takePendingWake());
}

void test_touch_held_while_sleep_starts_requires_release_before_wake() {
    StateMachine policy;
    policy.setEnabled(true, false, 100);

    policy.recordProbe(Sample::Pressed, 120);
    TEST_ASSERT_FALSE(policy.takePendingWake());

    policy.recordProbe(Sample::Released, 140);
    policy.recordProbe(Sample::Pressed, 160);
    TEST_ASSERT_TRUE(policy.takePendingWake());
}

void test_interrupt_gate_uses_sparse_fallback_probe() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);

    TEST_ASSERT_FALSE(policy.shouldProbe(true, false,
                                        100 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS - 1));
    TEST_ASSERT_TRUE(policy.shouldProbe(true, false,
                                       100 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS));

    policy.recordProbe(Sample::Released,
                       100 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS);
    TEST_ASSERT_FALSE(policy.shouldProbe(true, false,
                                        100 + (2 * TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS) - 1));
}

void test_polling_controller_does_not_wait_for_fallback() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);

    TEST_ASSERT_TRUE(policy.shouldProbe(false, false, 101));
}

void test_probe_error_preserves_armed_state_and_restarts_fallback_interval() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);
    policy.recordProbe(Sample::Error, 200);

    TEST_ASSERT_FALSE(policy.shouldProbe(true, false,
                                        200 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS - 1));
    TEST_ASSERT_TRUE(policy.shouldProbe(true, true, 220));
    policy.recordProbe(Sample::Pressed, 220);
    TEST_ASSERT_TRUE(policy.takePendingWake());
}

void test_reenabling_same_mode_does_not_erase_pending_wake() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);
    policy.recordProbe(Sample::Pressed, 120);

    policy.setEnabled(true, false, 130);
    TEST_ASSERT_TRUE(policy.takePendingWake());
}

void test_fallback_deadline_handles_millisecond_wraparound() {
    StateMachine policy;
    const uint32_t start_ms = UINT32_MAX - 1000;
    policy.setEnabled(true, true, start_ms);

    TEST_ASSERT_FALSE(policy.shouldProbe(
        true, false, start_ms + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS - 1));
    TEST_ASSERT_TRUE(policy.shouldProbe(
        true, false, start_ms + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_policy_never_probes_or_wakes);
    RUN_TEST(test_fresh_interrupt_and_valid_point_wake_immediately);
    RUN_TEST(test_touch_held_while_sleep_starts_requires_release_before_wake);
    RUN_TEST(test_interrupt_gate_uses_sparse_fallback_probe);
    RUN_TEST(test_polling_controller_does_not_wait_for_fallback);
    RUN_TEST(test_probe_error_preserves_armed_state_and_restarts_fallback_interval);
    RUN_TEST(test_reenabling_same_mode_does_not_erase_pending_wake);
    RUN_TEST(test_fallback_deadline_handles_millisecond_wraparound);
    return UNITY_END();
}
