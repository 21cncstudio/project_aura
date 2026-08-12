#include <unity.h>

#include "core/BoardInitPolicy.h"
#include "core/PowerStartPolicy.h"

using BoardInitPolicy::BeginOutcome;
using BoardInitPolicy::CompletionAction;

void setUp() {}
void tearDown() {}

void test_power_start_classifies_cold_sw_from_missing_retained_marker() {
    const auto classification = PowerStartPolicy::classify(false, false, false);
    TEST_ASSERT_TRUE(classification.board_cold_start);
    TEST_ASSERT_TRUE(classification.peripherals_cold_start);
}

void test_power_start_keeps_brownout_peripherals_in_unknown_retained_state() {
    const auto classification = PowerStartPolicy::classify(true, false, true);
    TEST_ASSERT_TRUE(classification.board_cold_start);
    TEST_ASSERT_FALSE(classification.peripherals_cold_start);
}

void test_brownout_remains_conservative_when_retained_marker_is_missing() {
    const auto classification = PowerStartPolicy::classify(false, false, true);
    TEST_ASSERT_TRUE(classification.board_cold_start);
    TEST_ASSERT_FALSE(classification.peripherals_cold_start);
}

void test_power_start_classifies_normal_software_restart_as_warm() {
    const auto classification = PowerStartPolicy::classify(true, false, false);
    TEST_ASSERT_FALSE(classification.board_cold_start);
    TEST_ASSERT_FALSE(classification.peripherals_cold_start);
}

void test_power_on_is_cold_for_board_and_peripherals() {
    const auto classification = PowerStartPolicy::classify(true, true, false);
    TEST_ASSERT_TRUE(classification.board_cold_start);
    TEST_ASSERT_TRUE(classification.peripherals_cold_start);
}

void test_success_uses_initialized_board() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionAction::UseBoard),
        static_cast<int>(BoardInitPolicy::completionAction(BeginOutcome::Success)));
}

void test_normal_failure_deletes_failed_board_without_retry_policy() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionAction::DeleteBoard),
        static_cast<int>(BoardInitPolicy::completionAction(BeginOutcome::Failed)));
}

void test_task_creation_failure_deletes_unstarted_board() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionAction::DeleteBoard),
        static_cast<int>(BoardInitPolicy::completionAction(BeginOutcome::TaskCreateFailed)));
}

void test_timeout_retains_unsafe_board_until_restart() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionAction::RetainUntilRestart),
        static_cast<int>(BoardInitPolicy::completionAction(BeginOutcome::Timeout)));
}

void test_i2c_gpio_recovery_is_only_used_on_marked_recovery_boot() {
    BoardInitPolicy::PreInitI2cSamples samples{};
    samples.pre_init_sda_high = false;
    samples.pre_init_scl_high = true;
    TEST_ASSERT_TRUE(BoardInitPolicy::shouldRecoverI2cBeforeInit(true, samples));

    samples.pre_init_sda_high = true;
    samples.pre_init_scl_high = false;
    TEST_ASSERT_TRUE(BoardInitPolicy::shouldRecoverI2cBeforeInit(true, samples));

    samples.pre_init_sda_high = true;
    samples.pre_init_scl_high = true;
    TEST_ASSERT_FALSE(BoardInitPolicy::shouldRecoverI2cBeforeInit(true, samples));

    samples.pre_init_sda_high = false;
    samples.pre_init_scl_high = false;
    TEST_ASSERT_FALSE(BoardInitPolicy::shouldRecoverI2cBeforeInit(false, samples));
}

void test_i2c_recovery_decision_uses_fresh_pre_init_sample_not_early_diagnostic() {
    BoardInitPolicy::PreInitI2cSamples samples{};
    samples.early_diagnostic_sda_high = false;
    samples.early_diagnostic_scl_high = false;
    samples.pre_init_sda_high = true;
    samples.pre_init_scl_high = true;
    TEST_ASSERT_FALSE(BoardInitPolicy::shouldRecoverI2cBeforeInit(true, samples));

    samples.early_diagnostic_sda_high = true;
    samples.early_diagnostic_scl_high = true;
    samples.pre_init_sda_high = false;
    samples.pre_init_scl_high = true;
    TEST_ASSERT_TRUE(BoardInitPolicy::shouldRecoverI2cBeforeInit(true, samples));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_power_start_classifies_cold_sw_from_missing_retained_marker);
    RUN_TEST(test_power_start_keeps_brownout_peripherals_in_unknown_retained_state);
    RUN_TEST(test_brownout_remains_conservative_when_retained_marker_is_missing);
    RUN_TEST(test_power_start_classifies_normal_software_restart_as_warm);
    RUN_TEST(test_power_on_is_cold_for_board_and_peripherals);
    RUN_TEST(test_success_uses_initialized_board);
    RUN_TEST(test_normal_failure_deletes_failed_board_without_retry_policy);
    RUN_TEST(test_task_creation_failure_deletes_unstarted_board);
    RUN_TEST(test_timeout_retains_unsafe_board_until_restart);
    RUN_TEST(test_i2c_gpio_recovery_is_only_used_on_marked_recovery_boot);
    RUN_TEST(test_i2c_recovery_decision_uses_fresh_pre_init_sample_not_early_diagnostic);
    return UNITY_END();
}
