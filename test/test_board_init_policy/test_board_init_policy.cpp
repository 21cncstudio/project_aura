#include <unity.h>

#include "core/BoardInitPolicy.h"

using BoardInitPolicy::Action;
using BoardInitPolicy::AttemptOutcome;

void setUp() {}
void tearDown() {}

void test_success_returns_without_cleanup() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::ReturnSuccess),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::Success, 1, 3)));
}

void test_normal_failure_retries_with_fresh_generation() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::RetryFresh),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::Failed, 1, 3)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::RetryFresh),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::Failed, 2, 3)));
}

void test_task_creation_failure_is_retryable() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::RetryFresh),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::TaskCreateFailed, 1, 3)));
}

void test_last_round_failure_aborts() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Abort),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::Failed, 3, 3)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Abort),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::TaskCreateFailed, 3, 3)));
}

void test_timeout_always_aborts_without_destructor_or_retry() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Abort),
                          static_cast<int>(BoardInitPolicy::decide(AttemptOutcome::Timeout, 1, 3)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_success_returns_without_cleanup);
    RUN_TEST(test_normal_failure_retries_with_fresh_generation);
    RUN_TEST(test_task_creation_failure_is_retryable);
    RUN_TEST(test_last_round_failure_aborts);
    RUN_TEST(test_timeout_always_aborts_without_destructor_or_retry);
    return UNITY_END();
}
