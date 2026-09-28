#include <unity.h>
#include "ArduinoMock.h"
#include "I2cMock.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "drivers/Sen0321.h"
#include "drivers/OptionalGasSensor.h"

void setUp() {
    setMillis(0); I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
}
void tearDown() {}
static void reading(uint16_t ppb) {
    const uint8_t bytes[] = {uint8_t(ppb >> 8), uint8_t(ppb)};
    I2cMock::setRegisters(Sen0321::Address, 0x07, bytes, 2);
}
static void start(Sen0321 &sensor) {
    I2cMock::setDevicePresent(Sen0321::Address, true);
    sensor.begin(); TEST_ASSERT_TRUE(sensor.start());
    sensor.poll(); // configure passive mode
}
static void sample(Sen0321 &sensor) {
    advanceMillis(1000); sensor.poll();
    advanceMillis(100); sensor.poll();
    advanceMillis(100); sensor.poll();
}
static void ready(Sen0321 &sensor) {
    start(sensor); reading(37);
    advanceMillis(Sen0321::WarmupMs); sample(sensor);
    TEST_ASSERT_TRUE(sensor.isDataValid());
}
void test_passive_protocol_timing_warmup_and_big_endian() {
    Sen0321 sensor; start(sensor); reading(0x012c); // 300 ppb
    TEST_ASSERT_EQUAL_UINT8(1, I2cMock::getRegister(0x73, 0x03));
    TEST_ASSERT_TRUE(sensor.isWarmupActive()); TEST_ASSERT_FALSE(sensor.isDataValid());
    const auto count = I2cMock::transactionCount();
    advanceMillis(999); sensor.poll(); TEST_ASSERT_EQUAL_UINT32(count, I2cMock::transactionCount());
    advanceMillis(1); sensor.poll(); TEST_ASSERT_EQUAL_UINT8(1, I2cMock::getRegister(0x73, 0x04));
    advanceMillis(99); sensor.poll(); TEST_ASSERT_EQUAL_UINT32(count + 1, I2cMock::transactionCount());
    advanceMillis(1); sensor.poll();
    advanceMillis(99); sensor.poll(); TEST_ASSERT_EQUAL_UINT32(count + 2, I2cMock::transactionCount());
    advanceMillis(1); sensor.poll();
    TEST_ASSERT_EQUAL_UINT32(count + 3, I2cMock::transactionCount());
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.300f, sensor.ppm());
    TEST_ASSERT_FALSE(sensor.isDataValid()); TEST_ASSERT_EQUAL_UINT32(0, sensor.lastDataMs());
    setMillis(Sen0321::WarmupMs); sample(sensor);
    TEST_ASSERT_TRUE(sensor.isDataValid()); TEST_ASSERT_FALSE(sensor.isWarmupActive());
    TEST_ASSERT_EQUAL_UINT32(getMillis(), sensor.lastDataMs());
    TEST_ASSERT_EQUAL(Config::SENSOR_I2C_PORT, I2cMock::lastTransactionPort());
}
void test_zero_floor_and_full_range_are_not_corrected() {
    Sen0321 sensor; ready(sensor);
    for (uint16_t ppb : {0, 10, 20, 37, 10000}) {
        reading(ppb); sample(sensor); TEST_ASSERT_TRUE(sensor.isDataValid());
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, ppb / 1000.0f, sensor.ppm());
    }
}
void test_range_and_read_failures_never_publish_false_zero() {
    for (int failure = 0; failure < 4; ++failure) {
        setUp(); Sen0321 sensor; ready(sensor); const auto fresh = sensor.lastDataMs();
        if (failure == 0) reading(10001);
        if (failure == 1) reading(65535);
        if (failure == 2) I2cMock::setReadFailure(0x73, 0x07, true);
        if (failure == 3) I2cMock::setRawReadLimit(0x73, 1);
        sample(sensor);
        TEST_ASSERT_FALSE(sensor.isDataValid()); TEST_ASSERT_EQUAL_UINT32(fresh, sensor.lastDataMs());
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.037f, sensor.ppm());
    }
}
// Called just after successful passive-mode configuration. Check that neither
// elapsed time while absent nor the old valid sample satisfies the new warmup.
static void require_full_warmup(Sen0321 &sensor, uint32_t previous_data_ms) {
    const uint32_t configured_at = getMillis();
    sample(sensor);
    TEST_ASSERT_TRUE(sensor.isWarmupActive()); TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL_UINT32(previous_data_ms, sensor.lastDataMs());
    setMillis(configured_at + Sen0321::WarmupMs - 1201);
    sample(sensor); // full reading at 179999 ms after configuration
    TEST_ASSERT_TRUE(sensor.isWarmupActive()); TEST_ASSERT_FALSE(sensor.isDataValid());
    TEST_ASSERT_EQUAL_UINT32(previous_data_ms, sensor.lastDataMs());
    sample(sensor);
    TEST_ASSERT_FALSE(sensor.isWarmupActive()); TEST_ASSERT_TRUE(sensor.isDataValid());
    TEST_ASSERT_EQUAL_UINT32(getMillis(), sensor.lastDataMs());
}
void test_one_or_two_failures_require_full_warmup_after_return() {
    for (int failures : {1, 2}) {
        setUp();
        // Recovery warmup itself crosses millis rollover in this scenario.
        setMillis(UINT32_MAX - Sen0321::WarmupMs - 30000);
        Sen0321 sensor; ready(sensor);
        const auto previous_data_ms = sensor.lastDataMs();
        I2cMock::setDevicePresent(0x73, false); sample(sensor);
        for (int i = 1; i < failures; ++i) { advanceMillis(5000); sensor.poll(); }
        TEST_ASSERT_FALSE(sensor.isDataValid()); TEST_ASSERT_TRUE(sensor.isWarmupActive());
        I2cMock::setDevicePresent(0x73, true);
        advanceMillis(5000); sensor.poll(); // successful reconfiguration
        require_full_warmup(sensor, previous_data_ms);
    }
}
void test_long_pause_after_failure_does_not_count_as_recovery_warmup() {
    Sen0321 sensor; ready(sensor);
    const auto previous_data_ms = sensor.lastDataMs();
    I2cMock::setDevicePresent(0x73, false); sample(sensor);
    advanceMillis(2 * Sen0321::WarmupMs); // no polling during the outage
    I2cMock::setDevicePresent(0x73, true); sensor.poll();
    require_full_warmup(sensor, previous_data_ms);
}
void test_delayed_initial_configuration_starts_its_own_warmup() {
    Sen0321 sensor;
    I2cMock::setDevicePresent(0x73, true);
    sensor.begin(); TEST_ASSERT_TRUE(sensor.start()); reading(52);
    advanceMillis(2 * Sen0321::WarmupMs); sensor.poll();
    require_full_warmup(sensor, 0);
}
void test_recovery_after_power_loss_requires_new_warmup() {
    Sen0321 sensor; ready(sensor);
    const auto previous_data_ms = sensor.lastDataMs();
    I2cMock::setDevicePresent(0x73, false); sample(sensor);
    for (int i = 0; i < 2; ++i) { advanceMillis(5000); sensor.poll(); }
    TEST_ASSERT_TRUE(sensor.isPresent()); TEST_ASSERT_TRUE(sensor.isWarmupActive());
    I2cMock::setDevicePresent(0x73, true); advanceMillis(5000); sensor.poll();
    require_full_warmup(sensor, previous_data_ms);
}
void test_initial_absence_retries_are_bounded_and_delayed_sensor_detected() {
    Sen0321 sensor; sensor.begin(); TEST_ASSERT_FALSE(sensor.start());
    advanceMillis(9999); sensor.poll(); TEST_ASSERT_EQUAL_UINT32(1, I2cMock::addressOnlyProbeCount(0x73));
    I2cMock::setDevicePresent(0x73, true); advanceMillis(1); sensor.poll(); TEST_ASSERT_TRUE(sensor.isPresent());
    setUp(); sensor.begin(); TEST_ASSERT_FALSE(sensor.start());
    for (int i = 0; i < 10; ++i) { advanceMillis(10000); sensor.poll(); }
    TEST_ASSERT_EQUAL_UINT32(3, I2cMock::addressOnlyProbeCount(0x73));
}
void test_warmup_handles_millis_rollover() {
    setMillis(UINT32_MAX - 1000); Sen0321 sensor; start(sensor); reading(20);
    advanceMillis(Sen0321::WarmupMs); sample(sensor);
    TEST_ASSERT_TRUE(sensor.isDataValid()); TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.020f, sensor.ppm());
}
void test_optional_slot_legacy_priority_and_source_lock() {
    I2cMock::setDevicePresent(0x75, true); I2cMock::setDevicePresent(0x73, true);
    OptionalGasSensor slot; slot.begin(); TEST_ASSERT_TRUE(slot.start());
    TEST_ASSERT_EQUAL_UINT8(0x75, slot.address());
    for (int i = 0; i < 4; ++i) { advanceMillis(1000); slot.poll(); }
    TEST_ASSERT_EQUAL_UINT8(0, I2cMock::getRegister(0x73, 0x03)); // no passive mode write
    I2cMock::setDevicePresent(0x75, false); advanceMillis(600000); slot.poll();
    TEST_ASSERT_EQUAL_UINT8(0x75, slot.address());
    setUp(); I2cMock::setDevicePresent(0x73, true); slot.begin(); TEST_ASSERT_TRUE(slot.start());
    TEST_ASSERT_EQUAL_UINT8(7, static_cast<uint8_t>(slot.optionalGasType()));
    TEST_ASSERT_EQUAL_UINT8(3, slot.ppmDecimals());
    I2cMock::setDevicePresent(0x75, true); slot.poll(); TEST_ASSERT_EQUAL_UINT8(0x73, slot.address());
}
void test_elapsed_sample_is_invalid_until_next_complete_read() {
    Sen0321 sensor; ready(sensor);
    advanceMillis(Sen0321::StaleMs + 1); TEST_ASSERT_FALSE(sensor.isDataValid());
    sensor.poll(); TEST_ASSERT_FALSE(sensor.isDataValid());
    advanceMillis(100); sensor.poll(); TEST_ASSERT_FALSE(sensor.isDataValid());
    advanceMillis(100); sensor.poll(); TEST_ASSERT_TRUE(sensor.isDataValid());
}
int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_elapsed_sample_is_invalid_until_next_complete_read);
    RUN_TEST(test_passive_protocol_timing_warmup_and_big_endian);
    RUN_TEST(test_zero_floor_and_full_range_are_not_corrected);
    RUN_TEST(test_range_and_read_failures_never_publish_false_zero);
    RUN_TEST(test_one_or_two_failures_require_full_warmup_after_return);
    RUN_TEST(test_long_pause_after_failure_does_not_count_as_recovery_warmup);
    RUN_TEST(test_delayed_initial_configuration_starts_its_own_warmup);
    RUN_TEST(test_recovery_after_power_loss_requires_new_warmup);
    RUN_TEST(test_initial_absence_retries_are_bounded_and_delayed_sensor_detected);
    RUN_TEST(test_warmup_handles_millis_rollover);
    RUN_TEST(test_optional_slot_legacy_priority_and_source_lock);
    return UNITY_END();
}
