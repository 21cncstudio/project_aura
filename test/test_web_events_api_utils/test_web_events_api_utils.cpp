#include <unity.h>

#include <ArduinoJson.h>
#include <string.h>

#include "core/Logger.h"
#include "web/WebEventsApiUtils.h"

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

void test_web_events_api_utils_fill_json_wraps_events_payload() {
    const Logger::RecentEntry entries[] = {
        make_entry(10, Logger::Info, "WiFi", "connected"),
        make_entry(20, Logger::Warn, "", ""),
        make_entry(30, Logger::Debug, "UI", "skip debug"),
    };

    ArduinoJson::JsonDocument doc;
    WebEventsApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), entries, 3, 123);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(123, doc["uptime_s"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["events"].size());
    TEST_ASSERT_EQUAL_STRING("WiFi", doc["events"][0]["type"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("SYSTEM", doc["events"][1]["type"].as<const char *>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_events_api_utils_fill_json_wraps_events_payload);
    return UNITY_END();
}
