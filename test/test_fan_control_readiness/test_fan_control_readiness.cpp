#include <unity.h>

#include "ArduinoMock.h"
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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_poll_before_begin_never_probes_or_writes_dac);
    RUN_TEST(test_normal_begin_keeps_dac_initialization_behavior);
    return UNITY_END();
}
