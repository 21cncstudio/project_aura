#include <unity.h>

#include "ArduinoMock.h"
#include "I2cMock.h"
#include "core/BootState.h"
#include "core/Logger.h"
#include "esp_system.h"

#define private public
#define Sen66 RealSen66
#include "../../src/drivers/Sen66.h"
#undef private

#include "../../src/core/I2CHelper.cpp"
#include "../../src/drivers/Sen66.cpp"
#undef Sen66

namespace {

void encodeWords(const uint16_t *words, size_t word_count, uint8_t *out) {
    for (size_t i = 0; i < word_count; ++i) {
        out[i * 3] = static_cast<uint8_t>(words[i] >> 8);
        out[i * 3 + 1] = static_cast<uint8_t>(words[i] & 0xFF);
        out[i * 3 + 2] = I2C::crc8(&out[i * 3], 2);
    }
}

} // namespace

void setUp() {
    setMillis(0);
    I2cMock::reset();
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(false);
    boot_reset_reason = ESP_RST_POWERON;
    boot_board_cold_start = true;
    boot_peripherals_cold_start = true;
}

void tearDown() {}

void test_real_sen66_device_reset_clears_co2_smoother_ring() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());

    sen66.co2_first_ = false;
    sen66.co2_idx_ = 3;
    sen66.co2_readings_[0] = 1111;
    sen66.co2_readings_[1] = 1222;
    sen66.co2_readings_[2] = 1333;
    sen66.co2_readings_[3] = 1444;
    sen66.co2_readings_[4] = 1555;

    TEST_ASSERT_TRUE(sen66.deviceReset());
    TEST_ASSERT_TRUE(sen66.co2_first_);
    TEST_ASSERT_EQUAL(0, sen66.co2_idx_);
    for (int reading : sen66.co2_readings_) {
        TEST_ASSERT_EQUAL(400, reading);
    }
}

void test_real_sen66_apply_temp_offset_params_includes_base_self_heating() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());

    sen66.temp_offset_ = 1.5f;

    const float expected_hw_correction = 1.5f - Config::BASE_TEMP_OFFSET;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_hw_correction, sen66.desiredTempCorrectionC());
    TEST_ASSERT_TRUE(sen66.applyTempOffsetParams());
    TEST_ASSERT_TRUE(sen66.temp_offset_hw_active_);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_hw_correction, sen66.temp_offset_hw_value_);
}

void test_real_sen66_read_values_applies_remaining_temp_correction_when_hw_offset_is_stale() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);

    const uint16_t read_values_words[9] = {
        0,
        0,
        0,
        0,
        5000,
        4800,
        0x7FFF,
        0x7FFF,
        500
    };
    uint8_t read_values_buf[27] = {};
    encodeWords(read_values_words, 9, read_values_buf);
    I2cMock::setCommandRead(Config::SEN66_ADDR,
                            Config::SEN66_CMD_READ_VALUES,
                            read_values_buf,
                            sizeof(read_values_buf));

    const uint16_t num_conc_words[5] = {
        0xFFFF,
        0,
        0,
        0,
        0
    };
    uint8_t num_conc_buf[15] = {};
    encodeWords(num_conc_words, 5, num_conc_buf);
    I2cMock::setCommandRead(Config::SEN66_ADDR,
                            Config::SEN66_CMD_READ_NUM_CONC,
                            num_conc_buf,
                            sizeof(num_conc_buf));

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.temp_offset_ = 0.0f;
    sen66.temp_offset_hw_active_ = true;
    sen66.temp_offset_hw_value_ = -1.0f;

    SensorData data{};
    TEST_ASSERT_TRUE(sen66.readValues(data));

    TEST_ASSERT_TRUE(data.temp_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.6f, data.temperature);
    TEST_ASSERT_TRUE(data.hum_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, data.humidity);
}

void test_real_sen66_cooperative_late_start_honors_config_and_start_delays() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.beginLateStart(true);

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis()))); // Temp config.
    TEST_ASSERT_EQUAL_UINT32(0, getMillis());
    TEST_ASSERT_TRUE(sen66.isBusy());

    setMillis(Config::SEN66_CMD_DELAY_MS - 1U);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    setMillis(Config::SEN66_CMD_DELAY_MS);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis()))); // Advance to ASC.
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis()))); // Default ASC.
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis()))); // Start command.

    setMillis(Config::SEN66_CMD_DELAY_MS + Config::SEN66_START_DELAY_MS - 1U);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    setMillis(Config::SEN66_CMD_DELAY_MS + Config::SEN66_START_DELAY_MS);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sen66.isOk());
    TEST_ASSERT_FALSE(sen66.isBusy());
}

void test_real_sen66_cooperative_warm_resync_waits_after_third_stop_failure() {
    boot_reset_reason = ESP_RST_SW;
    boot_board_cold_start = false;
    boot_peripherals_cold_start = false;
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_STOP,
                               true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.beginLateStart(true);

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    setMillis(Config::SEN66_CMD_DELAY_MS);
    sen66.pollLateStart(getMillis());
    sen66.pollLateStart(getMillis());
    setMillis(Config::SEN66_CMD_DELAY_MS * 2U);
    sen66.pollLateStart(getMillis());
    sen66.pollLateStart(getMillis());

    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::DeviceResetRetryWait),
        static_cast<int>(sen66.late_start_phase_));
    setMillis(Config::SEN66_CMD_DELAY_MS * 3U - 1U);
    sen66.pollLateStart(getMillis());
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::DeviceResetRetryWait),
        static_cast<int>(sen66.late_start_phase_));
    setMillis(Config::SEN66_CMD_DELAY_MS * 3U);
    sen66.pollLateStart(getMillis());
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::DeviceResetWrite),
        static_cast<int>(sen66.late_start_phase_));
}

void test_real_sen66_failed_known_stop_keeps_state_unknown_for_next_retry() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_STOP,
                               true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.measuring_ = true;
    sen66.beginLateStart(true);

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    setMillis(Config::SEN66_CMD_DELAY_MS);
    sen66.pollLateStart(getMillis());
    sen66.pollLateStart(getMillis());
    setMillis(Config::SEN66_CMD_DELAY_MS * 2U);
    sen66.pollLateStart(getMillis());
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Failed),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sen66.measurement_state_unknown_);

    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_STOP,
                               false);
    sen66.beginLateStart(true);
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::StopWrite),
        static_cast<int>(sen66.late_start_phase_));
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::InProgress),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::StopWait),
        static_cast<int>(sen66.late_start_phase_));
}

void test_real_sen66_failed_cooperative_start_forces_stop_on_next_retry() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_START,
                               true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.beginLateStart(true);
    sen66.pollLateStart(getMillis()); // Temp config.
    setMillis(Config::SEN66_CMD_DELAY_MS);
    sen66.pollLateStart(getMillis()); // Advance to ASC.
    sen66.pollLateStart(getMillis()); // Default ASC, advance to START.

    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Failed),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sen66.measurement_state_unknown_);
    TEST_ASSERT_FALSE(sen66.isMeasuring());

    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_START,
                               false);
    sen66.beginLateStart(true);
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::StopWrite),
        static_cast<int>(sen66.late_start_phase_));
}

void test_real_sen66_failed_synchronous_start_forces_stop_on_next_retry() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_START,
                               true);

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());

    TEST_ASSERT_FALSE(sen66.startMeasurement());
    TEST_ASSERT_TRUE(sen66.measurement_state_unknown_);
    TEST_ASSERT_FALSE(sen66.isMeasuring());

    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_START,
                               false);
    sen66.beginLateStart(true);
    TEST_ASSERT_EQUAL(
        static_cast<int>(RealSen66::LateStartPhase::StopWrite),
        static_cast<int>(sen66.late_start_phase_));
}

void test_real_sen66_cooperative_final_asc_failure_waits_and_reads_status() {
    I2cMock::setDevicePresent(Config::SEN66_ADDR, true);
    I2cMock::setCommandFailure(Config::SEN66_ADDR,
                               Config::SEN66_CMD_ASC,
                               true);
    const uint16_t status_words[2] = {0, 0};
    uint8_t status_buf[6] = {};
    encodeWords(status_words, 2, status_buf);
    I2cMock::setCommandRead(Config::SEN66_ADDR,
                            Config::SEN66_CMD_READ_STATUS,
                            status_buf,
                            sizeof(status_buf));

    RealSen66 sen66;
    TEST_ASSERT_TRUE(sen66.begin());
    sen66.beginLateStart(false);
    sen66.pollLateStart(getMillis()); // Temp config.
    setMillis(Config::SEN66_CMD_DELAY_MS);
    sen66.pollLateStart(getMillis()); // Advance to ASC read.
    sen66.pollLateStart(getMillis()); // ASC read command fails.
    sen66.pollLateStart(getMillis()); // Apply write 1 fails.

    setMillis(Config::SEN66_CMD_DELAY_MS + Config::SEN66_ASC_RETRY_DELAY_MS);
    sen66.pollLateStart(getMillis()); // Retry wait expires.
    sen66.pollLateStart(getMillis()); // Apply write 2 fails.
    TEST_ASSERT_EQUAL(static_cast<int>(RealSen66::LateStartPhase::AscFinalDelay),
                      static_cast<int>(sen66.late_start_phase_));

    const uint32_t final_delay_due =
        Config::SEN66_CMD_DELAY_MS + 2U * Config::SEN66_ASC_RETRY_DELAY_MS;
    setMillis(final_delay_due - 1U);
    sen66.pollLateStart(getMillis());
    TEST_ASSERT_EQUAL(static_cast<int>(RealSen66::LateStartPhase::AscFinalDelay),
                      static_cast<int>(sen66.late_start_phase_));
    setMillis(final_delay_due);
    sen66.pollLateStart(getMillis());
    TEST_ASSERT_EQUAL(static_cast<int>(RealSen66::LateStartPhase::AscFinalStatusWrite),
                      static_cast<int>(sen66.late_start_phase_));
    sen66.pollLateStart(getMillis()); // Status command.
    setMillis(final_delay_due + Config::SEN66_CMD_DELAY_MS);
    sen66.pollLateStart(getMillis()); // Arm status response.
    sen66.pollLateStart(getMillis()); // Read status.
    sen66.pollLateStart(getMillis()); // Start command.
    setMillis(final_delay_due + Config::SEN66_CMD_DELAY_MS +
              Config::SEN66_START_DELAY_MS);
    TEST_ASSERT_EQUAL(static_cast<int>(CooperativeStart::Result::Success),
                      static_cast<int>(sen66.pollLateStart(getMillis())));
    TEST_ASSERT_TRUE(sen66.isOk());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_real_sen66_device_reset_clears_co2_smoother_ring);
    RUN_TEST(test_real_sen66_apply_temp_offset_params_includes_base_self_heating);
    RUN_TEST(test_real_sen66_read_values_applies_remaining_temp_correction_when_hw_offset_is_stale);
    RUN_TEST(test_real_sen66_cooperative_late_start_honors_config_and_start_delays);
    RUN_TEST(test_real_sen66_cooperative_warm_resync_waits_after_third_stop_failure);
    RUN_TEST(test_real_sen66_failed_known_stop_keeps_state_unknown_for_next_retry);
    RUN_TEST(test_real_sen66_failed_cooperative_start_forces_stop_on_next_retry);
    RUN_TEST(test_real_sen66_failed_synchronous_start_forces_stop_on_next_retry);
    RUN_TEST(test_real_sen66_cooperative_final_asc_failure_waits_and_reads_status);
    return UNITY_END();
}

