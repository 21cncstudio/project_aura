#include <unity.h>

#include <ArduinoJson.h>

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
    payload.data.co_sensor_present = true;
    payload.data.co_valid = true;
    payload.data.co_ppm = 4.5f;
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
    payload.settings.units_c = true;
    payload.settings.temp_offset = 0.5f;
    payload.settings.hum_offset = -1.0f;
    payload.settings.display_name = "Aura";
    payload.dac_available = true;
    payload.firmware = "1.1.1-test";
    payload.build_date = "Mar 19 2026";
    payload.build_time = "12:00:00";

    ArduinoJson::JsonDocument doc;
    WebStateApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_FLOAT(22.5f, doc["sensors"]["temp"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(4.5f, doc["sensors"]["co"].as<float>());
    TEST_ASSERT_TRUE(doc["sensors"]["co_sensor_present"].as<bool>());
    TEST_ASSERT_FALSE(doc["derived"]["dew_point"].isNull());
    TEST_ASSERT_EQUAL_STRING("AuraNet", doc["network"]["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.2", doc["network"]["mqtt_broker"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("1.1.1-test", doc["system"]["firmware"].as<const char *>());
    TEST_ASSERT_TRUE(doc["system"]["dac_available"].as<bool>());
    TEST_ASSERT_TRUE(doc["settings"]["night_mode"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Aura", doc["settings"]["display_name"].as<const char *>());
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
    TEST_ASSERT_TRUE(doc["derived"]["mold"].isNull());
    TEST_ASSERT_TRUE(doc["network"]["rssi"].isNull());
    TEST_ASSERT_TRUE(doc["settings"]["night_mode"].isNull());
    TEST_ASSERT_EQUAL_STRING("Aura-AP", doc["network"]["wifi_ssid"].as<const char *>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_state_api_utils_fill_json_populates_sensor_network_and_settings_fields);
    RUN_TEST(test_web_state_api_utils_fill_json_sets_nulls_when_values_are_unavailable);
    return UNITY_END();
}
