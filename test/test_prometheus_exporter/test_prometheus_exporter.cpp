#include <unity.h>
#include <string.h>

#include "config/AppData.h"
#include "web/PrometheusExporter.h"

namespace {

void assert_contains(const String &text, const char *needle) {
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text.c_str(), needle), needle);
}

void assert_not_contains(const String &text, const char *needle) {
    TEST_ASSERT_NULL_MESSAGE(strstr(text.c_str(), needle), needle);
}

} // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// Sensor metrics
// ---------------------------------------------------------------------------

void test_sensor_metrics_valid_temperature() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 22.5f;

    String out;
    prom_append_sensor_metrics(out, data);

    assert_contains(out, "# HELP aura_temperature_celsius");
    assert_contains(out, "# TYPE aura_temperature_celsius gauge");
    assert_contains(out, "aura_temperature_celsius 22.5\n");
}

void test_sensor_metrics_invalid_temperature_omitted() {
    SensorData data{};
    data.temp_valid = false;
    data.temperature = 22.5f;

    String out;
    prom_append_sensor_metrics(out, data);

    assert_not_contains(out, "aura_temperature_celsius");
}

void test_sensor_metrics_all_valid() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 21.3f;
    data.hum_valid = true;
    data.humidity = 55.2f;
    data.pressure_valid = true;
    data.pressure = 1013.2f;
    data.pm05_valid = true;
    data.pm05 = 120.5f;
    data.pm1_valid = true;
    data.pm1 = 8.3f;
    data.pm25_valid = true;
    data.pm25 = 12.7f;
    data.pm4_valid = true;
    data.pm4 = 15.1f;
    data.pm10_valid = true;
    data.pm10 = 18.9f;
    data.co2_valid = true;
    data.co2 = 812;
    data.voc_valid = true;
    data.voc_index = 150;
    data.nox_valid = true;
    data.nox_index = 42;
    data.hcho_valid = true;
    data.hcho = 3.5f;
    data.co_valid = true;
    data.co_sensor_present = true;
    data.co_ppm = 1.2f;

    String out;
    prom_append_sensor_metrics(out, data);

    assert_contains(out, "aura_temperature_celsius 21.3\n");
    assert_contains(out, "aura_humidity_percent 55.2\n");
    assert_contains(out, "aura_pressure_hpa 1013.2\n");
    assert_contains(out, "aura_pm05_count_per_cm3 120.5\n");
    assert_contains(out, "aura_pm1_ugm3 8.3\n");
    assert_contains(out, "aura_pm25_ugm3 12.7\n");
    assert_contains(out, "aura_pm4_ugm3 15.1\n");
    assert_contains(out, "aura_pm10_ugm3 18.9\n");
    assert_contains(out, "aura_co2_ppm 812\n");
    assert_contains(out, "aura_voc_index 150\n");
    assert_contains(out, "aura_nox_index 42\n");
    assert_contains(out, "aura_hcho_ppb 3.5\n");
    assert_contains(out, "aura_co_ppm 1.2\n");
    assert_contains(out, "aura_co_sensor_present 1\n");
    assert_contains(out, "aura_co_sensor_warmup 0\n");
}

void test_sensor_metrics_all_invalid_only_booleans() {
    SensorData data{};
    // All validity flags default to false.

    String out;
    prom_append_sensor_metrics(out, data);

    // Only the boolean metrics should be present.
    assert_not_contains(out, "aura_temperature_celsius");
    assert_not_contains(out, "aura_humidity_percent");
    assert_not_contains(out, "aura_co2_ppm");
    assert_not_contains(out, "aura_co_ppm");
    assert_contains(out, "aura_co_sensor_present 0\n");
    assert_contains(out, "aura_co_sensor_warmup 0\n");
}

void test_sensor_metrics_co_excluded_without_sensor() {
    SensorData data{};
    data.co_valid = true;
    data.co_ppm = 5.0f;
    data.co_sensor_present = false; // Sensor not installed.

    String out;
    prom_append_sensor_metrics(out, data);

    // co_ppm should be excluded because co_sensor_present is false.
    assert_not_contains(out, "aura_co_ppm 5.0");
    assert_contains(out, "aura_co_sensor_present 0\n");
}

// ---------------------------------------------------------------------------
// Derived metrics
// ---------------------------------------------------------------------------

void test_derived_metrics_with_valid_climate() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 22.0f;
    data.hum_valid = true;
    data.humidity = 50.0f;

    String out;
    prom_append_derived_metrics(out, data);

    assert_contains(out, "aura_dew_point_celsius");
    assert_contains(out, "aura_absolute_humidity_gm3");
    assert_contains(out, "aura_mold_risk_index");
}

void test_derived_metrics_omitted_without_climate() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 22.0f;
    data.hum_valid = false; // Humidity invalid => derived omitted.

    String out;
    prom_append_derived_metrics(out, data);

    TEST_ASSERT_TRUE(out.empty());
}

// ---------------------------------------------------------------------------
// Pressure trend metrics
// ---------------------------------------------------------------------------

void test_pressure_trend_valid() {
    SensorData data{};
    data.pressure_delta_3h_valid = true;
    data.pressure_delta_3h = -1.5f;
    data.pressure_delta_24h_valid = true;
    data.pressure_delta_24h = 3.2f;

    String out;
    prom_append_pressure_trend_metrics(out, data);

    assert_contains(out, "aura_pressure_delta_3h_hpa -1.5\n");
    assert_contains(out, "aura_pressure_delta_24h_hpa 3.2\n");
}

void test_pressure_trend_invalid_omitted() {
    SensorData data{};
    data.pressure_delta_3h_valid = false;
    data.pressure_delta_24h_valid = false;

    String out;
    prom_append_pressure_trend_metrics(out, data);

    TEST_ASSERT_TRUE(out.empty());
}

// ---------------------------------------------------------------------------
// Prometheus format correctness
// ---------------------------------------------------------------------------

void test_each_metric_has_help_and_type() {
    SensorData data{};
    data.co2_valid = true;
    data.co2 = 400;

    String out;
    prom_append_sensor_metrics(out, data);

    // Verify the CO2 metric has proper HELP and TYPE lines before the value.
    const char *help = strstr(out.c_str(), "# HELP aura_co2_ppm");
    const char *type = strstr(out.c_str(), "# TYPE aura_co2_ppm gauge");
    const char *value = strstr(out.c_str(), "aura_co2_ppm 400\n");

    TEST_ASSERT_NOT_NULL(help);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_NULL(value);
    // HELP before TYPE before value.
    TEST_ASSERT_TRUE(help < type);
    TEST_ASSERT_TRUE(type < value);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_metrics_valid_temperature);
    RUN_TEST(test_sensor_metrics_invalid_temperature_omitted);
    RUN_TEST(test_sensor_metrics_all_valid);
    RUN_TEST(test_sensor_metrics_all_invalid_only_booleans);
    RUN_TEST(test_sensor_metrics_co_excluded_without_sensor);
    RUN_TEST(test_derived_metrics_with_valid_climate);
    RUN_TEST(test_derived_metrics_omitted_without_climate);
    RUN_TEST(test_pressure_trend_valid);
    RUN_TEST(test_pressure_trend_invalid_omitted);
    RUN_TEST(test_each_metric_has_help_and_type);
    return UNITY_END();
}
