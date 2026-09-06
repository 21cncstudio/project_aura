#include <unity.h>

#include "core/BoardInitTaskLifecycle.h"

using BoardInitTaskLifecycle::Lifecycle;
using BoardInitTaskLifecycle::State;
using BoardInitTaskLifecycle::TimeoutClaim;

void setUp() {}
void tearDown() {}

void test_completion_wins_timeout_boundary_and_requires_ack() {
    Lifecycle lifecycle;

    TEST_ASSERT_TRUE(lifecycle.childPublishCompletion());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TimeoutClaim::CompletionOwned),
        static_cast<int>(lifecycle.parentClaimTimeout()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Completed),
                          static_cast<int>(lifecycle.state()));
    TEST_ASSERT_FALSE(lifecycle.parentOwnsDeletion());

    TEST_ASSERT_TRUE(lifecycle.parentAcknowledgeCompletion());
    TEST_ASSERT_FALSE(lifecycle.parentOwnsDeletion());
    TEST_ASSERT_TRUE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_TRUE(lifecycle.parentOwnsDeletion());
}

void test_timeout_wins_boundary_and_blocks_late_completion() {
    Lifecycle lifecycle;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TimeoutClaim::CancelOwned),
        static_cast<int>(lifecycle.parentClaimTimeout()));
    TEST_ASSERT_TRUE(lifecycle.parentOwnsDeletion());
    TEST_ASSERT_FALSE(lifecycle.childPublishCompletion());
    TEST_ASSERT_FALSE(lifecycle.parentAcknowledgeCompletion());
    TEST_ASSERT_FALSE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TimeoutClaim::Invalid),
        static_cast<int>(lifecycle.parentClaimTimeout()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::CancelOwned),
                          static_cast<int>(lifecycle.state()));
}

void test_completion_cleanup_is_strictly_two_phase() {
    Lifecycle lifecycle;

    TEST_ASSERT_FALSE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_FALSE(lifecycle.parentAcknowledgeCompletion());
    TEST_ASSERT_FALSE(lifecycle.parentOwnsDeletion());

    TEST_ASSERT_TRUE(lifecycle.childPublishCompletion());
    TEST_ASSERT_FALSE(lifecycle.childPublishCompletion());
    TEST_ASSERT_FALSE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_TRUE(lifecycle.parentAcknowledgeCompletion());
    TEST_ASSERT_FALSE(lifecycle.parentAcknowledgeCompletion());
    TEST_ASSERT_TRUE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_FALSE(lifecycle.childPublishDeleteReady());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::DeleteReady),
                          static_cast<int>(lifecycle.state()));
}

void test_attempts_have_independent_ownership_state() {
    Lifecycle timed_out;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TimeoutClaim::CancelOwned),
        static_cast<int>(timed_out.parentClaimTimeout()));

    Lifecycle completed;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Running),
                          static_cast<int>(completed.state()));
    TEST_ASSERT_TRUE(completed.childPublishCompletion());
    TEST_ASSERT_TRUE(completed.parentAcknowledgeCompletion());
    TEST_ASSERT_TRUE(completed.childPublishDeleteReady());
    TEST_ASSERT_TRUE(completed.parentOwnsDeletion());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_completion_wins_timeout_boundary_and_requires_ack);
    RUN_TEST(test_timeout_wins_boundary_and_blocks_late_completion);
    RUN_TEST(test_completion_cleanup_is_strictly_two_phase);
    RUN_TEST(test_attempts_have_independent_ownership_state);
    return UNITY_END();
}
