#include <unity.h>

#include "core/RuntimeI2cRecoveryPolicy.h"

using RuntimeI2cRecoveryPolicy::Decision;
using RuntimeI2cRecoveryPolicy::State;

void setUp() {}
void tearDown() {}

void test_idle_samples_never_request_recovery() {
    State state;
    for (uint32_t now_ms = 0; now_ms < 10000; now_ms += 1000) {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                              static_cast<int>(state.poll(now_ms, true, true, false, false, true)));
    }
    TEST_ASSERT_EQUAL_UINT8(0, state.stuckLineSamples());
}

void test_stuck_line_requires_five_spaced_samples() {
    State state;
    for (uint8_t sample = 0;
         sample < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT - 1U;
         ++sample) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(Decision::None),
            static_cast<int>(state.poll(static_cast<uint32_t>(sample) * 1000U,
                                        true,
                                        false,
                                        false,
                                        false,
                                        true)));
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::Restart),
        static_cast<int>(state.poll(4000U, true, false, false, false, true)));
    TEST_ASSERT_TRUE(state.handled());
    TEST_ASSERT_TRUE(state.sharedBusFaultConfirmed());
}

void test_idle_sample_clears_stuck_line_streak() {
    State state;
    state.poll(0, true, false, false, false, true);
    state.poll(1000, true, false, false, false, true);
    state.poll(2000, true, true, false, false, true);
    TEST_ASSERT_EQUAL_UINT8(0, state.stuckLineSamples());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                          static_cast<int>(state.poll(3000, true, false, false, false, true)));
}

void test_unqualified_low_samples_never_confirm_a_fault() {
    State state;
    for (uint8_t sample = 0;
         sample < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT + 2U;
         ++sample) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(Decision::None),
            static_cast<int>(state.poll(
                static_cast<uint32_t>(sample) *
                    RuntimeI2cRecoveryPolicy::SAMPLE_INTERVAL_MS,
                false,
                false,
                false,
                false,
                true)));
    }
    TEST_ASSERT_EQUAL_UINT8(0, state.stuckLineSamples());
    TEST_ASSERT_FALSE(state.sharedBusFaultConfirmed());
    TEST_ASSERT_FALSE(state.handled());
}

void test_unqualified_sample_breaks_a_qualified_low_streak() {
    State state;
    for (uint8_t sample = 0;
         sample < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT - 1U;
         ++sample) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(Decision::None),
            static_cast<int>(state.poll(
                static_cast<uint32_t>(sample) *
                    RuntimeI2cRecoveryPolicy::SAMPLE_INTERVAL_MS,
                true,
                false,
                false,
                false,
                true)));
    }
    TEST_ASSERT_EQUAL_UINT8(
        RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT - 1U,
        state.stuckLineSamples());

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::None),
        static_cast<int>(state.poll(4000U, false, false, false, false, true)));
    TEST_ASSERT_EQUAL_UINT8(0, state.stuckLineSamples());

    for (uint8_t sample = 0;
         sample < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT - 1U;
         ++sample) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(Decision::None),
            static_cast<int>(state.poll(
                5000U + static_cast<uint32_t>(sample) *
                            RuntimeI2cRecoveryPolicy::SAMPLE_INTERVAL_MS,
                true,
                false,
                false,
                false,
                true)));
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::Restart),
        static_cast<int>(state.poll(9000U, true, false, false, false, true)));
}

void test_touch_offline_requests_immediate_single_restart() {
    State state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Restart),
                          static_cast<int>(state.poll(10, false, true, true, false, true)));
    TEST_ASSERT_FALSE(state.sharedBusFaultConfirmed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                          static_cast<int>(state.poll(20, false, true, true, false, true)));
    TEST_ASSERT_FALSE(state.sharedBusFaultConfirmed());
}

void test_loop_guard_and_missing_restart_task_suppress_once() {
    State recovery_boot;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressAlreadyAttempted),
        static_cast<int>(recovery_boot.poll(0, false, true, true, true, true)));
    TEST_ASSERT_FALSE(recovery_boot.sharedBusFaultConfirmed());

    State unavailable;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressRestartUnavailable),
        static_cast<int>(unavailable.poll(0, false, true, true, false, false)));
    TEST_ASSERT_FALSE(unavailable.sharedBusFaultConfirmed());
}

void test_suppressed_stuck_line_fault_marks_shared_bus_offline() {
    State recovery_boot;
    State unavailable;
    for (uint8_t sample = 0;
         sample < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT;
         ++sample) {
        const uint32_t now_ms = static_cast<uint32_t>(sample) * 1000U;
        const Decision recovery_decision = recovery_boot.poll(
            now_ms, true, false, false, true, true);
        const Decision unavailable_decision = unavailable.poll(
            now_ms, true, false, false, false, false);
        if (sample + 1U < RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT) {
            TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                                  static_cast<int>(recovery_decision));
            TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                                  static_cast<int>(unavailable_decision));
        } else {
            TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::SuppressAlreadyAttempted),
                                  static_cast<int>(recovery_decision));
            TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::SuppressRestartUnavailable),
                                  static_cast<int>(unavailable_decision));
        }
    }
    TEST_ASSERT_TRUE(recovery_boot.sharedBusFaultConfirmed());
    TEST_ASSERT_TRUE(unavailable.sharedBusFaultConfirmed());
}

void test_touch_only_suppression_keeps_monitoring_for_confirmed_stuck_lines() {
    State state;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressAlreadyAttempted),
        static_cast<int>(state.poll(0, true, true, true, true, true)));
    TEST_ASSERT_FALSE(state.sharedBusFaultConfirmed());

    for (uint8_t sample = 1;
         sample <= RuntimeI2cRecoveryPolicy::STUCK_LINE_SAMPLE_LIMIT;
         ++sample) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(Decision::None),
            static_cast<int>(state.poll(static_cast<uint32_t>(sample) * 1000U,
                                        true,
                                        false,
                                        true,
                                        true,
                                        true)));
    }
    TEST_ASSERT_TRUE(state.sharedBusFaultConfirmed());
}

void test_sample_interval_is_rollover_safe() {
    State state;
    constexpr uint32_t first = UINT32_MAX - 500U;
    state.poll(first, true, false, false, false, true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                          static_cast<int>(state.poll(498U, true, false, false, false, true)));
    TEST_ASSERT_EQUAL_UINT8(1, state.stuckLineSamples());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::None),
                          static_cast<int>(state.poll(499U, true, false, false, false, true)));
    TEST_ASSERT_EQUAL_UINT8(2, state.stuckLineSamples());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_samples_never_request_recovery);
    RUN_TEST(test_stuck_line_requires_five_spaced_samples);
    RUN_TEST(test_idle_sample_clears_stuck_line_streak);
    RUN_TEST(test_unqualified_low_samples_never_confirm_a_fault);
    RUN_TEST(test_unqualified_sample_breaks_a_qualified_low_streak);
    RUN_TEST(test_touch_offline_requests_immediate_single_restart);
    RUN_TEST(test_loop_guard_and_missing_restart_task_suppress_once);
    RUN_TEST(test_suppressed_stuck_line_fault_marks_shared_bus_offline);
    RUN_TEST(test_touch_only_suppression_keeps_monitoring_for_confirmed_stuck_lines);
    RUN_TEST(test_sample_interval_is_rollover_safe);
    return UNITY_END();
}
