#include <unity.h>

#include "core/Gt911ProbePolicy.h"
#include "esp_panel_board_custom_conf.h"

#ifndef TEST_GT911_EXPECTED_ADDRESS
#if AURA_HARDWARE_PROFILE_7
#define TEST_GT911_EXPECTED_ADDRESS 0x5D
#else
#define TEST_GT911_EXPECTED_ADDRESS 0x14
#endif
#endif

void setUp() {}
void tearDown() {}

void test_probe_plan_contains_only_configured_address() {
    constexpr auto plan = Gt911ProbePolicy::configuredAddressOnly(
        static_cast<uint8_t>(ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS));

    TEST_ASSERT_EQUAL_HEX8(TEST_GT911_EXPECTED_ADDRESS, plan.address);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_probe_plan_contains_only_configured_address);
    return UNITY_END();
}
