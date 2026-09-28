#include <unity.h>

#include "modules/SdCardPolicy.h"

using SdCardPolicy::MountOutcome;
using SdCardPolicy::State;

void setUp() {}
void tearDown() {}

void test_success_maps_to_mounted() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Mounted),
                          static_cast<int>(SdCardPolicy::stateForMountOutcome(MountOutcome::Success)));
}

void test_no_response_maps_to_neutral_not_detected() {
    const State state = SdCardPolicy::stateForMountOutcome(MountOutcome::NoResponse);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::NotDetected), static_cast<int>(state));
    TEST_ASSERT_FALSE(SdCardPolicy::isFault(state));
    TEST_ASSERT_EQUAL_STRING("not_detected", SdCardPolicy::stateText(state));
}

void test_other_mount_error_maps_to_fault() {
    const State state = SdCardPolicy::stateForMountOutcome(MountOutcome::Error);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(State::Fault), static_cast<int>(state));
    TEST_ASSERT_TRUE(SdCardPolicy::isFault(state));
    TEST_ASSERT_EQUAL_STRING("fault", SdCardPolicy::stateText(state));
}

void test_all_public_states_have_stable_text() {
    TEST_ASSERT_EQUAL_STRING("not_attempted", SdCardPolicy::stateText(State::NotAttempted));
    TEST_ASSERT_EQUAL_STRING("mounted", SdCardPolicy::stateText(State::Mounted));
    TEST_ASSERT_EQUAL_STRING("board_unavailable", SdCardPolicy::stateText(State::BoardUnavailable));
}

void test_recovery_backoff_wrap_and_manual_retry() {
    SdCardPolicy::Recovery policy;
    uint32_t now = UINT32_MAX - 2000;
    policy.mountResult(false, true, now);
    const uint32_t delays[] = {5000, 30000, 60000, 300000, 300000};
    for (uint32_t delay : delays) {
        TEST_ASSERT_EQUAL_UINT32(delay, policy.remainingMs(now));
        TEST_ASSERT_FALSE(policy.start(now + delay - 1));
        now += delay;
        TEST_ASSERT_TRUE(policy.start(now));
        TEST_ASSERT_FALSE(policy.start(now));
        policy.mountResult(false, true, now);
    }
    policy.request(now);
    TEST_ASSERT_TRUE(policy.start(now));
    policy.mountResult(true, false, now);
    TEST_ASSERT_FALSE(policy.pending());
    TEST_ASSERT_EQUAL_UINT32(1, policy.successes());
}

void test_filesystem_failure_requires_manual_retry() {
    SdCardPolicy::Recovery policy;
    policy.mountResult(false, false, 0);
    TEST_ASSERT_TRUE(policy.blocked());
    TEST_ASSERT_FALSE(policy.start(300000));
    policy.ioFailure(DailyStorageFailureKind::Io, 300000);
    policy.ioFailure(DailyStorageFailureKind::Io, 300001);
    TEST_ASSERT_FALSE(policy.pending());
    policy.request(300002);
    TEST_ASSERT_TRUE(policy.start(300002));
}

void test_success_between_io_errors_prevents_recovery() {
    SdCardPolicy::Recovery policy;
    policy.ioFailure(DailyStorageFailureKind::Io, 0);
    policy.ioSuccess();
    policy.ioFailure(DailyStorageFailureKind::Io, 1);
    TEST_ASSERT_FALSE(policy.pending());
    policy.ioFailure(DailyStorageFailureKind::Io, 2);
    TEST_ASSERT_TRUE(policy.pending());
}

void test_reconnect_never_overwrites_another_cards_history() {
    TEST_ASSERT_TRUE(SdCardPolicy::mayResumeHistory(true, false, false, true, false));
    TEST_ASSERT_TRUE(SdCardPolicy::mayResumeHistory(false, true, true, true, true));
    TEST_ASSERT_FALSE(SdCardPolicy::mayResumeHistory(false, true, false, false, true));
    TEST_ASSERT_FALSE(SdCardPolicy::mayResumeHistory(false, true, false, true, true));
    TEST_ASSERT_FALSE(SdCardPolicy::mayResumeHistory(false, false, false, true, false));
    TEST_ASSERT_TRUE(SdCardPolicy::mayResumeHistory(false, false, false, false, false));
    // Same CID is not enough when the initial snapshot read failed.
    TEST_ASSERT_FALSE(SdCardPolicy::mayResumeHistory(false, true, true, true, false));
    TEST_ASSERT_TRUE(SdCardPolicy::mayResumeHistory(false, true, true, false, false));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_success_maps_to_mounted);
    RUN_TEST(test_no_response_maps_to_neutral_not_detected);
    RUN_TEST(test_other_mount_error_maps_to_fault);
    RUN_TEST(test_all_public_states_have_stable_text);
    RUN_TEST(test_recovery_backoff_wrap_and_manual_retry);
    RUN_TEST(test_filesystem_failure_requires_manual_retry);
    RUN_TEST(test_reconnect_never_overwrites_another_cards_history);
    RUN_TEST(test_success_between_io_errors_prevents_recovery);
    return UNITY_END();
}
