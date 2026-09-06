#include <unity.h>

#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/I2CHelper.h"
#include "drivers/Bmp3xxProbe.h"
#include "drivers/Ds3231.h"
#include "drivers/Gp8403.h"
#include "drivers/Pcf8523.h"

void setUp() {
    I2cMock::reset();
}

void tearDown() {}

void assertLastTransactionUsedSensorBus() {
    TEST_ASSERT_EQUAL_INT(Config::SENSOR_I2C_PORT,
                          I2cMock::lastTransactionPort());
}

void test_generic_sensirion_transport_uses_sensor_bus() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        I2C::write_cmd(Config::SEN66_ADDR,
                       Config::SEN66_CMD_DATA_READY,
                       nullptr,
                       0));
    assertLastTransactionUsedSensorBus();

    uint8_t data[3] = {};
    TEST_ASSERT_EQUAL_INT(
        ESP_OK,
        I2C::read_bytes(Config::SEN66_ADDR, data, sizeof(data)));
    assertLastTransactionUsedSensorBus();
}

void test_rtc_dac_and_pressure_probe_use_sensor_bus() {
    I2cMock::setDevicePresent(Config::PCF8523_ADDR, true);
    Pcf8523 pcf8523;
    TEST_ASSERT_TRUE(pcf8523.begin());
    assertLastTransactionUsedSensorBus();

    I2cMock::setDevicePresent(Config::DS3231_ADDR, true);
    Ds3231 ds3231;
    uint8_t metadata[4] = {};
    TEST_ASSERT_TRUE(ds3231.readProbeMeta(metadata));
    assertLastTransactionUsedSensorBus();

    I2cMock::setDevicePresent(Config::DAC_I2C_ADDR_DEFAULT, true);
    Gp8403 dac;
    TEST_ASSERT_TRUE(dac.begin(Config::DAC_I2C_ADDR_DEFAULT));
    assertLastTransactionUsedSensorBus();

    I2cMock::setDevicePresent(Config::BMP3XX_ADDR_PRIMARY, true);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY,
                         Config::BMP3XX_REG_CHIP_ID,
                         Config::BMP3XX_CHIP_ID_BMP388);
    Bmp3xxProbe::Variant variant = Bmp3xxProbe::Variant::Unknown;
    TEST_ASSERT_TRUE(Bmp3xxProbe::detect(Config::BMP3XX_ADDR_PRIMARY,
                                         variant));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Bmp3xxProbe::Variant::BMP388),
                          static_cast<int>(variant));
    assertLastTransactionUsedSensorBus();

    const i2c_port_t other_port = Config::SENSOR_I2C_PORT == I2C_NUM_0
                                      ? I2C_NUM_1
                                      : I2C_NUM_0;
    TEST_ASSERT_GREATER_THAN_UINT32(
        0U, I2cMock::transactionCount(Config::SENSOR_I2C_PORT));
    TEST_ASSERT_EQUAL_UINT32(0U, I2cMock::transactionCount(other_port));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_generic_sensirion_transport_uses_sensor_bus);
    RUN_TEST(test_rtc_dac_and_pressure_probe_use_sensor_bus);
    return UNITY_END();
}
