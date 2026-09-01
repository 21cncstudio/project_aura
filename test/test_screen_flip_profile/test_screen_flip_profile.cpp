#include <unity.h>

#include "config/AppConfig.h"

void setUp() {}
void tearDown() {}

void test_clean_config_uses_profile_rotation_default() {
    const Config::StoredConfig config{};
#if AURA_HARDWARE_PROFILE_7
    TEST_ASSERT_TRUE(Config::SCREEN_FLIP_180_DEFAULT);
    TEST_ASSERT_TRUE(config.screen_flip_180);
#else
    TEST_ASSERT_FALSE(Config::SCREEN_FLIP_180_DEFAULT);
    TEST_ASSERT_FALSE(config.screen_flip_180);
#endif
}

void test_missing_value_uses_profile_default() {
    TEST_ASSERT_EQUAL(
        Config::SCREEN_FLIP_180_DEFAULT,
        Config::resolveScreenFlip180(false, !Config::SCREEN_FLIP_180_DEFAULT));
}

void test_persisted_value_overrides_profile_default_in_both_directions() {
    TEST_ASSERT_FALSE(Config::resolveScreenFlip180(true, false));
    TEST_ASSERT_TRUE(Config::resolveScreenFlip180(true, true));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_clean_config_uses_profile_rotation_default);
    RUN_TEST(test_missing_value_uses_profile_default);
    RUN_TEST(test_persisted_value_overrides_profile_default_in_both_directions);
    return UNITY_END();
}
