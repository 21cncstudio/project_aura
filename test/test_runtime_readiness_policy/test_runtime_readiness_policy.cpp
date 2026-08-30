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

void test_ota_confirmation_requires_display_sensor_host_and_runtime_health() {
    TEST_ASSERT_TRUE(RuntimeReadinessPolicy::canConfirmOta(true, true, true, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(true, true, true, false));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(true, true, false, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(true, false, true, true));
    TEST_ASSERT_FALSE(RuntimeReadinessPolicy::canConfirmOta(false, false, true, true));
}

void test_lvgl_runtime_management_stays_disabled_after_shared_bus_offline() {
    TEST_ASSERT_TRUE(
        RuntimeReadinessPolicy::canManageLvglRuntime(true, true, true));
    TEST_ASSERT_FALSE(
        RuntimeReadinessPolicy::canManageLvglRuntime(false, true, true));
    TEST_ASSERT_FALSE(
        RuntimeReadinessPolicy::canManageLvglRuntime(true, false, true));
    TEST_ASSERT_FALSE(
        RuntimeReadinessPolicy::canManageLvglRuntime(true, true, false));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_operational_requires_board_and_lvgl);
    RUN_TEST(test_ota_confirmation_requires_display_sensor_host_and_runtime_health);
    RUN_TEST(test_lvgl_runtime_management_stays_disabled_after_shared_bus_offline);
    return UNITY_END();
}
