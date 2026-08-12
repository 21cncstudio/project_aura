#include <unity.h>

#include <cstdint>

#include "core/StartupProbePolicy.h"

void setUp() {}
void tearDown() {}

void test_attempts_follow_absolute_offsets_and_exhaust_after_five() {
    StartupProbePolicy::State state;
    state.reset(500U);

    TEST_ASSERT_TRUE(state.shouldAttempt(500U));
    state.recordAttempt(false);
    TEST_ASSERT_FALSE(state.shouldAttempt(1499U));
    TEST_ASSERT_TRUE(state.shouldAttempt(1500U));
    state.recordAttempt(false);
    TEST_ASSERT_FALSE(state.shouldAttempt(3499U));
    TEST_ASSERT_TRUE(state.shouldAttempt(3500U));
    state.recordAttempt(false);
    TEST_ASSERT_TRUE(state.shouldAttempt(10500U));
    state.recordAttempt(false);
    TEST_ASSERT_TRUE(state.shouldAttempt(30500U));
    state.recordAttempt(false);

    TEST_ASSERT_TRUE(state.exhausted());
    TEST_ASSERT_FALSE(state.pending());
    TEST_ASSERT_FALSE(state.shouldAttempt(UINT32_MAX));
    TEST_ASSERT_EQUAL_UINT8(StartupProbePolicy::kMaxAttempts, state.attempts());
}

void test_success_stops_remaining_attempts() {
    StartupProbePolicy::State state;
    state.reset(0U);
    state.recordAttempt(false);
    TEST_ASSERT_TRUE(state.shouldAttempt(1000U));
    state.recordAttempt(true);

    TEST_ASSERT_TRUE(state.succeeded());
    TEST_ASSERT_FALSE(state.exhausted());
    TEST_ASSERT_FALSE(state.shouldAttempt(30000U));
    TEST_ASSERT_EQUAL_UINT8(2, state.attempts());
}

void test_deadlines_are_rollover_safe() {
    constexpr uint32_t start_ms = UINT32_MAX - 500U;
    StartupProbePolicy::State state;
    state.reset(start_ms);
    state.recordAttempt(false);

    TEST_ASSERT_FALSE(state.shouldAttempt(UINT32_MAX));
    TEST_ASSERT_FALSE(state.shouldAttempt(498U));
    TEST_ASSERT_TRUE(state.shouldAttempt(499U));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_attempts_follow_absolute_offsets_and_exhaust_after_five);
    RUN_TEST(test_success_stops_remaining_attempts);
    RUN_TEST(test_deadlines_are_rollover_safe);
    return UNITY_END();
}
