#include <unity.h>

#include "web/WebDacUtils.h"

void setUp() {}
void tearDown() {}

void test_web_dac_utils_normalize_timer_seconds_accepts_presets() {
    uint32_t seconds = 123;
    TEST_ASSERT_TRUE(WebDacUtils::normalizeTimerSeconds(0, seconds));
    TEST_ASSERT_EQUAL_UINT32(0, seconds);
    TEST_ASSERT_TRUE(WebDacUtils::normalizeTimerSeconds(600, seconds));
    TEST_ASSERT_EQUAL_UINT32(600, seconds);
    TEST_ASSERT_TRUE(WebDacUtils::normalizeTimerSeconds(28800, seconds));
    TEST_ASSERT_EQUAL_UINT32(28800, seconds);
}

void test_web_dac_utils_normalize_timer_seconds_rejects_invalid_values() {
    uint32_t seconds = 0;
    TEST_ASSERT_FALSE(WebDacUtils::normalizeTimerSeconds(-1, seconds));
    TEST_ASSERT_FALSE(WebDacUtils::normalizeTimerSeconds(1234, seconds));
}

void test_web_dac_utils_output_percent_scales_and_clamps() {
    TEST_ASSERT_EQUAL_UINT8(0, WebDacUtils::outputPercent(0));
    TEST_ASSERT_EQUAL_UINT8(25, WebDacUtils::outputPercent(2500));
    TEST_ASSERT_EQUAL_UINT8(50, WebDacUtils::outputPercent(5000));
    TEST_ASSERT_EQUAL_UINT8(100, WebDacUtils::outputPercent(10000));
    TEST_ASSERT_EQUAL_UINT8(100, WebDacUtils::outputPercent(12000));
}

void test_web_dac_utils_remaining_seconds_uses_round_up() {
    TEST_ASSERT_EQUAL_UINT32(0, WebDacUtils::remainingSeconds(false, 5000, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, WebDacUtils::remainingSeconds(true, 0, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, WebDacUtils::remainingSeconds(true, 1000, 1000));
    TEST_ASSERT_EQUAL_UINT32(4, WebDacUtils::remainingSeconds(true, 4500, 1000));
}

void test_web_dac_utils_status_text_prioritizes_fault_and_availability() {
    TEST_ASSERT_EQUAL_STRING("FAULT", WebDacUtils::statusText(true, true, true));
    TEST_ASSERT_EQUAL_STRING("OFFLINE", WebDacUtils::statusText(false, false, true));
    TEST_ASSERT_EQUAL_STRING("RUNNING", WebDacUtils::statusText(true, false, true));
    TEST_ASSERT_EQUAL_STRING("STOPPED", WebDacUtils::statusText(true, false, false));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_dac_utils_normalize_timer_seconds_accepts_presets);
    RUN_TEST(test_web_dac_utils_normalize_timer_seconds_rejects_invalid_values);
    RUN_TEST(test_web_dac_utils_output_percent_scales_and_clamps);
    RUN_TEST(test_web_dac_utils_remaining_seconds_uses_round_up);
    RUN_TEST(test_web_dac_utils_status_text_prioritizes_fault_and_availability);
    return UNITY_END();
}
