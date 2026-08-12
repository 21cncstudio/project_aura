#include <unity.h>

#include "core/SharedI2cShutdownPolicy.h"

using SharedI2cShutdownPolicy::SafeOutputDecision;

void setUp() {}
void tearDown() {}

void test_active_owners_block_dac_safe_output_even_when_lvgl_is_paused() {
    const SafeOutputDecision decision =
        SharedI2cShutdownPolicy::decideSafeOutput(true, false);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(SafeOutputDecision::SkipOwnersActive),
                          static_cast<int>(decision));
    TEST_ASSERT_FALSE(SharedI2cShutdownPolicy::shouldAttemptSafeOutput(decision));
}

void test_drained_owners_allow_dac_safe_output_after_lvgl_pause() {
    const SafeOutputDecision decision =
        SharedI2cShutdownPolicy::decideSafeOutput(true, true);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(SafeOutputDecision::Attempt),
                          static_cast<int>(decision));
    TEST_ASSERT_TRUE(SharedI2cShutdownPolicy::shouldAttemptSafeOutput(decision));
    TEST_ASSERT_FALSE(
        SharedI2cShutdownPolicy::shouldWarnUnconfirmedLvglPause(decision));
}

void test_drained_owners_allow_dac_safe_output_without_lvgl_pause_ack() {
    const SafeOutputDecision decision =
        SharedI2cShutdownPolicy::decideSafeOutput(false, true);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SafeOutputDecision::AttemptAfterUnconfirmedLvglPause),
        static_cast<int>(decision));
    TEST_ASSERT_TRUE(SharedI2cShutdownPolicy::shouldAttemptSafeOutput(decision));
    TEST_ASSERT_TRUE(
        SharedI2cShutdownPolicy::shouldWarnUnconfirmedLvglPause(decision));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_active_owners_block_dac_safe_output_even_when_lvgl_is_paused);
    RUN_TEST(test_drained_owners_allow_dac_safe_output_after_lvgl_pause);
    RUN_TEST(test_drained_owners_allow_dac_safe_output_without_lvgl_pause_ack);
    return UNITY_END();
}
