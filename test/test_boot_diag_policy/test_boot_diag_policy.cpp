#include <unity.h>

#include "ui/BootDiagPolicy.h"

void setUp() {}
void tearDown() {}

void test_sen66_pending_during_retry_window() {
    TEST_ASSERT_TRUE(BootDiagPolicy::sen66Pending(false, 5000, 3000));
}

void test_sen66_pending_clears_after_deadline() {
    TEST_ASSERT_FALSE(BootDiagPolicy::sen66Pending(false, 5000, 5000));
    TEST_ASSERT_FALSE(BootDiagPolicy::sen66Pending(false, 5000, 6000));
}

void test_sen66_pending_is_false_when_sensor_ok() {
    TEST_ASSERT_FALSE(BootDiagPolicy::sen66Pending(true, 5000, 3000));
}

void test_auto_advance_blocks_while_sensor_pending() {
    TEST_ASSERT_FALSE(BootDiagPolicy::shouldAutoAdvance(false, true, 3000, 3000));
}

void test_auto_advance_blocks_on_errors() {
    TEST_ASSERT_FALSE(BootDiagPolicy::shouldAutoAdvance(true, false, 3000, 3000));
}

void test_auto_advance_runs_only_after_timeout_without_blocks() {
    TEST_ASSERT_FALSE(BootDiagPolicy::shouldAutoAdvance(false, false, 2999, 3000));
    TEST_ASSERT_TRUE(BootDiagPolicy::shouldAutoAdvance(false, false, 3000, 3000));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_sen66_pending_during_retry_window);
    RUN_TEST(test_sen66_pending_clears_after_deadline);
    RUN_TEST(test_sen66_pending_is_false_when_sensor_ok);
    RUN_TEST(test_auto_advance_blocks_while_sensor_pending);
    RUN_TEST(test_auto_advance_blocks_on_errors);
    RUN_TEST(test_auto_advance_runs_only_after_timeout_without_blocks);
    return UNITY_END();
}
