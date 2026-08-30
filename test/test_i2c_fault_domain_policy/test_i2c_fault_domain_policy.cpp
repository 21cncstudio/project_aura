#include <unity.h>

#include "core/I2cFaultDomainPolicy.h"

void setUp() {}
void tearDown() {}

void test_shared_profile_follows_panel_readiness() {
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::sensorRuntimeReady(
        false, true, false));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::sensorRuntimeReady(
        false, false, true));
    TEST_ASSERT_TRUE(
        I2cFaultDomainPolicy::panelFailureDisablesSensorDomain(false));
}

void test_separate_profile_follows_sensor_host_readiness() {
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::sensorRuntimeReady(
        true, false, true));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::sensorRuntimeReady(
        true, true, false));
    TEST_ASSERT_FALSE(
        I2cFaultDomainPolicy::panelFailureDisablesSensorDomain(true));
}

void test_shared_shutdown_requires_both_domains_and_lvgl_pause() {
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
        false, true, true));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
        false, false, true));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
        false, true, false));
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::lvglPauseSatisfiedForSensorOutput(
        false, true));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::lvglPauseSatisfiedForSensorOutput(
        false, false));
}

void test_separate_shutdown_ignores_panel_state_but_requires_sensor_idle() {
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
        true, false, true));
    TEST_ASSERT_FALSE(I2cFaultDomainPolicy::sensorBusExclusiveForShutdown(
        true, true, false));
    TEST_ASSERT_TRUE(I2cFaultDomainPolicy::lvglPauseSatisfiedForSensorOutput(
        true, false));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_shared_profile_follows_panel_readiness);
    RUN_TEST(test_separate_profile_follows_sensor_host_readiness);
    RUN_TEST(test_shared_shutdown_requires_both_domains_and_lvgl_pause);
    RUN_TEST(test_separate_shutdown_ignores_panel_state_but_requires_sensor_idle);
    return UNITY_END();
}
