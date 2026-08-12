#include <unity.h>

#include "core/TouchWakePolicy.h"

using TouchWakePolicy::Sample;
using TouchWakePolicy::ErrorStreak;
using TouchWakePolicy::RecoveryStateMachine;
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

void test_polling_controller_uses_sparse_fallback() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);

    TEST_ASSERT_FALSE(policy.shouldProbe(false, false, 101));
    TEST_ASSERT_FALSE(policy.shouldProbe(
        false, false, 100 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS - 1));
    TEST_ASSERT_TRUE(policy.shouldProbe(
        false, false, 100 + TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS));
}

void test_interrupt_pending_bypasses_sparse_fallback_deadline() {
    StateMachine policy;
    policy.setEnabled(true, true, 100);

    TEST_ASSERT_TRUE(policy.shouldProbe(true, true, 101));
}

void test_failed_recovery_enters_offline_and_uses_backoff() {
    RecoveryStateMachine recovery;

    TEST_ASSERT_TRUE(recovery.canAttempt(100));
    recovery.recordAttempt(false, 100);

    TEST_ASSERT_TRUE(recovery.isOffline());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.attempts());
    TEST_ASSERT_EQUAL_UINT32(0, recovery.successes());
    TEST_ASSERT_EQUAL_UINT8(1, recovery.failStreak());
    TEST_ASSERT_EQUAL_UINT32(TouchWakePolicy::RECOVERY_COOLDOWN_MS,
                             recovery.cooldownMs());
    TEST_ASSERT_FALSE(recovery.canAttempt(
        100 + TouchWakePolicy::RECOVERY_COOLDOWN_MS - 1));
    TEST_ASSERT_TRUE(recovery.canAttempt(
        100 + TouchWakePolicy::RECOVERY_COOLDOWN_MS));

    recovery.recordAttempt(false, 100 + TouchWakePolicy::RECOVERY_COOLDOWN_MS);
    TEST_ASSERT_EQUAL_UINT8(2, recovery.failStreak());
    TEST_ASSERT_EQUAL_UINT32(2 * TouchWakePolicy::RECOVERY_COOLDOWN_MS,
                             recovery.cooldownMs());
}

void test_only_successful_read_marks_recovery_successful() {
    RecoveryStateMachine recovery;
    recovery.recordAttempt(false, 100);

    const uint32_t retry_ms = 100 + recovery.cooldownMs();
    TEST_ASSERT_TRUE(recovery.canAttempt(retry_ms));
    recovery.recordAttempt(true, retry_ms);

    TEST_ASSERT_FALSE(recovery.isOffline());
    TEST_ASSERT_EQUAL_UINT32(2, recovery.attempts());
    TEST_ASSERT_EQUAL_UINT32(1, recovery.successes());
    TEST_ASSERT_EQUAL_UINT8(0, recovery.failStreak());
}

void test_failure_during_success_cooldown_can_suspend_polling() {
    RecoveryStateMachine recovery;
    recovery.recordAttempt(true, 100);
    TEST_ASSERT_FALSE(recovery.canAttempt(200));

    recovery.suspendUntilRetry();
    TEST_ASSERT_TRUE(recovery.isOffline());
    TEST_ASSERT_EQUAL_UINT8(0, recovery.failStreak());
    TEST_ASSERT_FALSE(recovery.canAttempt(
        100 + TouchWakePolicy::RECOVERY_SUCCESS_COOLDOWN_MS - 1));
    TEST_ASSERT_TRUE(recovery.canAttempt(
        100 + TouchWakePolicy::RECOVERY_SUCCESS_COOLDOWN_MS));
}

void test_recovery_backoff_is_capped() {
    RecoveryStateMachine recovery;
    uint32_t now_ms = 0;

    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_TRUE(recovery.canAttempt(now_ms));
        recovery.recordAttempt(false, now_ms);
        now_ms += recovery.cooldownMs();
    }

    TEST_ASSERT_EQUAL_UINT32(TouchWakePolicy::RECOVERY_MAX_COOLDOWN_MS,
                             recovery.cooldownMs());
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

void test_sparse_wake_errors_form_a_recovery_streak() {
    ErrorStreak streak;
    TEST_ASSERT_TRUE(TouchWakePolicy::ERROR_STREAK_WINDOW_MS >
                     TouchWakePolicy::FALLBACK_PROBE_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT8(1, streak.recordError(2500));
    TEST_ASSERT_EQUAL_UINT8(2, streak.recordError(5000));
    TEST_ASSERT_EQUAL_UINT8(3, streak.recordError(7500));

    streak.reset();
    TEST_ASSERT_EQUAL_UINT8(1, streak.recordError(10000));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_policy_never_probes_or_wakes);
    RUN_TEST(test_fresh_interrupt_and_valid_point_wake_immediately);
    RUN_TEST(test_touch_held_while_sleep_starts_requires_release_before_wake);
    RUN_TEST(test_interrupt_gate_uses_sparse_fallback_probe);
    RUN_TEST(test_polling_controller_uses_sparse_fallback);
    RUN_TEST(test_interrupt_pending_bypasses_sparse_fallback_deadline);
    RUN_TEST(test_probe_error_preserves_armed_state_and_restarts_fallback_interval);
    RUN_TEST(test_reenabling_same_mode_does_not_erase_pending_wake);
    RUN_TEST(test_fallback_deadline_handles_millisecond_wraparound);
    RUN_TEST(test_sparse_wake_errors_form_a_recovery_streak);
    RUN_TEST(test_failed_recovery_enters_offline_and_uses_backoff);
    RUN_TEST(test_only_successful_read_marks_recovery_successful);
    RUN_TEST(test_failure_during_success_cooldown_can_suspend_polling);
    RUN_TEST(test_recovery_backoff_is_capped);
    return UNITY_END();
}
