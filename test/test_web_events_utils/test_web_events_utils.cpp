#include <unity.h>

#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>

#include "core/Logger.h"
#include "web/WebEventsUtils.h"

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

void test_web_events_utils_fill_recent_errors_json_filters_and_limits() {
    const Logger::RecentEntry entries[] = {
        make_entry(10, Logger::Warn, "Main", "older warning"),
        make_entry(20, Logger::Info, "Main", "skip"),
        make_entry(30, Logger::Error, "MQTT", "boom"),
        make_entry(35, Logger::Debug, "UI", "skip debug"),
        make_entry(40, Logger::Warn, "", ""),
        make_entry(50, Logger::Info, "WiFi", "skip trailing info"),
    };

    ArduinoJson::JsonDocument doc;
    const size_t added = WebEventsUtils::fillRecentErrorsJson(
        doc["errors"].to<ArduinoJson::JsonArray>(), entries, 6, 2);

    TEST_ASSERT_EQUAL_UINT32(2, added);
    TEST_ASSERT_EQUAL_UINT32(2, doc["errors"].size());
    TEST_ASSERT_EQUAL_UINT32(30, doc["errors"][0]["ts_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("E", doc["errors"][0]["level"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("MQTT", doc["errors"][0]["tag"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("boom", doc["errors"][0]["message"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(40, doc["errors"][1]["ts_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("W", doc["errors"][1]["level"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("SYSTEM", doc["errors"][1]["tag"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("", doc["errors"][1]["message"].as<const char *>());
}

void test_web_events_utils_recent_errors_keeps_all_when_limit_exceeds_count() {
    const Logger::RecentEntry entries[] = {
        make_entry(10, Logger::Info, "Main", "skip"),
        make_entry(20, Logger::Warn, "WiFi", "warning"),
        make_entry(30, Logger::Error, "GT911", "failure"),
    };

    ArduinoJson::JsonDocument doc;
    const size_t added = WebEventsUtils::fillRecentErrorsJson(
        doc["errors"].to<ArduinoJson::JsonArray>(), entries, 3, 8);

    TEST_ASSERT_EQUAL_UINT32(2, added);
    TEST_ASSERT_EQUAL_UINT32(20, doc["errors"][0]["ts_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(30, doc["errors"][1]["ts_ms"].as<uint32_t>());
}

void test_web_events_utils_recent_errors_handles_empty_inputs() {
    const Logger::RecentEntry entry =
        make_entry(10, Logger::Warn, "Main", "warning");
    const Logger::RecentEntry info =
        make_entry(20, Logger::Info, "Main", "skip");
    ArduinoJson::JsonDocument doc;
    const ArduinoJson::JsonArray errors =
        doc["errors"].to<ArduinoJson::JsonArray>();

    TEST_ASSERT_EQUAL_UINT32(
        0, WebEventsUtils::fillRecentErrorsJson(errors, nullptr, 1, 1));
    TEST_ASSERT_EQUAL_UINT32(
        0, WebEventsUtils::fillRecentErrorsJson(errors, &entry, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(
        0, WebEventsUtils::fillRecentErrorsJson(errors, &entry, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(
        0, WebEventsUtils::fillRecentErrorsJson(errors, &info, 1, 1));
    TEST_ASSERT_EQUAL_UINT32(0, errors.size());
}

void test_web_events_utils_recent_errors_preserves_order_across_uptime_rollover() {
    const Logger::RecentEntry entries[] = {
        make_entry(UINT32_MAX - 8U, Logger::Warn, "I2C", "older warning"),
        make_entry(UINT32_MAX - 1U, Logger::Error, "GT911", "before rollover"),
        make_entry(2, Logger::Warn, "I2C", "after rollover"),
    };

    ArduinoJson::JsonDocument doc;
    const size_t added = WebEventsUtils::fillRecentErrorsJson(
        doc["errors"].to<ArduinoJson::JsonArray>(), entries, 3, 2);

    TEST_ASSERT_EQUAL_UINT32(2, added);
    TEST_ASSERT_EQUAL_UINT32(
        UINT32_MAX - 1U, doc["errors"][0]["ts_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["errors"][1]["ts_ms"].as<uint32_t>());
}

void test_web_events_utils_fill_events_json_uses_web_event_rules_and_fallbacks() {
    const Logger::RecentEntry entries[] = {
        make_entry(10, Logger::Debug, "UI", "skip debug"),
        make_entry(20, Logger::Info, "WiFi", "connected"),
        make_entry(30, Logger::Info, "Mem", "skip mem"),
        make_entry(40, Logger::Warn, "", ""),
    };

    ArduinoJson::JsonDocument doc;
    const uint32_t emitted = WebEventsUtils::fillEventsJson(
        doc["events"].to<ArduinoJson::JsonArray>(), entries, 4);

    TEST_ASSERT_EQUAL_UINT32(2, emitted);
    TEST_ASSERT_EQUAL_UINT32(2, doc["events"].size());
    TEST_ASSERT_EQUAL_STRING("WiFi", doc["events"][0]["type"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("info", doc["events"][0]["severity"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("SYSTEM", doc["events"][1]["type"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Event", doc["events"][1]["message"].as<const char *>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_events_utils_fill_recent_errors_json_filters_and_limits);
    RUN_TEST(test_web_events_utils_recent_errors_keeps_all_when_limit_exceeds_count);
    RUN_TEST(test_web_events_utils_recent_errors_handles_empty_inputs);
    RUN_TEST(test_web_events_utils_recent_errors_preserves_order_across_uptime_rollover);
    RUN_TEST(test_web_events_utils_fill_events_json_uses_web_event_rules_and_fallbacks);
    return UNITY_END();
}
