#include <unity.h>

#include <ArduinoJson.h>

#include "drivers/DfrOptionalGasSensor.h"
#include "web/WebStateApiUtils.h"

void setUp() {}
void tearDown() {}

void test_web_state_api_utils_fill_json_populates_sensor_network_and_settings_fields() {
    WebStateApiUtils::Payload payload{};
    payload.uptime_s = 3605;
    payload.timestamp_ms = 123456;
    payload.has_time_epoch = true;
    payload.time_epoch_s = 1700000000;
    payload.data.temp_valid = true;
    payload.data.temperature = 22.5f;
    payload.data.hum_valid = true;
    payload.data.humidity = 55.0f;
    payload.data.pressure_valid = true;
    payload.data.pressure = 1013.2f;
    payload.data.pressure_delta_3h_valid = true;
    payload.data.pressure_delta_3h = 1.2f;
    payload.data.hcho_sensor_present = true;
    payload.data.hcho_valid = true;
    payload.data.hcho = 24.0f;
    payload.data.co_sensor_present = true;
    payload.data.co_valid = true;
    payload.data.co_ppm = 4.5f;
    payload.data.optional_gas_sensor_present = true;
    payload.data.optional_gas_valid = true;
    payload.data.optional_gas_ppm = 0.23f;
    payload.data.optional_gas_ppm_decimals = 2;
    payload.data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O3);
    payload.network.wifi_enabled = true;
    payload.network.sta_connected = true;
    payload.network.wifi_ssid = "AuraNet";
    payload.network.ip = "192.168.1.15";
    payload.network.has_rssi = true;
    payload.network.rssi = -45;
    payload.network.has_hostname = true;
    payload.network.hostname = "aura";
    payload.network.has_mqtt_broker = true;
    payload.network.mqtt_broker = "192.168.1.2";
    payload.network.mqtt_enabled = true;
    payload.network.mqtt_connected = true;
    payload.settings.available = true;
    payload.settings.night_mode = true;
    payload.settings.night_mode_locked = false;
    payload.settings.backlight_on = true;
    payload.settings.ntp_enabled = true;
    payload.settings.units_c = true;
    payload.settings.temp_offset = 0.5f;
    payload.settings.hum_offset = -1.0f;
    payload.settings.ntp_server = "router.local";
    payload.settings.display_name = "Aura";
    payload.ntp_active = true;
    payload.ntp_syncing = false;
    payload.ntp_error = false;
    payload.ntp_last_sync_ms = 120000;
    payload.dac_available = true;
    payload.ota_busy = true;
    payload.ota.upload_seen = true;
    payload.ota.session_id = 7;
    payload.ota.success = true;
    payload.ota.reboot_pending = true;
    payload.ota.size_known = true;
    payload.ota.expected_size = 4096;
    payload.ota.slot_size = 8192;
    payload.ota.written_size = 4096;
    payload.ota.upload_start_ms = 120000;
    payload.ota.first_chunk_ms = 120010;
    payload.ota.last_chunk_ms = 123000;
    payload.ota.chunk_count = 64;
    payload.ota.result_set_ms = 123400;
    payload.ota.result_ttl_ms = WebOtaState::terminalResultTtlMs();
    payload.firmware = "1.1.1-test";
    payload.build_date = "Mar 19 2026";
    payload.build_time = "12:00:00";
    payload.thresholds.co2.green = 700.0f;
    payload.thresholds.voc.orange = 325.0f;
    payload.thresholds.nox.yellow = 95.0f;
    payload.thresholds.background_alerts.co2_enabled = false;

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_TRUE(doc["ota_busy"].as<bool>());
    TEST_ASSERT_EQUAL_FLOAT(22.5f, doc["sensors"]["temp"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(24.0f, doc["sensors"]["hcho"].as<float>());
    TEST_ASSERT_TRUE(doc["sensors"]["hcho_sensor_present"].as<bool>());
    TEST_ASSERT_FALSE(doc["sensors"]["hcho_warmup"].as<bool>());
    TEST_ASSERT_EQUAL_FLOAT(4.5f, doc["sensors"]["co"].as<float>());
    TEST_ASSERT_TRUE(doc["sensors"]["co_sensor_present"].as<bool>());
    TEST_ASSERT_EQUAL_FLOAT(0.23f, doc["sensors"]["optional_gas"].as<float>());
    TEST_ASSERT_EQUAL_UINT8(2, doc["sensors"]["optional_gas_ppm_decimals"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("O3", doc["sensors"]["optional_gas_type"].as<const char *>());
    TEST_ASSERT_FALSE(doc["derived"]["dew_point"].isNull());
    TEST_ASSERT_EQUAL_STRING("AuraNet", doc["network"]["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.2", doc["network"]["mqtt_broker"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(7, doc["ota"]["session_id"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("rebooting", doc["ota"]["status"].as<const char *>());
    TEST_ASSERT_TRUE(doc["ota"]["reboot_pending"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Firmware uploaded. Device will reboot.", doc["ota"]["message"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("1.1.1-test", doc["system"]["firmware"].as<const char *>());
    TEST_ASSERT_TRUE(doc["system"]["dac_available"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("ok", doc["system"]["ntp_status"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(120000, doc["system"]["ntp_last_sync_ms"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc["settings"]["night_mode"].as<bool>());
    TEST_ASSERT_TRUE(doc["settings"]["ntp_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("router.local", doc["settings"]["ntp_server"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Aura", doc["settings"]["display_name"].as<const char *>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 700.0f, doc["thresholds"]["metrics"]["co2"]["green"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 325.0f, doc["thresholds"]["metrics"]["voc"]["orange"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 95.0f, doc["thresholds"]["metrics"]["nox"]["yellow"].as<float>());
    TEST_ASSERT_FALSE(doc["thresholds"]["background_alerts"]["co2_enabled"].as<bool>());
}

void test_web_state_api_utils_fill_json_sets_nulls_when_values_are_unavailable() {
    WebStateApiUtils::Payload payload{};
    payload.uptime_s = 10;
    payload.timestamp_ms = 20;
    payload.network.ap_mode = true;
    payload.network.wifi_ssid = "Aura-AP";
    payload.network.ip = "192.168.4.1";
    payload.firmware = "fw";
    payload.build_date = "date";
    payload.build_time = "time";

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["time_epoch_s"].isNull());
    TEST_ASSERT_TRUE(doc["sensors"]["temp"].isNull());
    TEST_ASSERT_FALSE(doc["sensors"]["hcho_sensor_present"].as<bool>());
    TEST_ASSERT_FALSE(doc["sensors"]["hcho_warmup"].as<bool>());
    TEST_ASSERT_TRUE(doc["sensors"]["optional_gas_ppm_decimals"].isNull());
    TEST_ASSERT_TRUE(doc["derived"]["mold"].isNull());
    TEST_ASSERT_TRUE(doc["network"]["rssi"].isNull());
    TEST_ASSERT_EQUAL_STRING("idle", doc["ota"]["status"].as<const char *>());
    TEST_ASSERT_TRUE(doc["ota"]["session_id"].isNull());
    TEST_ASSERT_TRUE(doc["ota"]["message"].isNull());
    TEST_ASSERT_TRUE(doc["ota"]["error_code"].isNull());
    TEST_ASSERT_EQUAL_STRING("off", doc["system"]["ntp_status"].as<const char *>());
    TEST_ASSERT_TRUE(doc["system"]["ntp_last_sync_ms"].isNull());
    TEST_ASSERT_TRUE(doc["settings"]["night_mode"].isNull());
    TEST_ASSERT_TRUE(doc["settings"]["ntp_enabled"].isNull());
    TEST_ASSERT_TRUE(doc["settings"]["ntp_server"].isNull());
    TEST_ASSERT_EQUAL_STRING("Aura-AP", doc["network"]["wifi_ssid"].as<const char *>());
}

void test_web_state_api_utils_hides_reactive_gas_metrics_during_warmup() {
    WebStateApiUtils::Payload payload{};
    payload.gas_warmup = true;
    payload.data.voc_valid = true;
    payload.data.voc_index = 175;
    payload.data.nox_valid = true;
    payload.data.nox_index = 42;

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["sensors"]["gas_warmup"].as<bool>());
    TEST_ASSERT_TRUE(doc["sensors"]["voc"].isNull());
    TEST_ASSERT_TRUE(doc["sensors"]["nox"].isNull());
}

void test_web_state_api_utils_hides_hcho_when_only_raw_sample_exists_from_sfa40_warmup_model() {
    WebStateApiUtils::Payload payload{};
    payload.data.hcho_sensor_present = true;
    payload.data.hcho_warmup = true;
    payload.data.hcho_valid = false;
    payload.data.hcho = 27.4f;

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["sensors"]["hcho"].isNull());
    TEST_ASSERT_TRUE(doc["sensors"]["hcho_sensor_present"].as<bool>());
    TEST_ASSERT_TRUE(doc["sensors"]["hcho_warmup"].as<bool>());
}

void test_web_state_api_utils_reports_failed_ota_with_device_error_code() {
    WebStateApiUtils::Payload payload{};
    payload.timestamp_ms = 5000;
    payload.ota.upload_seen = true;
    payload.ota.session_id = 22;
    payload.ota.size_known = true;
    payload.ota.expected_size = 3713984;
    payload.ota.slot_size = 6553600;
    payload.ota.written_size = 2204509;
    payload.ota.error = "Upload timed out after total deadline of 305972 ms";
    payload.ota.result_set_ms = 4000;
    payload.ota.result_ttl_ms = WebOtaState::terminalResultTtlMs();

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_EQUAL_STRING("failed", doc["ota"]["status"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("UPLOAD_TIMEOUT", doc["ota"]["error_code"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Upload timed out after total deadline of 305972 ms",
                             doc["ota"]["error"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(2204509, doc["ota"]["written"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(3713984, doc["ota"]["expected"].as<uint32_t>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_state_api_utils_fill_json_populates_sensor_network_and_settings_fields);
    RUN_TEST(test_web_state_api_utils_fill_json_sets_nulls_when_values_are_unavailable);
    RUN_TEST(test_web_state_api_utils_hides_reactive_gas_metrics_during_warmup);
    RUN_TEST(test_web_state_api_utils_hides_hcho_when_only_raw_sample_exists_from_sfa40_warmup_model);
    RUN_TEST(test_web_state_api_utils_reports_failed_ota_with_device_error_code);
    return UNITY_END();
}
