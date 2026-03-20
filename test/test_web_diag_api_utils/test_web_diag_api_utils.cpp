#include <unity.h>

#include <ArduinoJson.h>
#include <string.h>

#include "core/Logger.h"
#include "web/WebDiagApiUtils.h"

void setUp() {}
void tearDown() {}

namespace {

Logger::RecentEntry make_entry(uint32_t ms,
                               Logger::Level level,
                               const char *tag,
                               const char *message) {
    Logger::RecentEntry entry{};
    entry.ms = ms;
    entry.level = level;
    if (tag) {
        strncpy(entry.tag, tag, sizeof(entry.tag) - 1);
    }
    if (message) {
        strncpy(entry.message, message, sizeof(entry.message) - 1);
    }
    return entry;
}

} // namespace

void test_web_diag_api_utils_access_allowed_accepts_ap_or_sta_connectivity() {
    TEST_ASSERT_TRUE(WebDiagApiUtils::accessAllowed(true, false));
    TEST_ASSERT_TRUE(WebDiagApiUtils::accessAllowed(false, true));
    TEST_ASSERT_FALSE(WebDiagApiUtils::accessAllowed(false, false));
}

void test_web_diag_api_utils_fill_json_populates_network_errors_and_stream() {
    WebDiagApiUtils::Payload payload{};
    payload.uptime_s = 123;
    payload.ota_busy = true;
    payload.heap_free = 45678;
    payload.heap_min_free = 40000;
    payload.network.wifi_enabled = true;
    payload.network.sta_connected = true;
    payload.network.sta_status = 3;
    payload.network.scan_in_progress = false;
    payload.network.wifi_ssid = "AuraNet";
    payload.network.ip = "192.168.1.15";
    payload.network.has_hostname = true;
    payload.network.hostname = "aura";
    payload.network.has_rssi = true;
    payload.network.rssi = -42;
    payload.web_stream.active_transfers = 1;
    payload.web_stream.mqtt_pause_remaining_ms = 250;
    payload.web_stream.stats.ok_count = 7;
    payload.web_stream.stats.abort_count = 1;
    payload.web_stream.stats.last_abort_reason = StreamAbortReason::SocketWriteError;
    payload.web_stream.stats.last_errno = 113;
    payload.web_stream.stats.last_sent = 90;
    payload.web_stream.stats.last_total = 100;
    payload.web_stream.stats.last_max_write_ms = 220;
    payload.web_stream.stats.last_uri = "/dashboard";

    const Logger::RecentEntry entries[] = {
        make_entry(10, Logger::Warn, "WiFi", "warn"),
        make_entry(20, Logger::Error, "MQTT", "boom"),
        make_entry(30, Logger::Info, "Main", "skip"),
    };

    ArduinoJson::JsonDocument doc;
    WebDiagApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload, entries, 3, 2);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_TRUE(doc["ota_busy"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(45678, doc["heap"]["free"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("AuraNet", doc["network"]["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(-42, doc["network"]["rssi"].as<int>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["error_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["last_errors"].size());
    TEST_ASSERT_EQUAL_UINT32(7, doc["web_stream"]["ok_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("socket_write_error",
                             doc["web_stream"]["last_abort_reason"].as<const char *>());
    TEST_ASSERT_EQUAL_FLOAT(0.9f, doc["web_stream"]["last_sent_ratio"].as<float>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_diag_api_utils_access_allowed_accepts_ap_or_sta_connectivity);
    RUN_TEST(test_web_diag_api_utils_fill_json_populates_network_errors_and_stream);
    return UNITY_END();
}
