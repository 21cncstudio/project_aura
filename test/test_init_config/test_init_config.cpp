#include <unity.h>

#include "config/AppConfig.h"
#include "core/InitConfig.h"

void setUp() {}
void tearDown() {}

void test_normalize_offsets_rounding() {
    float temp_offset = 1.24f;
    float hum_offset = 2.6f;

    InitConfig::normalizeOffsets(temp_offset, hum_offset);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.2f, temp_offset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, hum_offset);
}

void test_normalize_offsets_clamps_high() {
    float temp_offset = 0.0f;
    float hum_offset = Config::HUM_OFFSET_MAX + 5.0f;

    InitConfig::normalizeOffsets(temp_offset, hum_offset);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, Config::HUM_OFFSET_MAX, hum_offset);
}

void test_normalize_offsets_clamps_low() {
    float temp_offset = 0.0f;
    float hum_offset = Config::HUM_OFFSET_MIN - 5.0f;

    InitConfig::normalizeOffsets(temp_offset, hum_offset);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, Config::HUM_OFFSET_MIN, hum_offset);
}

void test_default_date_format_uses_japanese_iso_and_unit_defaults() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::DateFormat::ISO),
                          static_cast<int>(Config::defaultDateFormat(Config::Language::JA, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::DateFormat::ISO),
                          static_cast<int>(Config::defaultDateFormat(Config::Language::JA, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::DateFormat::DMY),
                          static_cast<int>(Config::defaultDateFormat(Config::Language::EN, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::DateFormat::MDY),
                          static_cast<int>(Config::defaultDateFormat(Config::Language::EN, false)));
}

void test_production_panel_recovery_defaults_do_not_run_unproven_hardware_actions() {
    TEST_ASSERT_FALSE(Config::PANEL_CH422G_STARTUP_PREFLIGHT_ENABLED);
    TEST_ASSERT_FALSE(Config::PANEL_PREINIT_BUS_RECOVERY_ENABLED);
    TEST_ASSERT_FALSE(Config::PANEL_RUNTIME_GT911_HARD_RECOVERY_ENABLED);
    TEST_ASSERT_FALSE(Config::PANEL_RUNTIME_TOUCH_AUTO_RESTART_ENABLED);
    TEST_ASSERT_FALSE(Config::PANEL_RUNTIME_STUCK_LINE_CONFIRMATION_QUALIFIED);
    TEST_ASSERT_FALSE(Config::PANEL_RUNTIME_STUCK_BUS_AUTO_RECOVERY_ENABLED);
    TEST_ASSERT_FALSE(Config::PANEL_STARTUP_AUTO_RESTART_ENABLED);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_normalize_offsets_rounding);
    RUN_TEST(test_normalize_offsets_clamps_high);
    RUN_TEST(test_normalize_offsets_clamps_low);
    RUN_TEST(test_default_date_format_uses_japanese_iso_and_unit_defaults);
    RUN_TEST(test_production_panel_recovery_defaults_do_not_run_unproven_hardware_actions);
    return UNITY_END();
}
