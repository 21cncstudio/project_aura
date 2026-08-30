#include <unity.h>

#include "drivers/DfrGasProbeLogPolicy.h"

void setUp() {}
void tearDown() {}

void test_optional_never_present_nack_on_idle_bus_is_expected_absence() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        true,
        false,
        true,
        true,
        true,
    };

    TEST_ASSERT_TRUE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_TRUE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

void test_required_slot_nack_remains_a_warning() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        false,
        false,
        true,
        true,
        true,
    };

    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

void test_previously_present_optional_sensor_loss_remains_a_warning() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        true,
        true,
        true,
        true,
        true,
    };

    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

void test_optional_timeout_remains_a_warning() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        true,
        false,
        false,
        true,
        true,
    };

    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

void test_low_line_before_probe_is_diagnostic_only_after_bus_recovers() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        true,
        false,
        true,
        false,
        true,
    };

    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_TRUE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

void test_low_line_after_probe_remains_a_warning() {
    const DfrGasProbeLogPolicy::ProbeFailure failure{
        true,
        false,
        true,
        true,
        false,
    };

    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsence(failure));
    TEST_ASSERT_FALSE(
        DfrGasProbeLogPolicy::isExpectedOptionalAbsenceAfterProbe(failure));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_optional_never_present_nack_on_idle_bus_is_expected_absence);
    RUN_TEST(test_required_slot_nack_remains_a_warning);
    RUN_TEST(test_previously_present_optional_sensor_loss_remains_a_warning);
    RUN_TEST(test_optional_timeout_remains_a_warning);
    RUN_TEST(test_low_line_before_probe_is_diagnostic_only_after_bus_recovers);
    RUN_TEST(test_low_line_after_probe_remains_a_warning);
    return UNITY_END();
}
