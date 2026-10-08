#include <unity.h>

#include "config/AppConfig.h"
#include "core/ScreenRotationRestartPolicy.h"

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

void test_rotation_is_saved_before_one_restart() {
    for(bool initial : {false, true}) {
        bool value = initial, stored = initial;
        unsigned saves = 0, restarts = 0;
        ScreenRotationRestartPolicy policy;
        auto save = [&] { ++saves; stored = value; return true; };
        auto restart = [&] { TEST_ASSERT_EQUAL(!initial, stored); ++restarts; };
        TEST_ASSERT_TRUE(policy.request(value, save, restart));
        TEST_ASSERT_EQUAL(!initial, value);
        TEST_ASSERT_TRUE(policy.pending());
        TEST_ASSERT_FALSE(policy.request(value, save, restart));
        TEST_ASSERT_EQUAL_UINT(1, saves);
        TEST_ASSERT_EQUAL_UINT(1, restarts);
        TEST_ASSERT_EQUAL(!initial, value);
        // A new boot consumes the saved orientation and can toggle back.
        ScreenRotationRestartPolicy next_boot;
        value = stored;
        TEST_ASSERT_TRUE(next_boot.request(value, save, [&] { ++restarts; }));
        TEST_ASSERT_EQUAL(initial, stored);
        TEST_ASSERT_EQUAL_UINT(2, restarts);
    }
}

void test_failed_rotation_save_restores_state_and_allows_retry() {
    for(bool initial : {false, true}) {
        bool value = initial;
        unsigned restarts = 0;
        ScreenRotationRestartPolicy policy;
        TEST_ASSERT_FALSE(policy.request(value, [&] {
            TEST_ASSERT_EQUAL(!initial, value); return false;
        }, [&] { ++restarts; }));
        TEST_ASSERT_EQUAL(initial, value);
        TEST_ASSERT_FALSE(policy.pending());
        TEST_ASSERT_EQUAL_UINT(0, restarts);
        TEST_ASSERT_TRUE(policy.request(value, [] { return true; }, [&] { ++restarts; }));
        TEST_ASSERT_EQUAL_UINT(1, restarts);
        TEST_ASSERT_EQUAL(!initial, value);
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_clean_config_uses_profile_rotation_default);
    RUN_TEST(test_missing_value_uses_profile_default);
    RUN_TEST(test_persisted_value_overrides_profile_default_in_both_directions);
    RUN_TEST(test_rotation_is_saved_before_one_restart);
    RUN_TEST(test_failed_rotation_save_restores_state_and_allows_retry);
    return UNITY_END();
}
