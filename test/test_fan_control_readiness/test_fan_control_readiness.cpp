#include <unity.h>

#include "ArduinoMock.h"
#include "config/AppConfig.h"
#include "config/AppData.h"
#include "drivers/Gp8403.h"
#include "modules/DisplayThresholds.h"
#include "modules/FanControl.h"

void setUp() {
    setMillis(0);
    Gp8403::state() = Gp8403TestState{};
}

void tearDown() {}

void test_poll_before_begin_never_probes_or_writes_dac() {
    FanControl control;
    SensorData data{};

    control.poll(60000, &data, false, DisplayThresholds::defaults());

    TEST_ASSERT_EQUAL_UINT32(0, Gp8403::state().begin_calls);
    TEST_ASSERT_EQUAL_UINT32(0, Gp8403::state().probe_calls);
    TEST_ASSERT_EQUAL_UINT32(0, Gp8403::state().range_calls);
    TEST_ASSERT_EQUAL_UINT32(0, Gp8403::state().write_calls);
    TEST_ASSERT_FALSE(control.snapshot().available);
}

void test_normal_begin_keeps_dac_initialization_behavior() {
    FanControl control;

    control.begin(false, false);

    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().begin_calls);
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().range_calls);
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().write_calls);
    TEST_ASSERT_TRUE(control.snapshot().available);
}

void test_missing_dac_uses_bounded_startup_schedule_and_stops() {
    Gp8403::state().begin_ok = false;
    FanControl control;

    control.begin(false, false);
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().begin_calls);

    control.poll(999, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().begin_calls);
    control.poll(1000, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(2, Gp8403::state().begin_calls);
    control.poll(2999, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(2, Gp8403::state().begin_calls);
    control.poll(3000, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(3, Gp8403::state().begin_calls);
    control.poll(10000, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(4, Gp8403::state().begin_calls);
    control.poll(30000, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(5, Gp8403::state().begin_calls);

    Gp8403::state().begin_ok = true;
    control.poll(60000, nullptr, false, DisplayThresholds::defaults());
    control.poll(600000, nullptr, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT32(5, Gp8403::state().begin_calls);
    TEST_ASSERT_FALSE(control.snapshot().available);
}

void test_missing_dac_can_be_detected_by_later_startup_attempt() {
    Gp8403::state().begin_ok = false;
    FanControl control;
    control.begin(false, false);

    control.poll(1000, nullptr, false, DisplayThresholds::defaults());
    Gp8403::state().begin_ok = true;
    control.poll(3000, nullptr, false, DisplayThresholds::defaults());

    TEST_ASSERT_EQUAL_UINT32(3, Gp8403::state().begin_calls);
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().range_calls);
    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().write_calls);
    TEST_ASSERT_TRUE(control.snapshot().available);
}

void test_prepare_for_restart_writes_safe_output_once() {
    FanControl control;
    control.begin(false, false);
    const uint32_t writes_after_begin = Gp8403::state().write_calls;

    TEST_ASSERT_TRUE(control.prepareForRestart());

    TEST_ASSERT_EQUAL_UINT32(writes_after_begin + 1U, Gp8403::state().write_calls);
    TEST_ASSERT_EQUAL_UINT8(Config::DAC_CHANNEL_VOUT0, Gp8403::state().last_write_channel);
    TEST_ASSERT_EQUAL_UINT16(Config::DAC_SAFE_ERROR_MV, Gp8403::state().last_write_mv);
    TEST_ASSERT_TRUE(control.snapshot().output_known);
    TEST_ASSERT_FALSE(control.snapshot().running);
}

void test_prepare_for_restart_failure_is_not_retried() {
    FanControl control;
    control.begin(false, false);
    const uint32_t writes_after_begin = Gp8403::state().write_calls;
    Gp8403::state().write_ok = false;

    TEST_ASSERT_FALSE(control.prepareForRestart());

    TEST_ASSERT_EQUAL_UINT32(writes_after_begin + 1U, Gp8403::state().write_calls);
    TEST_ASSERT_TRUE(control.snapshot().faulted);
    TEST_ASSERT_FALSE(control.snapshot().available);
    TEST_ASSERT_FALSE(control.snapshot().output_known);
}

void test_prepare_for_restart_attempts_safe_write_after_failed_detection() {
    Gp8403::state().begin_ok = false;
    FanControl control;
    control.begin(false, false);
    TEST_ASSERT_EQUAL_UINT32(0, Gp8403::state().write_calls);

    TEST_ASSERT_TRUE(control.prepareForRestart());

    TEST_ASSERT_EQUAL_UINT32(1, Gp8403::state().write_calls);
    TEST_ASSERT_EQUAL_UINT16(Config::DAC_SAFE_ERROR_MV,
                             Gp8403::state().last_write_mv);
    TEST_ASSERT_TRUE(control.snapshot().output_known);
}

void test_prepare_for_i2c_offline_makes_one_bounded_safe_write_attempt() {
    FanControl control;
    control.begin(false, false);
    const uint32_t writes_after_begin = Gp8403::state().write_calls;
    Gp8403::state().write_ok = false;

    TEST_ASSERT_FALSE(control.prepareForI2cOffline());

    TEST_ASSERT_EQUAL_UINT32(writes_after_begin + 1U, Gp8403::state().write_calls);
    TEST_ASSERT_TRUE(control.snapshot().faulted);
    TEST_ASSERT_FALSE(control.snapshot().output_known);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_poll_before_begin_never_probes_or_writes_dac);
    RUN_TEST(test_normal_begin_keeps_dac_initialization_behavior);
    RUN_TEST(test_missing_dac_uses_bounded_startup_schedule_and_stops);
    RUN_TEST(test_missing_dac_can_be_detected_by_later_startup_attempt);
    RUN_TEST(test_prepare_for_restart_writes_safe_output_once);
    RUN_TEST(test_prepare_for_restart_failure_is_not_retried);
    RUN_TEST(test_prepare_for_restart_attempts_safe_write_after_failed_detection);
    RUN_TEST(test_prepare_for_i2c_offline_makes_one_bounded_safe_write_attempt);
    return UNITY_END();
}
