#include <unity.h>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/Logger.h"

#define USE_REAL_DPS310_HEADER
#define Dps310 RealDps310
#include "../../src/drivers/Dps310.cpp"
#undef Dps310
#undef USE_REAL_DPS310_HEADER

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
}

void tearDown() {}

void test_real_dps310_cooperative_late_start_completes_one_step_per_poll() {
    I2cMock::setDevicePresent(Config::DPS310_ADDR_PRIMARY, true);
    I2cMock::setRegister(Config::DPS310_ADDR_PRIMARY,
                         Config::DPS310_PRODREVID,
                         0x10);
    I2cMock::setRegister(Config::DPS310_ADDR_PRIMARY,
                         Config::DPS310_MEASCFG,
                         0xC0);
    uint8_t calibration[18] = {};
    I2cMock::setRegisters(Config::DPS310_ADDR_PRIMARY,
                          0x10,
                          calibration,
                          sizeof(calibration));

    RealDps310 dps310;
    TEST_ASSERT_TRUE(dps310.begin());
    dps310.beginLateStart();
    CooperativeStart::Result result = CooperativeStart::Result::InProgress;
    for (size_t i = 0; i < 128U && result == CooperativeStart::Result::InProgress; ++i) {
        result = dps310.pollLateStart(getMillis());
        advanceMillis(1U);
    }

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(result));
    TEST_ASSERT_TRUE(dps310.isOk());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_real_dps310_cooperative_late_start_completes_one_step_per_poll);
    return UNITY_END();
}
