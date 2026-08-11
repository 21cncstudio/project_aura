#include <unity.h>

#include "core/RuntimeReadinessPolicy.h"

void setUp() {}
void tearDown() {}

void test_operational_requires_board_and_lvgl() {
    TEST_ASSERT_TRUE(RuntimeReadinessPolicy::operational(true, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::operational(true, false));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::operational(false, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::operational(false, false));
}

void test_only_operational_boot_can_confirm_ota() {
    TEST_ASSERT_TRUE(RuntimeReadinessPolicy::canConfirmOta(true, true, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(true, true, false));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(true, false, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(false, false, true));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_operational_requires_board_and_lvgl);
    RUN_TEST(test_only_operational_boot_can_confirm_ota);
    return UNITY_END();
}
