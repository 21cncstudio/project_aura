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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_success_maps_to_mounted);
    RUN_TEST(test_no_response_maps_to_neutral_not_detected);
    RUN_TEST(test_other_mount_error_maps_to_fault);
    RUN_TEST(test_all_public_states_have_stable_text);
    return UNITY_END();
}
