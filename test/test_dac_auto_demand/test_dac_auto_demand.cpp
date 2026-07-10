#include <unity.h>

#include "config/AppData.h"
#include "modules/DacAutoDemand.h"

void setUp() {}
void tearDown() {}

namespace {

void disable_all_sensors(DacAutoConfig &cfg) {
    cfg.co2.enabled = false;
    cfg.co.enabled = false;
    cfg.pm05.enabled = false;
    cfg.pm1.enabled = false;
    cfg.pm4.enabled = false;
    cfg.pm25.enabled = false;
    cfg.pm10.enabled = false;
    cfg.hcho.enabled = false;
    cfg.voc.enabled = false;
    cfg.nox.enabled = false;
}

}  // namespace

void test_dac_auto_demand_uses_display_thresholds_for_voc_and_nox() {
    DacAutoConfig cfg{};
    cfg.enabled = true;
    disable_all_sensors(cfg);
    cfg.voc.enabled = true;
    cfg.voc.band = {11, 22, 33, 44};
    cfg.nox.enabled = true;
    cfg.nox.band = {15, 25, 35, 45};

    SensorData data{};
    data.voc_valid = true;
    data.voc_index = 120;
    data.nox_valid = true;
    data.nox_index = 95;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    thresholds.voc = {100.0f, 150.0f, 200.0f};
    thresholds.nox = {80.0f, 100.0f, 150.0f};

    DacAutoDemand::Result result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(25, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::NOX),
                          static_cast<int>(result.sensor));

    thresholds.voc = {120.0f, 150.0f, 200.0f};
    thresholds.nox = {95.0f, 100.0f, 150.0f};
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(15, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::NOX),
                          static_cast<int>(result.sensor));

    thresholds.voc = {80.0f, 100.0f, 110.0f};
    thresholds.nox = {50.0f, 80.0f, 90.0f};
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(45, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::NOX),
                          static_cast<int>(result.sensor));
}

void test_dac_auto_demand_uses_display_thresholds_for_co2_hcho_and_co() {
    DacAutoConfig cfg{};
    cfg.enabled = true;
    disable_all_sensors(cfg);
    cfg.co2.enabled = true;
    cfg.co2.band = {10, 20, 30, 40};
    cfg.hcho.enabled = true;
    cfg.hcho.band = {11, 21, 31, 41};
    cfg.co.enabled = true;
    cfg.co.band = {12, 22, 32, 42};

    SensorData data{};
    data.co2_valid = true;
    data.co2 = 750;
    data.hcho_valid = true;
    data.hcho = 55.0f;
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_ppm = 4.0f;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    thresholds.co2 = {700.0f, 900.0f, 1200.0f};
    thresholds.hcho = {40.0f, 60.0f, 100.0f};
    thresholds.co = {5.0f, 10.0f, 20.0f};

    DacAutoDemand::Result result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(21, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::HCHO),
                          static_cast<int>(result.sensor));

    thresholds.co2 = {800.0f, 900.0f, 1200.0f};
    thresholds.hcho = {55.0f, 60.0f, 100.0f};
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(12, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::CO),
                          static_cast<int>(result.sensor));
}

void test_dac_auto_demand_suppresses_reactive_gases_during_warmup() {
    DacAutoConfig cfg{};
    cfg.enabled = true;
    disable_all_sensors(cfg);
    cfg.voc.enabled = true;
    cfg.voc.band = {10, 20, 30, 100};
    cfg.nox.enabled = true;
    cfg.nox.band = {10, 20, 30, 100};

    SensorData data{};
    data.voc_valid = true;
    data.voc_index = 400;
    data.nox_valid = true;
    data.nox_index = 250;

    const DacAutoDemand::Result result =
        DacAutoDemand::evaluate(cfg, data, true, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::None),
                          static_cast<int>(result.sensor));
}

void test_dac_auto_demand_keeps_pm_on_fixed_zones() {
    DacAutoConfig cfg{};
    cfg.enabled = true;
    disable_all_sensors(cfg);
    cfg.pm25.enabled = true;
    cfg.pm25.band = {10, 20, 30, 40};

    SensorData data{};
    data.pm25_valid = true;
    data.pm25 = 12.0f;

    DacAutoDemand::Result result =
        DacAutoDemand::evaluate(cfg, data, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT8(10, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::PM25),
                          static_cast<int>(result.sensor));

    data.pm25 = 56.0f;
    result = DacAutoDemand::evaluate(cfg, data, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT8(40, result.percent);
}

void test_dac_auto_demand_formats_reason_values() {
    char label[16];
    char value[32];
    snprintf(label, sizeof(label), "%s", DacAutoDemand::sensorLabel(DacAutoDemand::Sensor::VOC));
    DacAutoDemand::formatSensorValue(DacAutoDemand::Sensor::VOC, 123.0f, value, sizeof(value));

    TEST_ASSERT_EQUAL_STRING("VOC:", label);
    TEST_ASSERT_EQUAL_STRING("123 idx", value);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_dac_auto_demand_uses_display_thresholds_for_voc_and_nox);
    RUN_TEST(test_dac_auto_demand_uses_display_thresholds_for_co2_hcho_and_co);
    RUN_TEST(test_dac_auto_demand_suppresses_reactive_gases_during_warmup);
    RUN_TEST(test_dac_auto_demand_keeps_pm_on_fixed_zones);
    RUN_TEST(test_dac_auto_demand_formats_reason_values);
    return UNITY_END();
}
