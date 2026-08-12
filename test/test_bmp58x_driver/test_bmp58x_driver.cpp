#include <unity.h>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/Logger.h"

// Pull in the production BMP58x implementation directly so this suite can
// verify the real startup path without interfering with mock-based tests.
#define USE_REAL_BMP580_HEADER
#define Bmp580 RealBmp580
#include "../../src/drivers/Bmp580.cpp"
#undef Bmp580
#undef USE_REAL_BMP580_HEADER

namespace {

void seedBmp58x(uint8_t chip_id, uint8_t status) {
    I2cMock::setDevicePresent(Config::BMP580_ADDR_PRIMARY, true);
    I2cMock::setRegister(Config::BMP580_ADDR_PRIMARY, Config::BMP580_REG_CHIP_ID, chip_id);
    I2cMock::setRegister(Config::BMP580_ADDR_PRIMARY, Config::BMP580_REG_STATUS, status);
    I2cMock::setRegister(Config::BMP580_ADDR_PRIMARY, Config::BMP580_REG_DSP_IIR, 0x00);
    I2cMock::setRegister(Config::BMP580_ADDR_PRIMARY, Config::BMP580_REG_ODR_CONFIG, 0x00);
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

void test_real_bmp58x_start_fails_when_nvm_ready_never_asserts() {
    seedBmp58x(Config::BMP580_CHIP_ID_PRIMARY, 0x00);

    RealBmp580 bmp58x;
    TEST_ASSERT_TRUE(bmp58x.begin());
    TEST_ASSERT_FALSE(bmp58x.start());
    TEST_ASSERT_FALSE(bmp58x.isOk());
}

void test_real_bmp58x_start_succeeds_when_nvm_ready_is_set() {
    seedBmp58x(Config::BMP580_CHIP_ID_SECONDARY, Config::BMP580_STATUS_NVM_RDY);

    RealBmp580 bmp58x;
    TEST_ASSERT_TRUE(bmp58x.begin());
    TEST_ASSERT_TRUE(bmp58x.start());
    TEST_ASSERT_TRUE(bmp58x.isOk());
    TEST_ASSERT_EQUAL_STRING("BMP585", bmp58x.variantLabel());
}

void test_real_bmp58x_cooperative_late_start_completes_after_reset_wait() {
    seedBmp58x(Config::BMP580_CHIP_ID_SECONDARY,
               Config::BMP580_STATUS_NVM_RDY);

    RealBmp580 bmp58x;
    TEST_ASSERT_TRUE(bmp58x.begin());
    bmp58x.beginLateStart();
    CooperativeStart::Result result = CooperativeStart::Result::InProgress;
    for (size_t i = 0; i < 64U && result == CooperativeStart::Result::InProgress; ++i) {
        result = bmp58x.pollLateStart(getMillis());
        advanceMillis(1U);
    }

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(result));
    TEST_ASSERT_TRUE(bmp58x.isOk());
    TEST_ASSERT_EQUAL_STRING("BMP585", bmp58x.variantLabel());
}

void test_real_bmp58x_cooperative_nvm_timeout_is_nonblocking() {
    seedBmp58x(Config::BMP580_CHIP_ID_PRIMARY, 0x00);
    RealBmp580 bmp58x;
    TEST_ASSERT_TRUE(bmp58x.begin());
    bmp58x.beginLateStart();

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(bmp58x.pollLateStart(getMillis()))); // Detect.
    TEST_ASSERT_EQUAL_UINT32(0U, getMillis());
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(bmp58x.pollLateStart(getMillis()))); // Reset.
    TEST_ASSERT_EQUAL_UINT32(0U, getMillis());
    setMillis(5U);
    bmp58x.pollLateStart(getMillis()); // Arm NVM deadline.
    bmp58x.pollLateStart(getMillis()); // First status read.
    TEST_ASSERT_EQUAL_UINT32(5U, getMillis());
    setMillis(25U);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Failed),
                      static_cast<int>(bmp58x.pollLateStart(getMillis())));
    TEST_ASSERT_FALSE(bmp58x.isOk());
}

void test_real_bmp58x_cooperative_reset_wait_handles_millis_rollover() {
    seedBmp58x(Config::BMP580_CHIP_ID_PRIMARY,
               Config::BMP580_STATUS_NVM_RDY);
    constexpr uint32_t start_ms = UINT32_MAX - 2U;
    setMillis(start_ms);

    RealBmp580 bmp58x;
    TEST_ASSERT_TRUE(bmp58x.begin());
    bmp58x.beginLateStart();
    bmp58x.pollLateStart(getMillis()); // Detect.
    bmp58x.pollLateStart(getMillis()); // Reset; due wraps to 2.
    setMillis(1U);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(bmp58x.pollLateStart(getMillis())));
    setMillis(2U);
    bmp58x.pollLateStart(getMillis());

    CooperativeStart::Result result = CooperativeStart::Result::InProgress;
    for (size_t i = 0; i < 32U && result == CooperativeStart::Result::InProgress; ++i) {
        result = bmp58x.pollLateStart(getMillis());
    }
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(result));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_real_bmp58x_start_fails_when_nvm_ready_never_asserts);
    RUN_TEST(test_real_bmp58x_start_succeeds_when_nvm_ready_is_set);
    RUN_TEST(test_real_bmp58x_cooperative_late_start_completes_after_reset_wait);
    RUN_TEST(test_real_bmp58x_cooperative_nvm_timeout_is_nonblocking);
    RUN_TEST(test_real_bmp58x_cooperative_reset_wait_handles_millis_rollover);
    return UNITY_END();
}
