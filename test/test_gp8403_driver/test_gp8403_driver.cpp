#include <unity.h>

#include "I2cMock.h"
#include "config/AppConfig.h"
#include "drivers/Gp8403.h"

void setUp() {
    I2cMock::reset();
}

void tearDown() {}

void test_begin_rejects_zero_address_without_bus_traffic() {
    Gp8403 dac;

    TEST_ASSERT_FALSE(dac.begin(0));
    TEST_ASSERT_EQUAL_UINT32(0, I2cMock::transactionCount());
}

void test_begin_uses_address_only_probe_without_register_read() {
    I2cMock::setDevicePresent(Config::DAC_I2C_ADDR_DEFAULT, true);
    I2cMock::setReadFailure(
        Config::DAC_I2C_ADDR_DEFAULT,
        Config::DAC_REG_OUTPUT_RANGE,
        true);

    Gp8403 dac;

    TEST_ASSERT_TRUE(dac.begin(Config::DAC_I2C_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL_UINT32(
        1,
        I2cMock::addressOnlyProbeCount(Config::DAC_I2C_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL_UINT32(1, I2cMock::transactionCount());
}

void test_begin_fails_when_address_does_not_ack() {
    Gp8403 dac;

    TEST_ASSERT_FALSE(dac.begin(Config::DAC_I2C_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL_UINT32(
        1,
        I2cMock::addressOnlyProbeCount(Config::DAC_I2C_ADDR_DEFAULT));
}

void test_range_command_keeps_documented_register_and_value() {
    I2cMock::setDevicePresent(Config::DAC_I2C_ADDR_DEFAULT, true);
    Gp8403 dac;
    TEST_ASSERT_TRUE(dac.begin(Config::DAC_I2C_ADDR_DEFAULT));

    TEST_ASSERT_TRUE(dac.setOutputRange10V());
    TEST_ASSERT_EQUAL_HEX8(
        Config::DAC_RANGE_10V,
        I2cMock::getRegister(
            Config::DAC_I2C_ADDR_DEFAULT,
            Config::DAC_REG_OUTPUT_RANGE));
}

void test_channel_commands_keep_documented_registers_and_little_endian_payload() {
    I2cMock::setDevicePresent(Config::DAC_I2C_ADDR_DEFAULT, true);
    Gp8403 dac;
    TEST_ASSERT_TRUE(dac.begin(Config::DAC_I2C_ADDR_DEFAULT));

    TEST_ASSERT_TRUE(dac.writeChannelRaw12(Config::DAC_CHANNEL_VOUT0, 0x0ABC));
    TEST_ASSERT_EQUAL_HEX8(
        0xC0,
        I2cMock::getRegister(
            Config::DAC_I2C_ADDR_DEFAULT,
            Config::DAC_REG_CHANNEL_0));
    TEST_ASSERT_EQUAL_HEX8(
        0xAB,
        I2cMock::getRegister(
            Config::DAC_I2C_ADDR_DEFAULT,
            static_cast<uint8_t>(Config::DAC_REG_CHANNEL_0 + 1)));

    TEST_ASSERT_TRUE(dac.writeChannelRaw12(Config::DAC_CHANNEL_VOUT1, 0x1234));
    TEST_ASSERT_EQUAL_HEX8(
        0xF0,
        I2cMock::getRegister(
            Config::DAC_I2C_ADDR_DEFAULT,
            Config::DAC_REG_CHANNEL_1));
    TEST_ASSERT_EQUAL_HEX8(
        0xFF,
        I2cMock::getRegister(
            Config::DAC_I2C_ADDR_DEFAULT,
            static_cast<uint8_t>(Config::DAC_REG_CHANNEL_1 + 1)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_rejects_zero_address_without_bus_traffic);
    RUN_TEST(test_begin_uses_address_only_probe_without_register_read);
    RUN_TEST(test_begin_fails_when_address_does_not_ack);
    RUN_TEST(test_range_command_keeps_documented_register_and_value);
    RUN_TEST(test_channel_commands_keep_documented_registers_and_little_endian_payload);
    return UNITY_END();
}
