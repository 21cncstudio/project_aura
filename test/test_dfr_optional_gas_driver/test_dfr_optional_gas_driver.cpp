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

void setCommandResponseAt(uint8_t address,
                          uint8_t command,
                          const uint8_t *frame,
                          size_t len) {
    I2cMock::setCommandRead(address, command, frame, len);
}

void setCommandResponse(uint8_t command, const uint8_t *frame, size_t len) {
    setCommandResponseAt(Config::DFR_OPTIONAL_GAS_ADDR,
                         command,
                         frame,
                         len);
}

void setReadGasResponseAt(uint8_t address,
                          uint16_t raw_ppm,
                          uint8_t gas_type,
                          uint8_t decimals) {
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
    setCommandResponseAt(address,
                         Config::DFR_GAS_CMD_READ_GAS,
                         frame,
                         sizeof(frame));
}

void setReadGasResponse(uint16_t raw_ppm, uint8_t gas_type, uint8_t decimals) {
    setReadGasResponseAt(Config::DFR_OPTIONAL_GAS_ADDR,
                         raw_ppm,
                         gas_type,
                         decimals);
}

void setBadChecksumGasResponseAt(uint8_t address,
                                 uint16_t raw_ppm,
                                 uint8_t gas_type,
                                 uint8_t decimals) {
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
    frame[8] = static_cast<uint8_t>(checksum7(frame) ^ 0x5A);
    setCommandResponseAt(address,
                         Config::DFR_GAS_CMD_READ_GAS,
                         frame,
                         sizeof(frame));
}

enum class StartupResponse : uint8_t {
    Valid = 0,
    BadHeader,
    WriteFailure,
    ReadFailure,
    BadChecksum,
    BadDecimals,
};

void configureStartupResponse(uint8_t address,
                              uint8_t gas_type,
                              StartupResponse response) {
    I2cMock::setDevicePresent(address, true);
    if (response == StartupResponse::WriteFailure) {
        I2cMock::setWriteFailure(address, 0x00, true);
        return;
    }
    if (response == StartupResponse::ReadFailure) {
        I2cMock::setReadFailure(address, 0x00, true);
        return;
    }

    uint8_t frame[9] = {
        0xFF,
        Config::DFR_GAS_CMD_READ_GAS,
        0x00,
        0x7B,
        gas_type,
        static_cast<uint8_t>(
            response == StartupResponse::BadDecimals ? 0x03 : 0x01),
        0x00,
        0x00,
        0x00,
    };
    frame[8] = checksum7(frame);
    if (response == StartupResponse::BadHeader) {
        frame[0] = 0x00;
    } else if (response == StartupResponse::BadChecksum) {
        frame[8] ^= 0x5A;
    }
    I2cMock::setCommandRead(address,
                            Config::DFR_GAS_CMD_READ_GAS,
                            frame,
                            sizeof(frame));
}

void startSen0466WithValidSample(Sen0466 &sensor,
                                 uint16_t raw_ppm = 42,
                                 uint8_t decimals = 1) {
    configureStartupResponse(Config::SEN0466_ADDR,
                             Config::DFR_GAS_TYPE_CO,
                             StartupResponse::Valid);
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         raw_ppm,
                         Config::DFR_GAS_TYPE_CO,
                         decimals);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_TRUE(sensor.isDataValid());
}

template <typename Sensor>
void assertStartupNeverChangesMode(uint8_t address,
                                   uint8_t gas_type,
                                   StartupResponse response,
                                   bool reinitialize) {
    setMillis(0);
    I2cMock::reset();
    Sensor sensor;

    if (reinitialize) {
        configureStartupResponse(address, gas_type, StartupResponse::Valid);
        TEST_ASSERT_TRUE(sensor.begin());
        TEST_ASSERT_TRUE(sensor.start());
        TEST_ASSERT_EQUAL_UINT32(
            0U,
            I2cMock::sensorCommandCount(
                address, Config::DFR_GAS_CMD_CHANGE_MODE));
        I2cMock::reset();
    }

    configureStartupResponse(address, gas_type, response);
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::sensorCommandCount(address, Config::DFR_GAS_CMD_READ_GAS));
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        I2cMock::sensorCommandCount(
            address, Config::DFR_GAS_CMD_CHANGE_MODE));
}

} // namespace

static_assert(Config::DFR_GAS_TYPE_NH3 == 0x02, "DFR NH3 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_H2S == 0x03, "DFR H2S gas type drifted");
static_assert(Config::DFR_GAS_TYPE_O2 == 0x05, "DFR O2 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_O3 == 0x2A, "DFR O3 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_SO2 == 0x2B, "DFR SO2 gas type drifted");
static_assert(Config::DFR_GAS_TYPE_NO2 == 0x2C, "DFR NO2 gas type drifted");
static_assert(Config::SEN0466_RUNTIME_TRANSPORT_RETRY_DELAY_MS == 150,
              "SEN0466 runtime retry delay drifted");

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
}

void tearDown() {}

void test_fresh_startup_is_read_only_for_both_dfr_addresses() {
    assertStartupNeverChangesMode<DfrOptionalGasSensor>(
        Config::DFR_OPTIONAL_GAS_ADDR,
        Config::DFR_GAS_TYPE_NH3,
        StartupResponse::Valid,
        false);
    assertStartupNeverChangesMode<Sen0466>(
        Config::SEN0466_ADDR,
        Config::DFR_GAS_TYPE_CO,
        StartupResponse::Valid,
        false);
}

void test_reinitialized_startup_is_read_only_for_both_dfr_addresses() {
    assertStartupNeverChangesMode<DfrOptionalGasSensor>(
        Config::DFR_OPTIONAL_GAS_ADDR,
        Config::DFR_GAS_TYPE_NH3,
        StartupResponse::BadHeader,
        true);
    assertStartupNeverChangesMode<Sen0466>(
        Config::SEN0466_ADDR,
        Config::DFR_GAS_TYPE_CO,
        StartupResponse::BadHeader,
        true);
}

template <typename Sensor>
void assertStartupFailureMatrixNeverChangesMode(uint8_t address,
                                                uint8_t gas_type) {
    const StartupResponse failures[] = {
        StartupResponse::BadHeader,
        StartupResponse::WriteFailure,
        StartupResponse::ReadFailure,
        StartupResponse::BadChecksum,
        StartupResponse::BadDecimals,
    };
    for (const StartupResponse failure : failures) {
        assertStartupNeverChangesMode<Sensor>(
            address, gas_type, failure, false);
    }
}

void test_optional_gas_startup_failures_never_change_mode() {
    assertStartupFailureMatrixNeverChangesMode<DfrOptionalGasSensor>(
        Config::DFR_OPTIONAL_GAS_ADDR,
        Config::DFR_GAS_TYPE_NH3);
}

void test_sen0466_startup_failures_never_change_mode() {
    assertStartupFailureMatrixNeverChangesMode<Sen0466>(
        Config::SEN0466_ADDR,
        Config::DFR_GAS_TYPE_CO);
}

void test_sen0466_warmup_waits_for_first_accepted_post_boundary_sample() {
    configureStartupResponse(Config::SEN0466_ADDR,
                             Config::DFR_GAS_TYPE_CO,
                             StartupResponse::Valid);
    Sen0466 sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         42,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    setMillis(Config::DFR_GAS_WARMUP_MS - Config::DFR_GAS_POLL_MS);
    sensor.poll();
    const uint32_t pre_boundary_sample_ms = sensor.lastDataMs();
    TEST_ASSERT_TRUE(sensor.isWarmupActive());
    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());

    // Reaching the deadline must not promote the accepted pre-boundary sample.
    setMillis(Config::DFR_GAS_WARMUP_MS);
    TEST_ASSERT_TRUE(sensor.isWarmupActive());
    TEST_ASSERT_FALSE(sensor.isDataValid());

    // A rejected post-boundary frame must not finish warmup either.
    setBadChecksumGasResponseAt(Config::SEN0466_ADDR,
                                57,
                                Config::DFR_GAS_TYPE_CO,
                                1);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isWarmupActive());
    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL_UINT32(pre_boundary_sample_ms, sensor.lastDataMs());

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_FALSE(sensor.isWarmupActive());
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
    TEST_ASSERT_TRUE(sensor.lastDataMs() > pre_boundary_sample_ms);
}

void test_sen0466_warmup_bad_frames_exit_to_invalid_after_stale_grace() {
    configureStartupResponse(Config::SEN0466_ADDR,
                             Config::DFR_GAS_TYPE_CO,
                             StartupResponse::Valid);
    Sen0466 sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setBadChecksumGasResponseAt(Config::SEN0466_ADDR,
                                57,
                                Config::DFR_GAS_TYPE_CO,
                                1);
    setMillis(Config::DFR_GAS_WARMUP_MS);
    for (uint8_t failed_poll = 0;
         failed_poll < Config::DFR_GAS_MAX_FAILS;
         ++failed_poll) {
        sensor.poll();
        TEST_ASSERT_TRUE(sensor.isWarmupActive());
        TEST_ASSERT_FALSE(sensor.isDataValid());
        if (failed_poll + 1U < Config::DFR_GAS_MAX_FAILS) {
            advanceMillis(Config::DFR_GAS_POLL_MS);
        }
    }

    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_STALE_MS - 1U);
    TEST_ASSERT_TRUE(sensor.isWarmupActive());
    TEST_ASSERT_FALSE(sensor.isDataValid());

    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_STALE_MS);
    TEST_ASSERT_FALSE(sensor.isWarmupActive());
    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_TRUE(sensor.isPresent());
}

void test_optional_gas_detects_nh3_after_warmup() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);

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
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::None),
                      static_cast<int>(sensor.gasType()));
    TEST_ASSERT_EQUAL_UINT8(0, sensor.rawGasType());
    TEST_ASSERT_EQUAL_UINT32(0, sensor.lastDataMs());
}

void test_optional_gas_rejects_unsupported_gas_type() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);

    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());

    setReadGasResponse(42, Config::DFR_GAS_TYPE_CO, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::None),
                      static_cast<int>(sensor.gasType()));
    TEST_ASSERT_EQUAL_UINT8(0, sensor.rawGasType());
    TEST_ASSERT_EQUAL_UINT32(0, sensor.lastDataMs());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::None),
                      static_cast<int>(sensor.optionalGasType()));
}

void test_optional_gas_semantic_reject_invalidates_without_committing_frame() {
    configureStartupResponse(Config::DFR_OPTIONAL_GAS_ADDR,
                             Config::DFR_GAS_TYPE_NH3,
                             StartupResponse::Valid);
    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());
    setReadGasResponse(42, Config::DFR_GAS_TYPE_NH3, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());

    const uint32_t accepted_ms = sensor.lastDataMs();
    setReadGasResponse(209, Config::DFR_GAS_TYPE_CO, 1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.ppm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_NH3,
                            sensor.rawGasType());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::NH3),
                      static_cast<int>(sensor.gasType()));
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
}

void test_sen0466_two_semantic_rejects_preserve_last_good_and_valid_resets_policy() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);

    const uint32_t accepted_ms = sensor.lastDataMs();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(1, sensor.ppmDecimals());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::CO),
                      static_cast<int>(sensor.gasType()));

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         209,
                         Config::DFR_GAS_TYPE_O2,
                         1);
    for (uint8_t reject = 0; reject < 2; ++reject) {
        const uint32_t commands_before = I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();

        TEST_ASSERT_EQUAL_UINT32(
            1U,
            I2cMock::sensorCommandCount(
                Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
                commands_before);
        TEST_ASSERT_TRUE(sensor.isDataValid());
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
        TEST_ASSERT_EQUAL_UINT8(1, sensor.ppmDecimals());
        TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO,
                                sensor.rawGasType());
        TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::CO),
                          static_cast<int>(sensor.gasType()));
        TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
    }

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_TRUE(sensor.lastDataMs() > accepted_ms);

    // The accepted sample resets the consecutive-failure count completely.
    const uint32_t second_accepted_ms = sensor.lastDataMs();
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         209,
                         Config::DFR_GAS_TYPE_O2,
                         1);
    for (uint8_t reject = 0; reject < 2; ++reject) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();
        TEST_ASSERT_TRUE(sensor.isDataValid());
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
        TEST_ASSERT_EQUAL_UINT32(second_accepted_ms, sensor.lastDataMs());
    }
}

void test_sen0466_third_semantic_reject_invalidates_then_valid_sample_recovers() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    const uint32_t accepted_ms = sensor.lastDataMs();

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         209,
                         Config::DFR_GAS_TYPE_O2,
                         1);
    for (uint8_t reject = 0; reject < Config::DFR_GAS_MAX_FAILS; ++reject) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();
        if (reject + 1U < Config::DFR_GAS_MAX_FAILS) {
            TEST_ASSERT_TRUE(sensor.isDataValid());
        }
    }

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrMultiGasSensor::GasType::CO),
                      static_cast<int>(sensor.gasType()));
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());

    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
    sensor.poll();
    TEST_ASSERT_FALSE(sensor.isDataValid());
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();

    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_TRUE(sensor.lastDataMs() > accepted_ms);
}

void test_sen0466_valid_catch_up_after_stale_interval_commits_without_invalid_result() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    const uint32_t accepted_ms = sensor.lastDataMs();
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);

    advanceMillis(Config::DFR_GAS_STALE_MS + 1U);
    sensor.poll();

    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_TRUE(sensor.lastDataMs() > accepted_ms);
}

void test_sen0466_failed_catch_up_after_stale_interval_fails_closed_without_retrying_frame_error() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    const uint32_t accepted_ms = sensor.lastDataMs();
    setBadChecksumGasResponseAt(Config::SEN0466_ADDR,
                                57,
                                Config::DFR_GAS_TYPE_CO,
                                1);

    advanceMillis(Config::DFR_GAS_STALE_MS + 1U);
    const uint32_t commands_before = I2cMock::sensorCommandCount(
        Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before);
    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
}

void test_sen0466_retries_one_runtime_write_failure_once() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    I2cMock::setWriteErrorOnCall(Config::SEN0466_ADDR,
                                 0x00,
                                 1,
                                 ESP_FAIL);

    advanceMillis(Config::DFR_GAS_POLL_MS);
    const uint32_t poll_started_ms = millis();
    const uint32_t commands_before = I2cMock::sensorCommandCount(
        Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before);
    TEST_ASSERT_EQUAL_UINT32(
        Config::SEN0466_RUNTIME_TRANSPORT_RETRY_DELAY_MS +
            Config::DFR_GAS_CMD_DELAY_MS,
        millis() - poll_started_ms);
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_CHANGE_MODE));
}

void test_sen0466_retries_one_runtime_read_timeout_once() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    I2cMock::setReadErrorOnCall(Config::SEN0466_ADDR,
                                0x00,
                                1,
                                ESP_ERR_TIMEOUT);

    advanceMillis(Config::DFR_GAS_POLL_MS);
    const uint32_t poll_started_ms = millis();
    const uint32_t commands_before = I2cMock::sensorCommandCount(
        Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        2U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before);
    TEST_ASSERT_EQUAL_UINT32(
        Config::SEN0466_RUNTIME_TRANSPORT_RETRY_DELAY_MS +
            (2U * Config::DFR_GAS_CMD_DELAY_MS),
        millis() - poll_started_ms);
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.7f, sensor.coPpm());
}

void test_sen0466_does_not_retry_nontransient_i2c_error() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    const uint32_t accepted_ms = sensor.lastDataMs();
    I2cMock::setWriteErrorOnCall(Config::SEN0466_ADDR,
                                 0x00,
                                 1,
                                 ESP_ERR_INVALID_ARG);

    advanceMillis(Config::DFR_GAS_POLL_MS);
    const uint32_t poll_started_ms = millis();
    const uint32_t commands_before = I2cMock::sensorCommandCount(
        Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before);
    TEST_ASSERT_EQUAL_UINT32(0U, millis() - poll_started_ms);
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
}

void test_optional_gas_does_not_use_sen0466_runtime_retry() {
    configureStartupResponse(Config::DFR_OPTIONAL_GAS_ADDR,
                             Config::DFR_GAS_TYPE_NH3,
                             StartupResponse::Valid);
    DfrOptionalGasSensor sensor;
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.start());
    setReadGasResponse(42, Config::DFR_GAS_TYPE_NH3, 1);
    setMillis(Config::DFR_GAS_WARMUP_MS + Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
    const uint32_t accepted_ms = sensor.lastDataMs();

    I2cMock::setWriteErrorOnCall(Config::DFR_OPTIONAL_GAS_ADDR,
                                 0x00,
                                 1,
                                 ESP_FAIL);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    const uint32_t poll_started_ms = millis();
    const uint32_t commands_before = I2cMock::sensorCommandCount(
        Config::DFR_OPTIONAL_GAS_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::sensorCommandCount(
            Config::DFR_OPTIONAL_GAS_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before);
    TEST_ASSERT_EQUAL_UINT32(0U, millis() - poll_started_ms);
    TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.ppm());
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
}

void test_sen0466_two_failed_attempts_count_as_one_failed_poll() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    const uint32_t accepted_ms = sensor.lastDataMs();
    I2cMock::setWriteFailure(Config::SEN0466_ADDR, 0x00, true);

    for (uint8_t failed_poll = 0;
         failed_poll < Config::DFR_GAS_MAX_FAILS;
         ++failed_poll) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        const uint32_t commands_before = I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
        sensor.poll();
        TEST_ASSERT_EQUAL_UINT32(
            2U,
            I2cMock::sensorCommandCount(
                Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
                commands_before);
        if (failed_poll + 1U < Config::DFR_GAS_MAX_FAILS) {
            TEST_ASSERT_TRUE(sensor.isDataValid());
        }
    }

    TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, sensor.coPpm());
    TEST_ASSERT_EQUAL_UINT8(Config::DFR_GAS_TYPE_CO, sensor.rawGasType());
    TEST_ASSERT_EQUAL_UINT32(accepted_ms, sensor.lastDataMs());
}

void test_sen0466_address_ack_does_not_reset_failed_poll_streak() {
    Sen0466 sensor;
    startSen0466WithValidSample(sensor, 42, 1);
    I2cMock::setWriteFailure(Config::SEN0466_ADDR, 0x00, true);
    for (uint8_t failed_poll = 0;
         failed_poll < Config::DFR_GAS_MAX_FAILS;
         ++failed_poll) {
        advanceMillis(Config::DFR_GAS_POLL_MS);
        sensor.poll();
    }
    TEST_ASSERT_FALSE(sensor.isDataValid());

    I2cMock::setWriteFailure(Config::SEN0466_ADDR, 0x00, false);
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         209,
                         Config::DFR_GAS_TYPE_O2,
                         1);
    advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
    sensor.poll();

    const uint32_t commands_before_reject = I2cMock::sensorCommandCount(
        Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS) -
            commands_before_reject);
    TEST_ASSERT_FALSE(sensor.isDataValid());

    // The rejected frame immediately re-enters cooldown because an address
    // ACK is not a semantically valid sample and must not reset fail_count_.
    setReadGasResponseAt(Config::SEN0466_ADDR,
                         57,
                         Config::DFR_GAS_TYPE_CO,
                         1);
    const uint32_t commands_before_cooldown_poll =
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS);
    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_EQUAL_UINT32(
        commands_before_cooldown_poll,
        I2cMock::sensorCommandCount(
            Config::SEN0466_ADDR, Config::DFR_GAS_CMD_READ_GAS));
    TEST_ASSERT_FALSE(sensor.isDataValid());
}

void test_optional_gas_clamps_detected_type_range() {
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);

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

void test_optional_gas_cooldown_recovery_is_read_only_when_address_acks() {
    setMillis(1000);
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);

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

    const uint32_t mode_writes_before_recovery =
        I2cMock::sensorCommandCount(
            Config::DFR_OPTIONAL_GAS_ADDR,
            Config::DFR_GAS_CMD_CHANGE_MODE);
    const uint32_t transactions_before_recovery =
        I2cMock::transactionCount();
    I2cMock::setWriteFailure(Config::DFR_OPTIONAL_GAS_ADDR, 0x00, false);
    advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
    sensor.poll();

    TEST_ASSERT_EQUAL_UINT32(
        1U,
        I2cMock::transactionCount() - transactions_before_recovery);
    TEST_ASSERT_EQUAL_UINT32(
        mode_writes_before_recovery,
        I2cMock::sensorCommandCount(
            Config::DFR_OPTIONAL_GAS_ADDR,
            Config::DFR_GAS_CMD_CHANGE_MODE));
    TEST_ASSERT_TRUE(sensor.isPresent());
    TEST_ASSERT_EQUAL(static_cast<int>(DfrOptionalGasSensor::OptionalGasType::NH3),
                      static_cast<int>(sensor.optionalGasType()));

    advanceMillis(Config::DFR_GAS_POLL_MS);
    sensor.poll();
    TEST_ASSERT_TRUE(sensor.isDataValid());
}

void test_optional_gas_marks_absent_when_recovery_fails_after_startup_grace_and_no_ack() {
    setMillis(1000);
    I2cMock::setDevicePresent(Config::DFR_OPTIONAL_GAS_ADDR, true);

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

    const uint32_t mode_writes_before_recovery =
        I2cMock::sensorCommandCount(
            Config::DFR_OPTIONAL_GAS_ADDR,
            Config::DFR_GAS_CMD_CHANGE_MODE);
    for (uint8_t i = 0; i < Config::DFR_GAS_MAX_COOLDOWN_RECOVERY_FAILS; ++i) {
        advanceMillis(Config::DFR_GAS_FAIL_COOLDOWN_MS);
        sensor.poll();
    }
    TEST_ASSERT_FALSE(sensor.isPresent());
    TEST_ASSERT_EQUAL_UINT32(
        mode_writes_before_recovery,
        I2cMock::sensorCommandCount(
            Config::DFR_OPTIONAL_GAS_ADDR,
            Config::DFR_GAS_CMD_CHANGE_MODE));
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
    advanceMillis(Config::DFR_GAS_ABSENT_RETRY_MS);
    sensor.poll();

    TEST_ASSERT_FALSE(sensor.isPresent());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_startup_is_read_only_for_both_dfr_addresses);
    RUN_TEST(test_reinitialized_startup_is_read_only_for_both_dfr_addresses);
    RUN_TEST(test_optional_gas_startup_failures_never_change_mode);
    RUN_TEST(test_sen0466_startup_failures_never_change_mode);
    RUN_TEST(test_sen0466_warmup_waits_for_first_accepted_post_boundary_sample);
    RUN_TEST(test_sen0466_warmup_bad_frames_exit_to_invalid_after_stale_grace);
    RUN_TEST(test_optional_gas_detects_nh3_after_warmup);
    RUN_TEST(test_optional_gas_detects_so2_after_warmup);
    RUN_TEST(test_optional_gas_detects_o3_after_warmup);
    RUN_TEST(test_optional_gas_preserves_dfrobot_decimal_places);
    RUN_TEST(test_optional_gas_detects_h2s_after_warmup);
    RUN_TEST(test_optional_gas_detects_no2_after_warmup);
    RUN_TEST(test_optional_gas_detects_o2_after_warmup);
    RUN_TEST(test_dedicated_co_sensor_rejects_o2_type);
    RUN_TEST(test_optional_gas_rejects_unsupported_gas_type);
    RUN_TEST(test_optional_gas_semantic_reject_invalidates_without_committing_frame);
    RUN_TEST(test_sen0466_two_semantic_rejects_preserve_last_good_and_valid_resets_policy);
    RUN_TEST(test_sen0466_third_semantic_reject_invalidates_then_valid_sample_recovers);
    RUN_TEST(test_sen0466_valid_catch_up_after_stale_interval_commits_without_invalid_result);
    RUN_TEST(test_sen0466_failed_catch_up_after_stale_interval_fails_closed_without_retrying_frame_error);
    RUN_TEST(test_sen0466_retries_one_runtime_write_failure_once);
    RUN_TEST(test_sen0466_retries_one_runtime_read_timeout_once);
    RUN_TEST(test_sen0466_does_not_retry_nontransient_i2c_error);
    RUN_TEST(test_optional_gas_does_not_use_sen0466_runtime_retry);
    RUN_TEST(test_sen0466_two_failed_attempts_count_as_one_failed_poll);
    RUN_TEST(test_sen0466_address_ack_does_not_reset_failed_poll_streak);
    RUN_TEST(test_optional_gas_clamps_detected_type_range);
    RUN_TEST(test_optional_gas_cooldown_recovery_is_read_only_when_address_acks);
    RUN_TEST(test_optional_gas_marks_absent_when_recovery_fails_after_startup_grace_and_no_ack);
    RUN_TEST(test_optional_gas_retries_after_absent_start_lockout);
    RUN_TEST(test_optional_gas_stops_absent_retry_after_limit);
    return UNITY_END();
}
