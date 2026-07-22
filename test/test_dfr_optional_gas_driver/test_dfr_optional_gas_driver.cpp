#include <unity.h>
#include <cstring>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "drivers/Sen0466.h"

namespace {

uint8_t checksum7(const uint8_t *frame) {
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 7; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    return static_cast<uint8_t>(~sum + 1);
}

void setCommandResponse(uint8_t command, const uint8_t *frame, size_t len) {
    I2cMock::setCommandRead(Config::DFR_OPTIONAL_GAS_ADDR, command, frame, len);
}

void setPassiveModeAck() {
    uint8_t frame[9] = {
        0xFF, Config::DFR_GAS_CMD_CHANGE_MODE, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    frame[8] = checksum7(frame);
    setCommandResponse(Config::DFR_GAS_CMD_CHANGE_MODE, frame, sizeof(frame));
}

void setReadGasResponse(uint16_t raw_ppm, uint8_t gas_type, uint8_t decimals) {
    uint8_t frame[9] = {
        0xFF,
        Config::DFR_GAS_CMD_READ_GAS,
        static_cast<uint8_t>(raw_ppm >> 8),
        static_cast<uint8_t>(raw_ppm & 0xFF),
        gas_type,
        decimals,
        0x00,
        0x00,
        0x00,
    };
    frame[8] = checksum7(frame);
    setCommandResponse(Config::DFR_GAS_CMD_READ_GAS, frame, sizeof(frame));
}

} // namespace

static_assert(Config::DFR_GAS_TYPE_NH3 == 0x02, "DFR NH3 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_H2S == 0x03, "DFR H2S gas type drifted");
static_assert(Config::DFR_GAS_TYPE_O2 == 0x05, "DFR O2 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_O3 == 0x2A, "DFR O3 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_SO2 == 0x2B, "DFR SO2 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_NO2 == 0x2C, "DFR NO2 gas type drifted");

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
}

void tearDown() {}

void test_optional_gas_detects_nh3_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());
    TEST_ASSERT_TRUE(sensor.isPresent());

    setReadGasResponse(123, Config::DFR_GAS_TYPE_NH3, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.3f, sensor.ppm());
    TEST_ASSERT_EQUAL_STRING("NH3", sensor.optionalGasLabel());
}

void test_optional_gas_detects_so2_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(75, Config::DFR_GAS_TYPE_SO2, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::SO2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, sensor.ppm());
    TEST_ASSERT_EQUAL_STRING("SO2", sensor.optionalGasLabel());
}

void test_optional_gas_detects_o3_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(42, Config::DFR_GAS_TYPE_O3, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::O3),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.ppm());
    TEST_ASSERT_EQUAL_UINT8(1, sensor.ppmDecimals());
    TEST_ASSERT_EQUAL_STRING("O3", sensor.optionalGasLabel());
}

void test_optional_gas_preserves_dfrobot_decimal_places() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(23, Config::DFR_GAS_TYPE_O3, 2);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.23f, sensor.ppm());
    TEST_ASSERT_EQUAL_UINT8(2, sensor.ppmDecimals());
}

void test_optional_gas_detects_h2s_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(84, Config::DFR_GAS_TYPE_H2S, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::H2S),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.4f, sensor.ppm());
    TEST_ASSERT_EQUAL_STRING("H2S", sensor.optionalGasLabel());
}

void test_optional_gas_detects_no2_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(55, Config::DFR_GAS_TYPE_NO2, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NO2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.5f, sensor.ppm());
    TEST_ASSERT_EQUAL_STRING("NO2", sensor.optionalGasLabel());
}

void test_optional_gas_detects_o2_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(209, Config::DFR_GAS_TYPE_O2, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::O2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.9f, sensor.concentration());
    TEST_ASSERT_EQUAL_UINT8(1, sensor.concentrationDecimals());
    TEST_ASSERT_EQUAL_STRING("O2", sensor.optionalGasLabel());
    TEST_ASSERT_EQUAL_STRING("%Vol", DfrOptionalGasSensor::unitForType(sensor.optionalGasType()));
}

void test_dedicated_co_sensor_rejects_o2_type() {
    I2cMock::setDevicePresent(Config::SEN0466_ADDR, true);

    uint8_t passive_ack[9] = {
        0xFF, Config::DFR_GAS_CMD_CHANGE_MODE, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    passive_ack[8] = checksum7(passive_ack);
    I2cMock::setCommandRead(Config::SEN0466_ADDR,
                            Config::DFR_GAS_CMD_CHANGE_MODE,
                            passive_ack,
                            sizeof(passive_ack));

    Sen0466 sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    uint8_t o2_response[9] = {
        0xFF,
        Config::DFR_GAS_CMD_READ_GAS,
        0x00,
        0xD1,
        Config::DFR_GAS_TYPE_O2,
        0x01,
        0x00,
        0x00,
        0x00,
    };
    o2_response[8] = checksum7(o2_response);
    I2cMock::setCommandRead(Config::SEN0466_ADDR,
                            Config::DFR_GAS_CMD_READ_GAS,
                            o2_response,
                            sizeof(o2_response));

    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::O2),
                      static_cast<int>(sensor.gasType()));
}

void test_optional_gas_rejects_unsupported_gas_type() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(42, Config::DFR_GAS_TYPE_CO, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::CO),
                      static_cast<int>(sensor.gasType()));
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::None),
                      static_cast<int>(sensor.optionalGasType()));
}

void test_optional_gas_clamps_detected_type_range() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(999, Config::DFR_GAS_TYPE_O3, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::O3),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SEN0472_O3_MAX_PPM, sensor.ppm());

    setReadGasResponse(500, Config::DFR_GAS_TYPE_SO2, 1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::SO2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SEN0470_SO2_MAX_PPM, sensor.ppm());

    setReadGasResponse(500, Config::DFR_GAS_TYPE_NO2, 1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NO2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SEN0471_NO2_MAX_PPM, sensor.ppm());

    setReadGasResponse(300, Config::DFR_GAS_TYPE_O2, 1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::O2),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, Config::SEN0465_O2_MAX_PERCENT_VOL, sensor.concentration());
}

void test_optional_gas_keeps_known_type_when_recovery_fails_but_address_acks() {
    setMillis(1000);
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(123, Config::DFR_GAS_TYPE_NH3, 1);
    setMillis(1000 + Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));

    I2cMock::setWriteFailure(Config::DFR_OPTIONAL_GAS_ADDR, 0x00, true);
    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_FAILS; ++i) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();
    }
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));

    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_COOLDOWN_RECOVERY_FAILS; ++i) {
        advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
        sensor.poll();
    }
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));
}

void test_optional_gas_marks_absent_when_recovery_fails_after_startup_grace_and_no_ack() {
    setMillis(1000);
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(123, Config::DFR_GAS_TYPE_NH3, 1);
    setMillis(1000 + Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));

    setMillis(1000 + Config::DFR_GAS_STARTUP_FAULT_GRACE_MS + Config::DFR_GAS_POLL_MS);
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, false);
    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_FAILS; ++i) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();
    }
    TEST_ASSERT_TRUE(sensor.isPresent());

    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_COOLDOWN_RECOVERY_FAILS; ++i) {
        advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
        sensor.poll();
    }
    TEST_ASSERT_FALSE(sensor.isPresent());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::None),
                      static_cast<int>(sensor.optionalGasType()));
}

void test_optional_gas_retries_after_absent_start_lockout() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, false);

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_START_ATTEMPTS; ++i) {
        TEST_ASSERT_FALSE(sensor.start());
    }

    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();

    advanceMillis(Config::DFR_GAS_RETRY_MS);
    sensor.poll();
    TEST_ASSERT_FALSE(sensor.isPresent());

    advanceMillis(Config::DFR_GAS_ABSENT_RETRY_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isPresent());
}

void test_optional_gas_stops_absent_retry_after_limit() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, false);

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_START_ATTEMPTS; ++i) {
        TEST_ASSERT_FALSE(sensor.start());
    }

    for (uint8_t retry = 0; retry < Config::DFR_GAS_MAX_ABSENT_RETRIES; ++retry) {
        advanceMillis(Config::DFR_GAS_ABSENT_RETRY_MS);
        for (uint8_t attempt = 0; attempt < Config::DFR_GAS_MAX_START_ATTEMPTS; ++attempt) {
            sensor.poll();
            TEST_ASSERT_FALSE(sensor.isPresent());
            advanceMillis(Config::DFR_GAS_RETRY_MS);
        }
    }

    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);
    setPassiveModeAck();
    advanceMillis(Config::DFR_GAS_ABSENT_RETRY_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isPresent());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_optional_gas_detects_nh3_after_warmup);
    RUN_TEST(test_optional_gas_detects_so2_after_warmup);
    RUN_TEST(test_optional_gas_detects_o3_after_warmup);
    RUN_TEST(test_optional_gas_preserves_dfrobot_decimal_places);
    RUN_TEST(test_optional_gas_detects_h2s_after_warmup);
    RUN_TEST(test_optional_gas_detects_no2_after_warmup);
    RUN_TEST(test_optional_gas_detects_o2_after_warmup);
    RUN_TEST(test_dedicated_co_sensor_rejects_o2_type);
    RUN_TEST(test_optional_gas_rejects_unsupported_gas_type);
    RUN_TEST(test_optional_gas_clamps_detected_type_range);
    RUN_TEST(test_optional_gas_keeps_known_type_when_recovery_fails_but_address_acks);
    RUN_TEST(test_optional_gas_marks_absent_when_recovery_fails_after_startup_grace_and_no_ack);
    RUN_TEST(test_optional_gas_retries_after_absent_start_lockout);
    RUN_TEST(test_optional_gas_stops_absent_retry_after_limit);
    return UNITY_END();
}
