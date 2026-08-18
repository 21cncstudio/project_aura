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
    payload.boot.reset_reason = "POWERON";
    payload.boot.auto_recovery_boot = true;
    payload.boot.i2c_status = "sda_stuck_low";
    payload.boot.sda_high = false;
    payload.boot.scl_high = true;
    payload.boot.board_ready = false;
    payload.boot.board_rounds = 3;
    payload.boot.board_begin_attempts = 1;
    payload.boot.cold_power_start = true;
    payload.boot.cold_power_wait_ms = 7000;
    payload.boot.expander_probe_status = "ready";
    payload.boot.expander_probe_result_valid = true;
    payload.boot.expander_probe_attempts = 4;
    payload.boot.expander_probe_wait_ms = 750;
    payload.boot.expander_probe_error = 0;
    payload.boot.expander_probe_phase = "complete";
    payload.boot.expander_probe_failed_address = 0;
    payload.boot.expander_probe_failed_value = 0;
    payload.boot.expander_probe_bus_recoveries = 2;
    payload.boot.expander_probe_failure_lines_valid = true;
    payload.boot.expander_probe_failure_sda_high = false;
    payload.boot.expander_probe_failure_scl_high = true;
    payload.boot.expander_probe_recovery_sda_high = true;
    payload.boot.expander_probe_recovery_scl_high = true;
    payload.boot.expander_probe_recovery_pulses = 9;
    payload.boot.board_stage = "expander";
    payload.boot.board_failure = "begin";
    payload.boot.lvgl_ready = false;
    payload.boot.previous_backlight_trace_status = "active";
    payload.boot.previous_backlight_trace_valid = true;
    payload.boot.previous_backlight_trace_event = "schedule_wake";
    payload.boot.previous_backlight_trace_stage = "driver_call_begin";
    payload.boot.previous_backlight_trace_driver_result = "unknown";
    payload.boot.previous_backlight_trace_command_result = "failed";
    payload.boot.previous_backlight_trace_sequence = 7;
    payload.boot.previous_backlight_trace_uptime_ms = 20142000;
    payload.boot.previous_backlight_trace_epoch_s = 1786597200;
    payload.boot.previous_backlight_trace_pre_quiet_elapsed_ms = 500;
    payload.boot.previous_backlight_trace_pre_quiet_active_operations = 2;
    payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded = true;
    payload.boot.previous_backlight_trace_pre_quiet_wait_exceeded_active_operations = 3;
    payload.boot.previous_backlight_trace_pre_quiet_forced_by_timeout = true;
    payload.boot.previous_backlight_trace_retention_uncertain = true;
    payload.boot.previous_backlight_trace_expected_network_manager_addr = 0x3fca1000;
    payload.boot.previous_backlight_trace_post_backlight_network_manager_addr = 0x3fca1000;
    payload.boot.previous_backlight_trace_pre_render_network_manager_addr = 0;
    payload.boot.previous_backlight_trace_post_backlight_task_handle = 0x3fcc2000;
    payload.boot.previous_backlight_trace_pre_render_task_handle = 0x3fcc2000;
    payload.boot.previous_backlight_trace_target_on = true;
    payload.boot.previous_backlight_trace_previous_on = false;
    payload.boot.previous_backlight_trace_before_valid = true;
    payload.boot.previous_backlight_trace_before_sda_high = true;
    payload.boot.previous_backlight_trace_before_scl_high = true;

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
    TEST_ASSERT_EQUAL_STRING("POWERON", doc["boot"]["reset_reason"].as<const char *>());
    TEST_ASSERT_TRUE(doc["boot"]["auto_recovery_boot"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("sda_stuck_low", doc["boot"]["i2c_status"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(3, doc["boot"]["board_rounds"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc["boot"]["cold_power_start"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(7000, doc["boot"]["cold_power_wait_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("ready", doc["boot"]["expander_probe_status"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(4, doc["boot"]["expander_probe_attempts"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(750, doc["boot"]["expander_probe_wait_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("complete", doc["boot"]["expander_probe_phase"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(0, doc["boot"]["expander_probe_failed_address"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["boot"]["expander_probe_bus_recoveries"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc["boot"]["expander_probe_failure_lines_valid"].as<bool>());
    TEST_ASSERT_FALSE(doc["boot"]["expander_probe_failure_sda_high"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot"]["expander_probe_recovery_sda_high"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(9, doc["boot"]["expander_probe_recovery_pulses"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("expander", doc["boot"]["board_stage"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("begin", doc["boot"]["board_failure"].as<const char *>());
    TEST_ASSERT_FALSE(doc["boot"]["lvgl_ready"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("active",
                             doc["boot"]["previous_backlight_trace_status"].as<const char *>());
    TEST_ASSERT_TRUE(
        doc["boot"]["previous_backlight_trace_retention_uncertain"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("schedule_wake",
                             doc["boot"]["previous_backlight_trace"]["event"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("driver_call_begin",
                             doc["boot"]["previous_backlight_trace"]["stage"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("unknown",
                             doc["boot"]["previous_backlight_trace"]["driver_result"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("failed",
                             doc["boot"]["previous_backlight_trace"]["command_result"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(7,
                             doc["boot"]["previous_backlight_trace"]["sequence"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(20142000,
                             doc["boot"]["previous_backlight_trace"]["uptime_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(1786597200,
                             doc["boot"]["previous_backlight_trace"]["epoch_s"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(0,
                             doc["boot"]["previous_backlight_trace"]["driver_duration_us"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        500,
        doc["boot"]["previous_backlight_trace"]["pre_quiet_elapsed_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        2,
        doc["boot"]["previous_backlight_trace"]["pre_quiet_active_operations"].as<uint32_t>());
    TEST_ASSERT_TRUE(
        doc["boot"]["previous_backlight_trace"]["pre_quiet_wait_exceeded"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(
        3,
        doc["boot"]["previous_backlight_trace"]
           ["pre_quiet_wait_exceeded_active_operations"].as<uint32_t>());
    TEST_ASSERT_TRUE(
        doc["boot"]["previous_backlight_trace"]["pre_quiet_forced_by_timeout"].as<bool>());
    TEST_ASSERT_TRUE(
        doc["boot"]["previous_backlight_trace"]["retention_uncertain"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(
        0x3fca1000,
        doc["boot"]["previous_backlight_trace"]["expected_network_manager_addr"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        0x3fca1000,
        doc["boot"]["previous_backlight_trace"]["post_backlight_network_manager_addr"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        0,
        doc["boot"]["previous_backlight_trace"]["pre_render_network_manager_addr"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        0x3fcc2000,
        doc["boot"]["previous_backlight_trace"]["post_backlight_task_handle"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(
        0x3fcc2000,
        doc["boot"]["previous_backlight_trace"]["pre_render_task_handle"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["target_on"].as<bool>());
    TEST_ASSERT_FALSE(doc["boot"]["previous_backlight_trace"]["previous_on"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["before_sda_high"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["before_scl_high"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["after_driver_sda_high"].isNull());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["after_driver_scl_high"].isNull());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["after_probe_sda_high"].isNull());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"]["after_probe_scl_high"].isNull());
    TEST_ASSERT_EQUAL_STRING("AuraNet", doc["network"]["wifi_ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(-42, doc["network"]["rssi"].as<int>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["error_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["last_errors"].size());
    TEST_ASSERT_EQUAL_UINT32(7, doc["web_stream"]["ok_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("socket_write_error",
                             doc["web_stream"]["last_abort_reason"].as<const char *>());
    TEST_ASSERT_EQUAL_FLOAT(0.9f, doc["web_stream"]["last_sent_ratio"].as<float>());
}

void test_web_diag_api_utils_marks_missing_backlight_trace_null() {
    WebDiagApiUtils::Payload payload{};
    payload.boot.previous_backlight_trace_status = "power_lost";
    payload.boot.previous_backlight_trace_valid = false;

    ArduinoJson::JsonDocument doc;
    WebDiagApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload, nullptr, 0, 0);

    TEST_ASSERT_EQUAL_STRING(
        "power_lost",
        doc["boot"]["previous_backlight_trace_status"].as<const char *>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"].isNull());
    String json;
    ArduinoJson::serializeJson(doc, json);
    TEST_ASSERT_NULL(strstr(json.c_str(), "crc"));
    TEST_ASSERT_NULL(strstr(json.c_str(), "magic"));
}

void test_web_diag_api_utils_keeps_uncertainty_without_trace_details() {
    WebDiagApiUtils::Payload payload{};
    payload.boot.previous_backlight_trace_status = "corrupt";
    payload.boot.previous_backlight_trace_valid = false;
    payload.boot.previous_backlight_trace_retention_uncertain = true;

    ArduinoJson::JsonDocument doc;
    WebDiagApiUtils::fillJson(
        doc.to<ArduinoJson::JsonObject>(), payload, nullptr, 0, 0);

    TEST_ASSERT_EQUAL_STRING(
        "corrupt",
        doc["boot"]["previous_backlight_trace_status"].as<const char *>());
    TEST_ASSERT_TRUE(
        doc["boot"]["previous_backlight_trace_retention_uncertain"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot"]["previous_backlight_trace"].isNull());
}

void test_web_diag_api_utils_marks_unrun_expander_probe_details_null() {
    WebDiagApiUtils::Payload payload{};
    payload.boot.expander_probe_status = "not_run";
    payload.boot.expander_probe_result_valid = false;

    ArduinoJson::JsonDocument doc;
    WebDiagApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), payload, nullptr, 0, 0);

    const ArduinoJson::JsonObjectConst boot = doc["boot"].as<ArduinoJson::JsonObjectConst>();
    TEST_ASSERT_EQUAL_STRING("not_run", boot["expander_probe_status"].as<const char *>());
    String json;
    ArduinoJson::serializeJson(doc, json);
    const char *detail_fields[] = {
        "expander_probe_attempts",
        "expander_probe_wait_ms",
        "expander_probe_error",
        "expander_probe_phase",
        "expander_probe_failed_address",
        "expander_probe_failed_value",
        "expander_probe_bus_recoveries",
        "expander_probe_failure_lines_valid",
        "expander_probe_failure_sda_high",
        "expander_probe_failure_scl_high",
        "expander_probe_recovery_sda_high",
        "expander_probe_recovery_scl_high",
        "expander_probe_recovery_pulses",
    };
    for (const char *field : detail_fields) {
        const String serialized_null = String("\"") + field + "\":null";
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(json.c_str(), serialized_null.c_str()), field);
        TEST_ASSERT_TRUE_MESSAGE(boot[field].isNull(), field);
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_diag_api_utils_access_allowed_accepts_ap_or_sta_connectivity);
    RUN_TEST(test_web_diag_api_utils_fill_json_populates_network_errors_and_stream);
    RUN_TEST(test_web_diag_api_utils_marks_unrun_expander_probe_details_null);
    RUN_TEST(test_web_diag_api_utils_marks_missing_backlight_trace_null);
    RUN_TEST(test_web_diag_api_utils_keeps_uncertainty_without_trace_details);
    return UNITY_END();
}
