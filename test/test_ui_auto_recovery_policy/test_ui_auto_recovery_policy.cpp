#include <unity.h>

#include "config/AppConfig.h"
#include "core/BoardInitPolicy.h"
#include "core/BoardRecoveryPolicy.h"
#include "core/BootState.h"
#include "ui/UiAutoRecoveryPolicy.h"

using UiAutoRecoveryPolicy::Decision;

void setUp() {
    boot_ui_auto_recovery_reboot = false;
    boot_board_auto_recovery_reboot = false;
}
void tearDown() {}

void test_normal_boot_requests_one_restart() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::RequestRestart),
        static_cast<int>(UiAutoRecoveryPolicy::decide(false, false)));
}

void test_recovery_boot_suppresses_restart_loop() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressRecoveryBoot),
        static_cast<int>(UiAutoRecoveryPolicy::decide(true, false)));
}

void test_duplicate_request_is_suppressed() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressAlreadyRequested),
        static_cast<int>(UiAutoRecoveryPolicy::decide(false, true)));
}

void test_either_recovery_marker_suppresses_cross_recovery() {
    boot_board_auto_recovery_reboot = true;
    TEST_ASSERT_TRUE(boot_any_auto_recovery_boot());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressRecoveryBoot),
        static_cast<int>(UiAutoRecoveryPolicy::decide(
            boot_any_auto_recovery_boot(), false)));

    boot_board_auto_recovery_reboot = false;
    boot_ui_auto_recovery_reboot = true;
    TEST_ASSERT_TRUE(boot_any_auto_recovery_boot());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::SuppressRecoveryBoot),
        static_cast<int>(UiAutoRecoveryPolicy::decide(
            boot_any_auto_recovery_boot(), false)));
}

void test_recovery_marker_does_not_bypass_disabled_preinit_recovery() {
    boot_ui_auto_recovery_reboot = true;
    BoardInitPolicy::PreInitI2cSamples samples{};
    samples.pre_init_sda_high = false;
    samples.pre_init_scl_high = true;

    const bool auto_recovery_boot = boot_any_auto_recovery_boot();
    TEST_ASSERT_FALSE(Config::PANEL_PREINIT_BUS_RECOVERY_ENABLED);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BoardInitPolicy::PreInitAction::VendorInit),
        static_cast<int>(BoardInitPolicy::preInitAction(
            auto_recovery_boot && Config::PANEL_PREINIT_BUS_RECOVERY_ENABLED,
            samples)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BoardRecoveryPolicy::Decision::SuppressPolicyDisabled),
        static_cast<int>(BoardRecoveryPolicy::decide(
            false,
            false,
            false,
            auto_recovery_boot,
            true,
            Config::PANEL_STARTUP_AUTO_RESTART_ENABLED)));
}

void test_explicit_diagnostic_preinit_recovery_remains_available() {
    BoardInitPolicy::PreInitI2cSamples samples{};
    samples.pre_init_sda_high = false;
    samples.pre_init_scl_high = true;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BoardInitPolicy::PreInitAction::RecoverThenVendorInit),
        static_cast<int>(BoardInitPolicy::preInitAction(true, samples)));

    samples.pre_init_sda_high = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BoardInitPolicy::PreInitAction::VendorInit),
        static_cast<int>(BoardInitPolicy::preInitAction(true, samples)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_normal_boot_requests_one_restart);
    RUN_TEST(test_recovery_boot_suppresses_restart_loop);
    RUN_TEST(test_duplicate_request_is_suppressed);
    RUN_TEST(test_either_recovery_marker_suppresses_cross_recovery);
    RUN_TEST(test_recovery_marker_does_not_bypass_disabled_preinit_recovery);
    RUN_TEST(test_explicit_diagnostic_preinit_recovery_remains_available);
    return UNITY_END();
}
