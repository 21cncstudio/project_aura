#include <unity.h>
#include <math.h>
#include <string.h>

#include "config/AppConfig.h"
#include "config/AppData.h"
#include "core/BootState.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "modules/FanStateSnapshot.h"
#include "modules/MqttPayloadBuilder.h"
#include "ArduinoMock.h"

namespace {

void assert_contains(const String &text, const char *needle) {
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text.c_str(), needle), needle);
}

} // namespace

void setUp() {}
void tearDown() {}

void test_state_payload_includes_pm05_pm1_pm4_and_co_null_without_sensor() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 22.4f;
    data.hum_valid = true;
    data.humidity = 46.2f;
    data.co2_valid = true;
    data.co2 = 812;
    data.co_sensor_present = false;
    data.co_valid = true;
    data.co_ppm = 7.3f;
    data.pm05_valid = true;
    data.pm05 = 321.4f;
    data.pm1_valid = true;
    data.pm1 = 8.7f;
    data.pm4_valid = true;
    data.pm4 = 12.3f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, true, false, true);

    assert_contains(payload, "\"pm05\":321.4");
    assert_contains(payload, "\"pm1\":8.7");
    assert_contains(payload, "\"pm4\":12.3");
    assert_contains(payload, "\"co\":null");
    assert_contains(payload, "\"night_mode\":\"ON\"");
    assert_contains(payload, "\"alert_blink\":\"OFF\"");
    assert_contains(payload, "\"backlight\":\"ON\"");
}

void test_state_payload_includes_co_when_sensor_present_and_valid() {
    SensorData data{};
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_ppm = 1.5f;
    data.pm05_valid = false;
    data.pm05 = 777.0f;
    data.pm1_valid = false;
    data.pm1 = 0.0f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, true, false);

    assert_contains(payload, "\"co\":1.5");
    assert_contains(payload, "\"pm05\":null");
    assert_contains(payload, "\"pm1\":null");
    assert_contains(payload, "\"alert_blink\":\"ON\"");
    assert_contains(payload, "\"backlight\":\"OFF\"");
}

void test_state_payload_buffer_builder_matches_string_payload() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 21.6f;
    data.hum_valid = true;
    data.humidity = 43.5f;
    data.co2_valid = true;
    data.co2 = 745;
    data.pm25_valid = true;
    data.pm25 = 11.2f;
    data.pressure_valid = true;
    data.pressure = 1009.4f;

    char payload[Config::MQTT_BUFFER_SIZE] = {};
    size_t written = MqttPayloadBuilder::buildStatePayload(
        payload, sizeof(payload), data, false, true, true, false);
    TEST_ASSERT_GREATER_THAN_UINT32(0, static_cast<uint32_t>(written));

    String string_payload = MqttPayloadBuilder::buildStatePayload(data, false, true, true, false);
    TEST_ASSERT_EQUAL_STRING(string_payload.c_str(), payload);
}

void test_state_payload_distinguishes_actual_pending_and_target_backlight() {
    SensorData data{};
    FanStateSnapshot fan{};
    char payload[Config::MQTT_BUFFER_SIZE] = {};

    const size_t written = MqttPayloadBuilder::buildStatePayload(
        payload,
        sizeof(payload),
        data,
        fan,
        false,
        false,
        false,
        false,
        true,
        true,
        false,
        0);

    TEST_ASSERT_GREATER_THAN_UINT32(0, static_cast<uint32_t>(written));
    assert_contains(payload, "\"backlight\":\"OFF\"");
    assert_contains(payload, "\"backlight_transition_pending\":\"ON\"");
    assert_contains(payload, "\"backlight_target_on\":\"ON\"");
}

void test_state_payload_pressure_defaults_to_absolute_without_altitude() {
    SensorData data{};
    data.pressure_valid = true;
    data.pressure = 1009.4f;
    data.pressure_delta_3h_valid = true;
    data.pressure_delta_3h = 1.8f;

    String payload = MqttPayloadBuilder::buildStatePayload(
        data, false, false, false, false, false, 0);

    assert_contains(payload, "\"pressure\":1009.4");
    assert_contains(payload, "\"pressure_absolute\":1009.4");
    assert_contains(payload, "\"pressure_delta_3h\":1.8");
}

void test_state_payload_pressure_uses_msl_and_keeps_absolute_field() {
    SensorData data{};
    data.pressure_valid = true;
    data.pressure = 1000.0f;
    data.pressure_delta_3h_valid = true;
    data.pressure_delta_3h = 12.0f;
    data.pressure_delta_24h_valid = true;
    data.pressure_delta_24h = -8.0f;

    const int16_t altitude_m = 1000;
    const float base = 1.0f - (static_cast<float>(altitude_m) / 44330.0f);
    const float expected_pressure = data.pressure / powf(base, 5.255f);
    const float expected_delta_3h = data.pressure_delta_3h / powf(base, 5.255f);
    const float expected_delta_24h = data.pressure_delta_24h / powf(base, 5.255f);
    char pressure_buf[32];
    char delta3h_buf[32];
    char delta24h_buf[32];
    snprintf(pressure_buf, sizeof(pressure_buf), "\"pressure\":%.1f", expected_pressure);
    snprintf(delta3h_buf, sizeof(delta3h_buf), "\"pressure_delta_3h\":%.1f", expected_delta_3h);
    snprintf(delta24h_buf, sizeof(delta24h_buf), "\"pressure_delta_24h\":%.1f", expected_delta_24h);

    String payload = MqttPayloadBuilder::buildStatePayload(
        data, false, false, false, false, true, altitude_m);

    assert_contains(payload, pressure_buf);
    assert_contains(payload, "\"pressure_absolute\":1000.0");
    assert_contains(payload, delta3h_buf);
    assert_contains(payload, delta24h_buf);
}

void test_state_payload_includes_aqi_when_computable() {
    SensorData data{};
    data.co2_valid = true;
    data.co2 = static_cast<int>(Config::AQ_CO2_YELLOW_MAX_PPM);

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"aqi\":50");
}

void test_state_payload_excludes_aqi_when_only_warmup_gas_metrics_exist() {
    SensorData data{};
    data.voc_valid = true;
    data.voc_index = Config::AQ_VOC_ORANGE_MAX_INDEX;
    data.nox_valid = true;
    data.nox_index = Config::AQ_NOX_YELLOW_MAX_INDEX;
    data.hcho_valid = true;
    data.hcho = 0.0f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, true, false, false, false);

    assert_contains(payload, "\"aqi\":null");
    assert_contains(payload, "\"voc_index\":null");
    assert_contains(payload, "\"nox_index\":null");
}

void test_state_payload_keeps_aqi_available_during_warmup_when_pm_is_ready() {
    SensorData data{};
    data.hcho_valid = true;
    data.hcho = 0.0f;
    data.pm25_valid = true;
    data.pm25 = Config::AQ_PM25_YELLOW_MAX_UGM3;

    String payload = MqttPayloadBuilder::buildStatePayload(data, true, false, false, false);

    assert_contains(payload, "\"aqi\":50");
}

void test_state_payload_hides_hcho_when_only_raw_sample_exists_from_sfa40_warmup_model() {
    SensorData data{};
    data.hcho_valid = false;
    data.hcho = 27.4f;
    data.pm25_valid = true;
    data.pm25 = Config::AQ_PM25_YELLOW_MAX_UGM3;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"hcho\":null");
    assert_contains(payload, "\"aqi\":50");
}

void test_state_payload_includes_optional_gas_generic_and_nh3_compat_fields() {
    SensorData data{};
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_ppm = 12.5f;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::NH3);
    data.nh3_sensor_present = true;
    data.nh3_valid = true;
    data.nh3_ppm = 12.5f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"optional_gas\":12.5");
    assert_contains(payload, "\"optional_gas_type\":\"NH3\"");
    assert_contains(payload, "\"optional_gas_unit\":\"ppm\"");
    assert_contains(payload, "\"optional_gas_decimals\":1");
    assert_contains(payload, "\"optional_gas_ppm_decimals\":1");
    assert_contains(payload, "\"nh3\":12.5");
    assert_contains(payload, "\"so2\":null");
    assert_contains(payload, "\"no2\":null");
}

void test_state_payload_includes_o2_percent_volume_fields_only_for_o2() {
    SensorData data{};
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_ppm = 20.9f;
    data.optional_gas_ppm_decimals = 1;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O2);

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"optional_gas\":20.9");
    assert_contains(payload, "\"optional_gas_type\":\"O2\"");
    assert_contains(payload, "\"optional_gas_unit\":\"%Vol\"");
    assert_contains(payload, "\"optional_gas_decimals\":1");
    assert_contains(payload, "\"optional_gas_ppm_decimals\":1");
    assert_contains(payload, "\"o2\":20.9");
    assert_contains(payload, "\"nh3\":null");
    assert_contains(payload, "\"o3\":null");
    assert_contains(payload, "\"so2\":null");
    assert_contains(payload, "\"no2\":null");
    assert_contains(payload, "\"h2s\":null");
}

void test_state_payload_includes_specific_so2_and_hides_legacy_nh3() {
    SensorData data{};
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_ppm = 7.5f;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::SO2);
    data.nh3_sensor_present = false;
    data.nh3_valid = false;
    data.nh3_ppm = 0.0f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"optional_gas\":7.5");
    assert_contains(payload, "\"optional_gas_type\":\"SO2\"");
    assert_contains(payload, "\"nh3\":null");
    assert_contains(payload, "\"so2\":7.5");
    assert_contains(payload, "\"no2\":null");
}

void test_state_payload_includes_specific_o3_and_hides_other_optional_gases() {
    SensorData data{};
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_ppm = 0.23f;
    data.optional_gas_ppm_decimals = 2;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O3);
    data.nh3_sensor_present = false;
    data.nh3_valid = false;
    data.nh3_ppm = 0.0f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"optional_gas\":0.23");
    assert_contains(payload, "\"optional_gas_type\":\"O3\"");
    assert_contains(payload, "\"nh3\":null");
    assert_contains(payload, "\"o3\":0.23");
    assert_contains(payload, "\"so2\":null");
    assert_contains(payload, "\"no2\":null");
    assert_contains(payload, "\"h2s\":null");

    data.optional_gas_ppm = 1.26f;
    payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);
    assert_contains(payload, "\"optional_gas\":1.3");
    assert_contains(payload, "\"o3\":1.3");
}

void test_state_payload_includes_specific_h2s_and_hides_other_optional_gases() {
    SensorData data{};
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_ppm = 4.0f;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::H2S);
    data.nh3_sensor_present = false;
    data.nh3_valid = false;
    data.nh3_ppm = 0.0f;

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"optional_gas\":4.0");
    assert_contains(payload, "\"optional_gas_type\":\"H2S\"");
    assert_contains(payload, "\"nh3\":null");
    assert_contains(payload, "\"so2\":null");
    assert_contains(payload, "\"no2\":null");
    assert_contains(payload, "\"h2s\":4.0");
}

void test_state_payload_includes_summary_fields_for_air_status_and_issue() {
    SensorData data{};
    data.co2_valid = true;
    data.co2 = static_cast<int>(Config::AQ_CO2_ORANGE_MAX_PPM);

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"aqi\":75");
    assert_contains(payload, "\"air_status\":\"Fair\"");
    assert_contains(payload, "\"main_issue\":\"CO2\"");
}

void test_state_payload_reports_no_issue_when_air_is_good() {
    SensorData data{};
    data.co2_valid = true;
    data.co2 = static_cast<int>(Config::AQ_CO2_YELLOW_MAX_PPM);

    String payload = MqttPayloadBuilder::buildStatePayload(data, false, false, false, false);

    assert_contains(payload, "\"aqi\":50");
    assert_contains(payload, "\"air_status\":\"Good\"");
    assert_contains(payload, "\"main_issue\":\"Clear\"");
}

void test_state_payload_includes_fan_fields_when_present() {
    setMillis(0);
    SensorData data{};
    FanStateSnapshot fan{};
    fan.present = true;
    fan.available = true;
    fan.running = true;
    fan.faulted = false;
    fan.output_known = true;
    fan.mode = FanMode::Auto;
    fan.auto_resume_blocked = false;
    fan.selected_timer_s = 3600U;
    fan.output_mv = 5000;

    String payload = MqttPayloadBuilder::buildStatePayload(data, fan, false, false, false, false);

    assert_contains(payload, "\"fan_present\":\"ON\"");
    assert_contains(payload, "\"fan_available\":\"ON\"");
    assert_contains(payload, "\"fan_running\":\"ON\"");
    assert_contains(payload, "\"fan_manual_running\":\"OFF\"");
    assert_contains(payload, "\"fan_fault\":\"OFF\"");
    assert_contains(payload, "\"fan_auto\":\"ON\"");
    assert_contains(payload, "\"fan_stopped\":\"OFF\"");
    assert_contains(payload, "\"fan_mode\":\"auto\"");
    assert_contains(payload, "\"fan_control_mode\":\"Auto\"");
    assert_contains(payload, "\"fan_timer\":\"1 h\"");
    assert_contains(payload, "\"fan_timer_remaining\":\"Off\"");
    assert_contains(payload, "\"fan_manual_speed\":1");
    assert_contains(payload, "\"fan_manual_percent\":10");
    assert_contains(payload, "\"fan_status\":\"RUNNING\"");
    assert_contains(payload, "\"fan_output_percent\":50");
    assert_contains(payload, "\"fan_output_mv\":5000");
}

void test_state_payload_reports_fan_timer_remaining_when_manual_timer_is_active() {
    setMillis(1000);
    SensorData data{};
    FanStateSnapshot fan{};
    fan.present = true;
    fan.available = true;
    fan.running = true;
    fan.manual_override_active = true;
    fan.mode = FanMode::Manual;
    fan.selected_timer_s = 1800U;
    fan.stop_at_ms = 31UL * 60UL * 1000UL;

    String payload = MqttPayloadBuilder::buildStatePayload(data, fan, false, false, false, false);

    assert_contains(payload, "\"fan_timer_remaining\":\"30 min\"");
}

void test_discovery_sensor_payload_contains_pm05_template_and_topics() {
    const String device_id = "aura_test";
    const String device_name = "Aura \"Kitchen\"";
    const String base_topic = "project_aura/room1";
    const String entity_object_id =
        MqttPayloadBuilder::buildDiscoveryEntityObjectId(base_topic, "pm05");

    String payload = MqttPayloadBuilder::buildDiscoverySensorPayload(
        device_id,
        device_name,
        base_topic,
        "pm05",
        entity_object_id.c_str(),
        "PM0.5",
        "#/cm\\u00b3",
        "",
        "measurement",
        "{{ value_json.pm05 }}",
        "mdi:dots-hexagon");

    assert_contains(payload, "\"name\":\"PM0.5\"");
    assert_contains(payload, "\"unique_id\":\"aura_test_pm05\"");
    assert_contains(payload, "\"object_id\":\"project_aura_room1_pm05\"");
    assert_contains(payload, "\"state_topic\":\"project_aura/room1/state\"");
    assert_contains(payload, "\"availability_topic\":\"project_aura/room1/status\"");
    assert_contains(payload, "\"value_template\":\"{{ value_json.pm05 }}\"");
    assert_contains(payload, "\"unit_of_measurement\":\"#/cm\\u00b3\"");
    assert_contains(payload, "\"state_class\":\"measurement\"");
    assert_contains(payload, "\"icon\":\"mdi:dots-hexagon\"");
    assert_contains(payload, "\"device\":{\"identifiers\":[\"aura_test\"],\"name\":\"Aura \\\"Kitchen\\\"\"");
}

void test_discovery_entity_object_id_sanitizes_base_topic() {
    String object_id = MqttPayloadBuilder::buildDiscoveryEntityObjectId(
        "Project Aura/Kitchen-1", "fan_auto");
    TEST_ASSERT_EQUAL_STRING("project_aura_kitchen_1_fan_auto", object_id.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_state_payload_includes_pm05_pm1_pm4_and_co_null_without_sensor);
    RUN_TEST(test_state_payload_includes_co_when_sensor_present_and_valid);
    RUN_TEST(test_state_payload_buffer_builder_matches_string_payload);
    RUN_TEST(test_state_payload_distinguishes_actual_pending_and_target_backlight);
    RUN_TEST(test_state_payload_pressure_defaults_to_absolute_without_altitude);
    RUN_TEST(test_state_payload_pressure_uses_msl_and_keeps_absolute_field);
    RUN_TEST(test_state_payload_includes_aqi_when_computable);
    RUN_TEST(test_state_payload_excludes_aqi_when_only_warmup_gas_metrics_exist);
    RUN_TEST(test_state_payload_keeps_aqi_available_during_warmup_when_pm_is_ready);
    RUN_TEST(test_state_payload_hides_hcho_when_only_raw_sample_exists_from_sfa40_warmup_model);
    RUN_TEST(test_state_payload_includes_optional_gas_generic_and_nh3_compat_fields);
    RUN_TEST(test_state_payload_includes_o2_percent_volume_fields_only_for_o2);
    RUN_TEST(test_state_payload_includes_specific_so2_and_hides_legacy_nh3);
    RUN_TEST(test_state_payload_includes_specific_o3_and_hides_other_optional_gases);
    RUN_TEST(test_state_payload_includes_specific_h2s_and_hides_other_optional_gases);
    RUN_TEST(test_state_payload_includes_summary_fields_for_air_status_and_issue);
    RUN_TEST(test_state_payload_reports_no_issue_when_air_is_good);
    RUN_TEST(test_state_payload_includes_fan_fields_when_present);
    RUN_TEST(test_state_payload_reports_fan_timer_remaining_when_manual_timer_is_active);
    RUN_TEST(test_discovery_sensor_payload_contains_pm05_template_and_topics);
    RUN_TEST(test_discovery_entity_object_id_sanitizes_base_topic);
    return UNITY_END();
}
