#include <unity.h>

#include "core/BoardRecoveryPolicy.h"

using BoardRecoveryPolicy::Decision;

void setUp() {}
void tearDown() {}

void test_ready_board_never_requests_restart() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::NotNeeded),
                          static_cast<int>(BoardRecoveryPolicy::decide(true, true, false, true)));
}

void test_first_eligible_failure_requests_one_restart() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Restart),
                          static_cast<int>(BoardRecoveryPolicy::decide(false, true, false, true)));
}

void test_recovery_boot_failure_stays_headless() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::SuppressAlreadyAttempted),
                          static_cast<int>(BoardRecoveryPolicy::decide(false, false, true, true)));
}

void test_ineligible_failure_does_not_auto_restart() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::SuppressNotEligible),
                          static_cast<int>(BoardRecoveryPolicy::decide(false, false, false, true)));
}

void test_missing_restart_task_stays_headless() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::SuppressRestartUnavailable),
                          static_cast<int>(BoardRecoveryPolicy::decide(false, true, false, false)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_ready_board_never_requests_restart);
    RUN_TEST(test_first_eligible_failure_requests_one_restart);
    RUN_TEST(test_recovery_boot_failure_stays_headless);
    RUN_TEST(test_ineligible_failure_does_not_auto_restart);
    RUN_TEST(test_missing_restart_task_stays_headless);
    return UNITY_END();
}
