#include <string.h>
#include <unity.h>

#include "web/WebApiUtils.h"

void setUp() {}
void tearDown() {}

namespace {

Logger::RecentEntry make_entry(Logger::Level level, const char *tag) {
    Logger::RecentEntry entry;
    entry.level = level;
    strncpy(entry.tag, tag, sizeof(entry.tag) - 1);
    entry.tag[sizeof(entry.tag) - 1] = '\0';
    return entry;
}

} // namespace

void test_web_api_utils_event_texts_match_dashboard_contract() {
    TEST_ASSERT_EQUAL_STRING("E", WebApiUtils::eventLevelText(Logger::Error));
    TEST_ASSERT_EQUAL_STRING("W", WebApiUtils::eventLevelText(Logger::Warn));
    TEST_ASSERT_EQUAL_STRING("I", WebApiUtils::eventLevelText(Logger::Info));
    TEST_ASSERT_EQUAL_STRING("D", WebApiUtils::eventLevelText(Logger::Debug));
    TEST_ASSERT_EQUAL_STRING("critical", WebApiUtils::eventSeverityText(Logger::Error));
    TEST_ASSERT_EQUAL_STRING("warning", WebApiUtils::eventSeverityText(Logger::Warn));
    TEST_ASSERT_EQUAL_STRING("info", WebApiUtils::eventSeverityText(Logger::Info));
}

void test_web_api_utils_should_emit_web_event_filters_mem_and_debug() {
    TEST_ASSERT_FALSE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Info, "Mem")));
    TEST_ASSERT_FALSE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Debug, "WiFi")));
    TEST_ASSERT_TRUE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Warn, "Random")));
    TEST_ASSERT_TRUE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Error, "Random")));
}

void test_web_api_utils_should_emit_web_event_accepts_curated_info_tags() {
    TEST_ASSERT_TRUE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Info, "OTA")));
    TEST_ASSERT_TRUE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Info, "UI")));
    TEST_ASSERT_FALSE(WebApiUtils::shouldEmitWebEvent(make_entry(Logger::Info, "Panel")));
}

void test_web_api_utils_format_uptime_human_formats_days_and_hours() {
    TEST_ASSERT_EQUAL_STRING("0h 59m", WebApiUtils::formatUptimeHuman(3599).c_str());
    TEST_ASSERT_EQUAL_STRING("1h 1m", WebApiUtils::formatUptimeHuman(3660).c_str());
    TEST_ASSERT_EQUAL_STRING("1d 2h 3m", WebApiUtils::formatUptimeHuman(93780).c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_api_utils_event_texts_match_dashboard_contract);
    RUN_TEST(test_web_api_utils_should_emit_web_event_filters_mem_and_debug);
    RUN_TEST(test_web_api_utils_should_emit_web_event_accepts_curated_info_tags);
    RUN_TEST(test_web_api_utils_format_uptime_human_formats_days_and_hours);
    return UNITY_END();
}
