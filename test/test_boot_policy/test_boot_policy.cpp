#include <unity.h>

#include "core/BootPolicy.h"

namespace {

int actionToInt(StorageManager::BootAction action) {
    return static_cast<int>(action);
}

} // namespace

void setUp() {}
void tearDown() {}

void test_boot_policy_non_crash_resets_counters() {
    uint32_t boot_count = 3;
    uint32_t safe_boot_stage = 2;

    StorageManager::BootAction action =
        BootPolicy::apply(false, boot_count, safe_boot_stage, 5);

    TEST_ASSERT_EQUAL_UINT32(0, boot_count);
    TEST_ASSERT_EQUAL_UINT32(0, safe_boot_stage);
    TEST_ASSERT_EQUAL_INT(actionToInt(StorageManager::BootAction::Normal), actionToInt(action));
}

void test_boot_policy_crash_increments_until_limit() {
    uint32_t boot_count = 0;
    uint32_t safe_boot_stage = 0;

    StorageManager::BootAction action =
        BootPolicy::apply(true, boot_count, safe_boot_stage, 5);

    TEST_ASSERT_EQUAL_UINT32(1, boot_count);
    TEST_ASSERT_EQUAL_UINT32(0, safe_boot_stage);
    TEST_ASSERT_EQUAL_INT(actionToInt(StorageManager::BootAction::Normal), actionToInt(action));
}

void test_boot_policy_safe_rollback_when_threshold_hit() {
    uint32_t boot_count = 4;
    uint32_t safe_boot_stage = 0;

    StorageManager::BootAction action =
        BootPolicy::apply(true, boot_count, safe_boot_stage, 5);

    TEST_ASSERT_EQUAL_UINT32(5, boot_count);
    TEST_ASSERT_EQUAL_UINT32(1, safe_boot_stage);
    TEST_ASSERT_EQUAL_INT(actionToInt(StorageManager::BootAction::SafeRollback), actionToInt(action));
}

void test_boot_policy_safe_factory_after_stage() {
    uint32_t boot_count = 5;
    uint32_t safe_boot_stage = 1;

    StorageManager::BootAction action =
        BootPolicy::apply(true, boot_count, safe_boot_stage, 5);

    TEST_ASSERT_EQUAL_UINT32(6, boot_count);
    TEST_ASSERT_EQUAL_UINT32(1, safe_boot_stage);
    TEST_ASSERT_EQUAL_INT(actionToInt(StorageManager::BootAction::SafeFactoryReset), actionToInt(action));
}

void test_boot_policy_mark_stable_before_timeout() {
    bool boot_stable = false;
    uint32_t boot_count = 3;
    uint32_t safe_boot_stage = 2;

    bool changed = BootPolicy::markStable(999, 0, 1000, boot_stable, boot_count, safe_boot_stage);

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_FALSE(boot_stable);
    TEST_ASSERT_EQUAL_UINT32(3, boot_count);
    TEST_ASSERT_EQUAL_UINT32(2, safe_boot_stage);
}

void test_boot_policy_mark_stable_after_timeout() {
    bool boot_stable = false;
    uint32_t boot_count = 2;
    uint32_t safe_boot_stage = 1;

    bool changed = BootPolicy::markStable(1000, 0, 1000, boot_stable, boot_count, safe_boot_stage);

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_TRUE(boot_stable);
    TEST_ASSERT_EQUAL_UINT32(0, boot_count);
    TEST_ASSERT_EQUAL_UINT32(0, safe_boot_stage);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_policy_non_crash_resets_counters);
    RUN_TEST(test_boot_policy_crash_increments_until_limit);
    RUN_TEST(test_boot_policy_safe_rollback_when_threshold_hit);
    RUN_TEST(test_boot_policy_safe_factory_after_stage);
    RUN_TEST(test_boot_policy_mark_stable_before_timeout);
    RUN_TEST(test_boot_policy_mark_stable_after_timeout);
    return UNITY_END();
}
