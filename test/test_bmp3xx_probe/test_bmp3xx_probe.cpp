#include <unity.h>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "drivers/Bmp3xxProbe.h"

#define USE_REAL_BMP3XX_HEADER
#define Bmp3xx RealBmp3xx
#include "../../src/drivers/Bmp3xx.cpp"
#undef Bmp3xx
#undef USE_REAL_BMP3XX_HEADER

namespace {

void seedBmp3xxRegisters(uint8_t addr, uint8_t chip_id) {
    I2cMock::setDevicePresent(addr, true);
    I2cMock::setRegister(addr, Config::BMP3XX_REG_CHIP_ID, chip_id);
    I2cMock::setRegister(addr, Config::BMP3XX_REG_ERR, 0x00);
    I2cMock::setRegister(addr, Config::BMP3XX_REG_PWR_CTRL, 0x33);
    I2cMock::setRegister(addr, Config::BMP3XX_REG_OSR, 0x3F);
    I2cMock::setRegister(addr, Config::BMP3XX_REG_ODR, 0x1F);
}

} // namespace

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
}

void tearDown() {}

void test_bmp3xx_probe_accepts_bmp388_with_non_default_valid_config_bits() {
    seedBmp3xxRegisters(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_CHIP_ID_BMP388);

    Bmp3xxProbe::Variant variant = Bmp3xxProbe::Variant::Unknown;
    TEST_ASSERT_TRUE(Bmp3xxProbe::detect(Config::BMP3XX_ADDR_PRIMARY, variant));
    TEST_ASSERT_EQUAL(static_cast<int>(Bmp3xxProbe::Variant::BMP388),
                      static_cast<int>(variant));
}

void test_bmp3xx_probe_accepts_bmp390_with_non_default_valid_config_bits() {
    seedBmp3xxRegisters(Config::BMP3XX_ADDR_ALT, Config::BMP3XX_CHIP_ID_BMP390);

    Bmp3xxProbe::Variant variant = Bmp3xxProbe::Variant::Unknown;
    TEST_ASSERT_TRUE(Bmp3xxProbe::detect(Config::BMP3XX_ADDR_ALT, variant));
    TEST_ASSERT_EQUAL(static_cast<int>(Bmp3xxProbe::Variant::BMP390),
                      static_cast<int>(variant));
}

void test_bmp3xx_probe_rejects_shared_address_device_with_matching_first_byte_only() {
    I2cMock::setDevicePresent(Config::BMP3XX_ADDR_PRIMARY, true);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_CHIP_ID,
                         Config::BMP3XX_CHIP_ID_BMP390);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_ERR, 0xA5);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_PWR_CTRL, 0xFF);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_OSR, 0xC1);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY, Config::BMP3XX_REG_ODR, 0xE0);

    Bmp3xxProbe::Variant variant = Bmp3xxProbe::Variant::BMP388;
    TEST_ASSERT_FALSE(Bmp3xxProbe::detect(Config::BMP3XX_ADDR_PRIMARY, variant));
    TEST_ASSERT_EQUAL(static_cast<int>(Bmp3xxProbe::Variant::Unknown),
                      static_cast<int>(variant));
}

void test_real_bmp3xx_cooperative_late_start_completes_without_blocking_delay() {
    seedBmp3xxRegisters(Config::BMP3XX_ADDR_PRIMARY,
                        Config::BMP3XX_CHIP_ID_BMP390);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY,
                         Config::BMP3XX_REG_STATUS,
                         Config::BMP3XX_STATUS_CMD_RDY);
    uint8_t calibration[21] = {};
    I2cMock::setRegisters(Config::BMP3XX_ADDR_PRIMARY,
                          Config::BMP3XX_REG_CALIB_DATA,
                          calibration,
                          sizeof(calibration));

    RealBmp3xx bmp3xx;
    TEST_ASSERT_TRUE(bmp3xx.begin());
    bmp3xx.beginLateStart();
    CooperativeStart::Result result = CooperativeStart::Result::InProgress;
    for (size_t i = 0; i < 64U && result == CooperativeStart::Result::InProgress; ++i) {
        result = bmp3xx.pollLateStart(getMillis());
        advanceMillis(1U);
    }

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(result));
    TEST_ASSERT_TRUE(bmp3xx.isOk());
    TEST_ASSERT_EQUAL_STRING("BMP390", bmp3xx.variantLabel());
}

void test_real_bmp3xx_cooperative_late_start_rejects_final_error_read_failure() {
    seedBmp3xxRegisters(Config::BMP3XX_ADDR_PRIMARY,
                        Config::BMP3XX_CHIP_ID_BMP390);
    I2cMock::setRegister(Config::BMP3XX_ADDR_PRIMARY,
                         Config::BMP3XX_REG_STATUS,
                         Config::BMP3XX_STATUS_CMD_RDY);
    uint8_t calibration[21] = {};
    I2cMock::setRegisters(Config::BMP3XX_ADDR_PRIMARY,
                          Config::BMP3XX_REG_CALIB_DATA,
                          calibration,
                          sizeof(calibration));
    // The first ERR read belongs to address qualification; fail the final
    // post-configuration ERR read instead.
    I2cMock::setReadFailureOnCall(Config::BMP3XX_ADDR_PRIMARY,
                                  Config::BMP3XX_REG_ERR,
                                  2U);

    RealBmp3xx bmp3xx;
    TEST_ASSERT_TRUE(bmp3xx.begin());
    bmp3xx.beginLateStart();
    CooperativeStart::Result result = CooperativeStart::Result::InProgress;
    for (size_t i = 0; i < 64U && result == CooperativeStart::Result::InProgress; ++i) {
        result = bmp3xx.pollLateStart(getMillis());
        advanceMillis(1U);
    }

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Failed),
                      static_cast<int>(result));
    TEST_ASSERT_FALSE(bmp3xx.isOk());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_bmp3xx_probe_accepts_bmp388_with_non_default_valid_config_bits);
    RUN_TEST(test_bmp3xx_probe_accepts_bmp390_with_non_default_valid_config_bits);
    RUN_TEST(test_bmp3xx_probe_rejects_shared_address_device_with_matching_first_byte_only);
    RUN_TEST(test_real_bmp3xx_cooperative_late_start_completes_without_blocking_delay);
    RUN_TEST(test_real_bmp3xx_cooperative_late_start_rejects_final_error_read_failure);
    return UNITY_END();
}
