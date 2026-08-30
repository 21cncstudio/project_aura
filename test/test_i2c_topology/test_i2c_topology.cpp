#include <unity.h>

#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/SensorI2cBus.h"

void setUp() {
    I2cMock::reset();
}

void tearDown() {}

void test_panel_bus_remains_on_vendor_i2c0() {
    TEST_ASSERT_EQUAL_INT(I2C_NUM_0, Config::I2C_PORT);
    TEST_ASSERT_EQUAL_UINT8(8U, Config::I2C_SDA_PIN);
    TEST_ASSERT_EQUAL_UINT8(9U, Config::I2C_SCL_PIN);
    TEST_ASSERT_EQUAL_UINT32(100000U, Config::I2C_FREQ_HZ);
}

void test_sensor_topology_matches_hardware_profile() {
#if AURA_HARDWARE_PROFILE_7
    TEST_ASSERT_TRUE(Config::SENSOR_I2C_SEPARATE);
    TEST_ASSERT_EQUAL_INT(I2C_NUM_1, Config::SENSOR_I2C_PORT);
    TEST_ASSERT_EQUAL_UINT8(44U, Config::SENSOR_I2C_SDA_PIN);
    TEST_ASSERT_EQUAL_UINT8(6U, Config::SENSOR_I2C_SCL_PIN);
    TEST_ASSERT_FALSE(Config::SENSOR_I2C_INTERNAL_PULLUPS);
#else
    TEST_ASSERT_FALSE(Config::SENSOR_I2C_SEPARATE);
    TEST_ASSERT_EQUAL_INT(Config::I2C_PORT, Config::SENSOR_I2C_PORT);
    TEST_ASSERT_EQUAL_UINT8(Config::I2C_SDA_PIN, Config::SENSOR_I2C_SDA_PIN);
    TEST_ASSERT_EQUAL_UINT8(Config::I2C_SCL_PIN, Config::SENSOR_I2C_SCL_PIN);
#endif
    TEST_ASSERT_EQUAL_UINT32(100000U, Config::SENSOR_I2C_FREQ_HZ);
}

void test_sensor_host_initialization_matches_hardware_profile() {
    const SensorI2cBus::Result result = SensorI2cBus::begin();
    TEST_ASSERT_TRUE(result.ready());

#if AURA_HARDWARE_PROFILE_7
    TEST_ASSERT_TRUE(result.separate);
    TEST_ASSERT_EQUAL_UINT32(1U, I2cMock::parameterConfigCount());
    TEST_ASSERT_EQUAL_UINT32(1U, I2cMock::driverInstallCount());
    TEST_ASSERT_EQUAL_INT(I2C_NUM_1, I2cMock::configuredPort());
    TEST_ASSERT_EQUAL_INT(I2C_NUM_1, I2cMock::installedPort());
    const i2c_config_t &config = I2cMock::configuredConfig();
    TEST_ASSERT_EQUAL_INT(I2C_MODE_MASTER, config.mode);
    TEST_ASSERT_EQUAL_INT(44, config.sda_io_num);
    TEST_ASSERT_EQUAL_INT(6, config.scl_io_num);
    TEST_ASSERT_EQUAL_INT(GPIO_PULLUP_DISABLE, config.sda_pullup_en);
    TEST_ASSERT_EQUAL_INT(GPIO_PULLUP_DISABLE, config.scl_pullup_en);
    TEST_ASSERT_EQUAL_UINT32(100000U, config.master.clk_speed);
#else
    TEST_ASSERT_FALSE(result.separate);
    TEST_ASSERT_EQUAL_UINT32(0U, I2cMock::parameterConfigCount());
    TEST_ASSERT_EQUAL_UINT32(0U, I2cMock::driverInstallCount());
#endif
}

void test_sensor_host_configuration_failure_stops_before_install() {
#if AURA_HARDWARE_PROFILE_7
    I2cMock::setParameterConfigResult(ESP_ERR_INVALID_ARG);
    const SensorI2cBus::Result result = SensorI2cBus::begin();
    TEST_ASSERT_FALSE(result.ready());
    TEST_ASSERT_TRUE(result.configuration_attempted);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, result.configuration_error);
    TEST_ASSERT_FALSE(result.installation_attempted);
    TEST_ASSERT_EQUAL_UINT32(1U, I2cMock::parameterConfigCount());
    TEST_ASSERT_EQUAL_UINT32(0U, I2cMock::driverInstallCount());
#else
    TEST_PASS();
#endif
}

void test_sensor_host_install_failure_is_reported() {
#if AURA_HARDWARE_PROFILE_7
    I2cMock::setDriverInstallResult(ESP_ERR_NO_MEM);
    const SensorI2cBus::Result result = SensorI2cBus::begin();
    TEST_ASSERT_FALSE(result.ready());
    TEST_ASSERT_TRUE(result.configuration_attempted);
    TEST_ASSERT_EQUAL_INT(ESP_OK, result.configuration_error);
    TEST_ASSERT_TRUE(result.installation_attempted);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NO_MEM, result.installation_error);
    TEST_ASSERT_EQUAL_UINT32(1U, I2cMock::parameterConfigCount());
    TEST_ASSERT_EQUAL_UINT32(1U, I2cMock::driverInstallCount());
#else
    TEST_PASS();
#endif
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_panel_bus_remains_on_vendor_i2c0);
    RUN_TEST(test_sensor_topology_matches_hardware_profile);
    RUN_TEST(test_sensor_host_initialization_matches_hardware_profile);
    RUN_TEST(test_sensor_host_configuration_failure_stops_before_install);
    RUN_TEST(test_sensor_host_install_failure_is_reported);
    return UNITY_END();
}
