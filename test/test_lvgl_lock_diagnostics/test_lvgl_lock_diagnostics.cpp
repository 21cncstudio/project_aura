#include <unity.h>

#include "core/LvglLockDiagnostics.h"

using LvglLockDiagnostics::Counters;
using LvglLockDiagnostics::Purpose;

void setUp() {}
void tearDown() {}

void test_logo_retries_do_not_report_runtime_failures() {
    Counters counts;
    TEST_ASSERT_FALSE(counts.recordAttempt(Purpose::StartupLogo, false));
    TEST_ASSERT_FALSE(counts.recordAttempt(Purpose::StartupLogo, false));
    TEST_ASSERT_TRUE(counts.recordAttempt(Purpose::StartupLogo, true));

    const auto result = counts.snapshot();
    TEST_ASSERT_EQUAL_UINT32(2, result.startup_logo_misses);
    TEST_ASSERT_EQUAL_UINT32(0, result.runtime_failures);
}

void test_runtime_failure_during_logo_wait_is_not_hidden() {
    Counters counts;
    counts.recordAttempt(Purpose::StartupLogo, false);
    TEST_ASSERT_FALSE(counts.recordAttempt(Purpose::Runtime, false));
    counts.recordAttempt(Purpose::StartupLogo, true);
    counts.recordAttempt(Purpose::Runtime, true);

    const auto result = counts.snapshot();
    TEST_ASSERT_EQUAL_UINT32(1, result.startup_logo_misses);
    TEST_ASSERT_EQUAL_UINT32(1, result.runtime_failures);
}

void test_later_runtime_failure_preserves_both_histories() {
    Counters counts;
    counts.recordAttempt(Purpose::Runtime, false);
    counts.recordAttempt(Purpose::StartupLogo, false);
    counts.recordAttempt(Purpose::StartupLogo, true);
    const auto at_logo_ready = counts.snapshot();
    TEST_ASSERT_EQUAL_UINT32(1, at_logo_ready.runtime_failures);
    TEST_ASSERT_EQUAL_UINT32(1, at_logo_ready.startup_logo_misses);

    counts.recordAttempt(Purpose::Runtime, false);
    const auto later = counts.snapshot();
    TEST_ASSERT_EQUAL_UINT32(2, later.runtime_failures);
    TEST_ASSERT_EQUAL_UINT32(1, later.startup_logo_misses);
}

void test_snapshot_does_not_consume_pending_failures() {
    Counters counts;
    counts.recordAttempt(Purpose::StartupLogo, false);
    counts.recordAttempt(Purpose::Runtime, false);
    const auto first = counts.snapshot();
    const auto second = counts.snapshot();
    TEST_ASSERT_EQUAL_UINT32(first.startup_logo_misses, second.startup_logo_misses);
    TEST_ASSERT_EQUAL_UINT32(first.runtime_failures, second.runtime_failures);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_logo_retries_do_not_report_runtime_failures);
    RUN_TEST(test_runtime_failure_during_logo_wait_is_not_hidden);
    RUN_TEST(test_later_runtime_failure_preserves_both_histories);
    RUN_TEST(test_snapshot_does_not_consume_pending_failures);
    return UNITY_END();
}
