#include <unity.h>

#include <ArduinoJson.h>

#include "web/WebNetworkUtils.h"

void setUp() {}
void tearDown() {}

void test_web_network_utils_fill_diag_json_sets_diag_specific_fields() {
    WebNetworkUtils::Snapshot snapshot{};
    snapshot.wifi_enabled = true;
    snapshot.sta_connected = true;
    snapshot.scan_in_progress = true;
    snapshot.sta_status = 3;
    snapshot.wifi_ssid = "AuraNet";
    snapshot.ip = "192.168.1.10";
    snapshot.has_hostname = true;
    snapshot.hostname = "aura";
    snapshot.has_rssi = true;
    snapshot.rssi = -47;

    ArduinoJson::JsonDocument doc;
    WebNetworkUtils::fillDiagJson(doc.to<ArduinoJson::JsonObject>(), snapshot);

    TEST_ASSERT_TRUE(doc["wifi_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("sta", doc["mode"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(3, doc["sta_status"].as<int>());
    TEST_ASSERT_TRUE(doc["scan_in_progress"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("AuraNet", doc["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.10", doc["ip"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(-47, doc["rssi"].as<int>());
}

void test_web_network_utils_fill_state_json_sets_mqtt_fields_and_null_rssi() {
    WebNetworkUtils::Snapshot snapshot{};
    snapshot.wifi_enabled = false;
    snapshot.ap_mode = true;
    snapshot.wifi_ssid = "Aura-AP";
    snapshot.ip = "192.168.4.1";
    snapshot.has_mqtt_broker = true;
    snapshot.mqtt_broker = "192.168.1.2";
    snapshot.mqtt_enabled = true;
    snapshot.mqtt_connected = false;

    ArduinoJson::JsonDocument doc;
    WebNetworkUtils::fillStateJson(doc.to<ArduinoJson::JsonObject>(), snapshot);

    TEST_ASSERT_FALSE(doc["wifi_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("ap", doc["mode"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Aura-AP", doc["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", doc["ip"].as<const char *>());
    TEST_ASSERT_TRUE(doc["rssi"].isNull());
    TEST_ASSERT_EQUAL_STRING("aura", doc["hostname"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.2", doc["mqtt_broker"].as<const char *>());
    TEST_ASSERT_TRUE(doc["mqtt_enabled"].as<bool>());
    TEST_ASSERT_FALSE(doc["mqtt_connected"].as<bool>());
}

void test_hostname_status_reports_mismatch_and_failure_history_in_both_apis() {
    WebNetworkUtils::Snapshot snapshot{};
    snapshot.has_hostname = true;
    snapshot.hostname = "aura-1fc944";
    auto &status = snapshot.hostname_status;
    snprintf(status.actual, sizeof(status.actual), "%s", "esp32s3-1FC944");
    status.available = true;
    status.checked = true;
    status.checked_at_ms = 5000;
    status.apply_attempted = true;
    status.apply_error = -1;
    status.apply_failures = 2;
    status.last_failure_error = 0x101;
    status.mismatch_count = 1;
    for (auto fill : {WebNetworkUtils::fillDiagJson, WebNetworkUtils::fillStateJson}) {
        ArduinoJson::JsonDocument doc;
        fill(doc.to<ArduinoJson::JsonObject>(), snapshot);
        TEST_ASSERT_EQUAL_STRING("aura-1fc944", doc["hostname"].as<const char *>());
        TEST_ASSERT_EQUAL_STRING("esp32s3-1FC944", doc["hostname_status"]["sta"].as<const char *>());
        TEST_ASSERT_FALSE(doc["hostname_status"]["matches"].as<bool>());
        TEST_ASSERT_EQUAL_INT32(-1, doc["hostname_status"]["apply_error"].as<int32_t>());
        TEST_ASSERT_EQUAL_UINT32(2, doc["hostname_status"]["apply_failures"].as<uint32_t>());
        TEST_ASSERT_EQUAL_INT32(0x101, doc["hostname_status"]["last_failure_error"].as<int32_t>());
        TEST_ASSERT_EQUAL_UINT32(5000, doc["hostname_status"]["checked_at_ms"].as<uint32_t>());
    }
}

void test_hostname_status_is_unknown_before_interface_observation() {
    WebNetworkUtils::Snapshot snapshot{};
    for (auto fill : {WebNetworkUtils::fillDiagJson, WebNetworkUtils::fillStateJson}) {
        ArduinoJson::JsonDocument doc;
        fill(doc.to<ArduinoJson::JsonObject>(), snapshot);
        for (const char *key : {"sta", "matches", "apply_error", "last_failure_error", "read_error", "checked_at_ms"}) {
            TEST_ASSERT_TRUE(doc["hostname_status"][key].isNull());
        }
        TEST_ASSERT_EQUAL_UINT32(0, doc["hostname_status"]["apply_failures"].as<uint32_t>());
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_network_utils_fill_diag_json_sets_diag_specific_fields);
    RUN_TEST(test_web_network_utils_fill_state_json_sets_mqtt_fields_and_null_rssi);
    RUN_TEST(test_hostname_status_reports_mismatch_and_failure_history_in_both_apis);
    RUN_TEST(test_hostname_status_is_unknown_before_interface_observation);
    return UNITY_END();
}
