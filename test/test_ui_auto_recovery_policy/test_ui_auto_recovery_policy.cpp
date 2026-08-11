#include <unity.h>

#include "ui/UiAutoRecoveryPolicy.h"

using UiAutoRecoveryPolicy::Decision;

void setUp() {}
void tearDown() {}

void test_normal_boot_requests_one_restart() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::RequestRestart),
        static_cast<int>(UiAutoRecoveryPolicy::decide(false, false)));
}

void test_recovery_boot_suppresses_restart_loop() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressRecoveryBoot),
        static_cast<int>(UiAutoRecoveryPolicy::decide(true, false)));
}

void test_duplicate_request_is_suppressed() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressAlreadyRequested),
        static_cast<int>(UiAutoRecoveryPolicy::decide(false, true)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_normal_boot_requests_one_restart);
    RUN_TEST(test_recovery_boot_suppresses_restart_loop);
    RUN_TEST(test_duplicate_request_is_suppressed);
    return UNITY_END();
}
