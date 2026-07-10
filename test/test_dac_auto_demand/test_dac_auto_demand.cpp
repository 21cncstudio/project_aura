#include <unity.h>

#include <math.h>

#include "config/AppConfig.h"
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

DacAutoSensorConfig &sensor_config(DacAutoConfig &cfg, DacAutoDemand::Sensor sensor) {
    switch (sensor) {
        case DacAutoDemand::Sensor::CO2:
            return cfg.co2;
        case DacAutoDemand::Sensor::CO:
            return cfg.co;
        case DacAutoDemand::Sensor::PM05:
            return cfg.pm05;
        case DacAutoDemand::Sensor::PM1:
            return cfg.pm1;
        case DacAutoDemand::Sensor::PM4:
            return cfg.pm4;
        case DacAutoDemand::Sensor::PM25:
            return cfg.pm25;
        case DacAutoDemand::Sensor::PM10:
            return cfg.pm10;
        case DacAutoDemand::Sensor::HCHO:
            return cfg.hcho;
        case DacAutoDemand::Sensor::VOC:
            return cfg.voc;
        case DacAutoDemand::Sensor::NOX:
            return cfg.nox;
        case DacAutoDemand::Sensor::None:
        default:
            return cfg.co2;
    }
}

void enable_only(DacAutoConfig &cfg, DacAutoDemand::Sensor sensor) {
    cfg.enabled = true;
    disable_all_sensors(cfg);
    DacAutoSensorConfig &selected = sensor_config(cfg, sensor);
    selected.enabled = true;
    selected.band = {10, 20, 30, 40};
}

void set_sensor_value(SensorData &data, DacAutoDemand::Sensor sensor, float value) {
    switch (sensor) {
        case DacAutoDemand::Sensor::CO2:
            data.co2_valid = true;
            data.co2 = static_cast<int>(value);
            break;
        case DacAutoDemand::Sensor::CO:
            data.co_sensor_present = true;
            data.co_valid = true;
            data.co_ppm = value;
            break;
        case DacAutoDemand::Sensor::PM05:
            data.pm05_valid = true;
            data.pm05 = value;
            break;
        case DacAutoDemand::Sensor::PM1:
            data.pm1_valid = true;
            data.pm1 = value;
            break;
        case DacAutoDemand::Sensor::PM4:
            data.pm4_valid = true;
            data.pm4 = value;
            break;
        case DacAutoDemand::Sensor::PM25:
            data.pm25_valid = true;
            data.pm25 = value;
            break;
        case DacAutoDemand::Sensor::PM10:
            data.pm10_valid = true;
            data.pm10 = value;
            break;
        case DacAutoDemand::Sensor::HCHO:
            data.hcho_valid = true;
            data.hcho = value;
            break;
        case DacAutoDemand::Sensor::VOC:
            data.voc_valid = true;
            data.voc_index = static_cast<int>(value);
            break;
        case DacAutoDemand::Sensor::NOX:
            data.nox_valid = true;
            data.nox_index = static_cast<int>(value);
            break;
        case DacAutoDemand::Sensor::None:
        default:
            break;
    }
}

void set_shared_thresholds(DisplayThresholds::Config &thresholds,
                           DacAutoDemand::Sensor sensor,
                           const DisplayThresholds::High &value) {
    switch (sensor) {
        case DacAutoDemand::Sensor::CO2:
            thresholds.co2 = value;
            break;
        case DacAutoDemand::Sensor::CO:
            thresholds.co = value;
            break;
        case DacAutoDemand::Sensor::HCHO:
            thresholds.hcho = value;
            break;
        case DacAutoDemand::Sensor::VOC:
            thresholds.voc = value;
            break;
        case DacAutoDemand::Sensor::NOX:
            thresholds.nox = value;
            break;
        default:
            break;
    }
}

void assert_single_sensor_demand(DacAutoDemand::Sensor sensor,
                                 float value,
                                 const DisplayThresholds::Config &thresholds,
                                 uint8_t expected_percent) {
    DacAutoConfig cfg{};
    enable_only(cfg, sensor);
    SensorData data{};
    set_sensor_value(data, sensor, value);

    const DacAutoDemand::Result result =
        DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected_percent,
                                    result.percent,
                                    DacAutoDemand::sensorLabel(sensor));
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(sensor),
                                  static_cast<int>(result.sensor),
                                  DacAutoDemand::sensorLabel(sensor));
}

}  // namespace

void test_dac_auto_demand_uses_inclusive_display_thresholds_for_each_shared_sensor() {
    const DacAutoDemand::Sensor sensors[] = {
        DacAutoDemand::Sensor::CO2,
        DacAutoDemand::Sensor::CO,
        DacAutoDemand::Sensor::HCHO,
        DacAutoDemand::Sensor::VOC,
        DacAutoDemand::Sensor::NOX,
    };

    for (DacAutoDemand::Sensor sensor : sensors) {
        DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
        set_shared_thresholds(thresholds, sensor, {100.0f, 200.0f, 300.0f});

        assert_single_sensor_demand(sensor, 100.0f, thresholds, 10);
        assert_single_sensor_demand(sensor, 101.0f, thresholds, 20);
        assert_single_sensor_demand(sensor, 200.0f, thresholds, 20);
        assert_single_sensor_demand(sensor, 201.0f, thresholds, 30);
        assert_single_sensor_demand(sensor, 300.0f, thresholds, 30);
        assert_single_sensor_demand(sensor, 301.0f, thresholds, 40);
    }
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

void test_dac_auto_demand_keeps_all_pm_sensors_on_inclusive_fixed_zones() {
    struct PmCase {
        DacAutoDemand::Sensor sensor;
        float green;
        float yellow;
        float orange;
    };
    const PmCase cases[] = {
        {DacAutoDemand::Sensor::PM05,
         Config::AQ_PM05_GREEN_MAX_PPCM3,
         Config::AQ_PM05_YELLOW_MAX_PPCM3,
         Config::AQ_PM05_ORANGE_MAX_PPCM3},
        {DacAutoDemand::Sensor::PM1,
         Config::AQ_PM1_GREEN_MAX_UGM3,
         Config::AQ_PM1_YELLOW_MAX_UGM3,
         Config::AQ_PM1_ORANGE_MAX_UGM3},
        {DacAutoDemand::Sensor::PM4,
         Config::AQ_PM4_GREEN_MAX_UGM3,
         Config::AQ_PM4_YELLOW_MAX_UGM3,
         Config::AQ_PM4_ORANGE_MAX_UGM3},
        {DacAutoDemand::Sensor::PM25,
         Config::AQ_PM25_GREEN_MAX_UGM3,
         Config::AQ_PM25_YELLOW_MAX_UGM3,
         Config::AQ_PM25_ORANGE_MAX_UGM3},
        {DacAutoDemand::Sensor::PM10,
         Config::AQ_PM10_GREEN_MAX_UGM3,
         Config::AQ_PM10_YELLOW_MAX_UGM3,
         Config::AQ_PM10_ORANGE_MAX_UGM3},
    };

    const DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    for (const PmCase &pm : cases) {
        assert_single_sensor_demand(pm.sensor, pm.green, thresholds, 10);
        assert_single_sensor_demand(pm.sensor, pm.yellow, thresholds, 20);
        assert_single_sensor_demand(pm.sensor, pm.orange, thresholds, 30);
        assert_single_sensor_demand(pm.sensor, pm.orange + 0.1f, thresholds, 40);
    }
}

void test_dac_auto_demand_tie_break_keeps_first_sensor() {
    DacAutoConfig cfg{};
    cfg.enabled = true;
    disable_all_sensors(cfg);
    cfg.co2.enabled = true;
    cfg.co2.band = {50, 50, 50, 50};
    cfg.co.enabled = true;
    cfg.co.band = {50, 50, 50, 50};

    SensorData data{};
    set_sensor_value(data, DacAutoDemand::Sensor::CO2, 500.0f);
    set_sensor_value(data, DacAutoDemand::Sensor::CO, 1.0f);

    const DacAutoDemand::Result result =
        DacAutoDemand::evaluate(cfg, data, false, DisplayThresholds::defaults());
    TEST_ASSERT_EQUAL_UINT8(50, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::CO2),
                          static_cast<int>(result.sensor));
}

void test_dac_auto_demand_ignores_disabled_and_invalid_samples() {
    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    DacAutoConfig cfg{};
    SensorData data{};

    enable_only(cfg, DacAutoDemand::Sensor::CO2);
    cfg.enabled = false;
    set_sensor_value(data, DacAutoDemand::Sensor::CO2, 2000.0f);
    DacAutoDemand::Result result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::None),
                          static_cast<int>(result.sensor));

    cfg.enabled = true;
    cfg.co2.enabled = false;
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);

    enable_only(cfg, DacAutoDemand::Sensor::CO);
    data = SensorData{};
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_ppm = NAN;
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);

    data.co_ppm = -0.1f;
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);

    enable_only(cfg, DacAutoDemand::Sensor::VOC);
    data = SensorData{};
    data.voc_valid = true;
    data.voc_index = -1;
    result = DacAutoDemand::evaluate(cfg, data, false, thresholds);
    TEST_ASSERT_EQUAL_UINT8(0, result.percent);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DacAutoDemand::Sensor::None),
                          static_cast<int>(result.sensor));
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
    RUN_TEST(test_dac_auto_demand_uses_inclusive_display_thresholds_for_each_shared_sensor);
    RUN_TEST(test_dac_auto_demand_suppresses_reactive_gases_during_warmup);
    RUN_TEST(test_dac_auto_demand_keeps_all_pm_sensors_on_inclusive_fixed_zones);
    RUN_TEST(test_dac_auto_demand_tie_break_keeps_first_sensor);
    RUN_TEST(test_dac_auto_demand_ignores_disabled_and_invalid_samples);
    RUN_TEST(test_dac_auto_demand_formats_reason_values);
    return UNITY_END();
}
