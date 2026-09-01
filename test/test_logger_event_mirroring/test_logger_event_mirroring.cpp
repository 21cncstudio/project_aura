#include <unity.h>

#include "ArduinoMock.h"
#include "core/Logger.h"
#include "core/MqttEventQueue.h"
#include "core/SystemEventPolicy.h"

void setUp() {
    setMillis(0);
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(true);
    Logger::resetRecentForTest();
    MqttEventQueue::instance().clear();
}

void tearDown() {
    Logger::resetRecentForTest();
    MqttEventQueue::instance().clear();
}

void test_logger_mirrors_only_web_dashboard_events_to_mqtt_queue() {
    Logger::log(Logger::Info, "WiFi", "connected");
    advanceMillis(1);
    Logger::log(Logger::Info, "Panel", "ignored info");
    advanceMillis(1);
    Logger::log(Logger::Warn, "", "");
    advanceMillis(1);
    Logger::log(Logger::Debug, "UI", "debug noise");

    TEST_ASSERT_EQUAL_UINT32(2, MqttEventQueue::instance().size());

    Logger::RecentEntry first{};
    Logger::RecentEntry second{};
    TEST_ASSERT_TRUE(MqttEventQueue::instance().pop(first));
    TEST_ASSERT_TRUE(MqttEventQueue::instance().pop(second));
    TEST_ASSERT_EQUAL_STRING("WiFi", first.tag);
    TEST_ASSERT_EQUAL_STRING("connected", first.message);
    TEST_ASSERT_EQUAL(Logger::Warn, second.level);
    TEST_ASSERT_EQUAL_STRING("", second.tag);
    TEST_ASSERT_EQUAL_STRING("", second.message);
}

void test_logger_mirroring_respects_recent_dedup_window() {
    Logger::log(Logger::Warn, "WiFi", "link unstable");
    advanceMillis(1000);
    Logger::log(Logger::Warn, "WiFi", "link unstable");

    TEST_ASSERT_EQUAL_UINT32(1, MqttEventQueue::instance().size());
}

void test_capture_pause_prevents_recursive_mqtt_feedback() {
    {
        MqttEventQueue::CapturePause capture_pause;
        Logger::log(Logger::Warn, "MQTT", "event publish failed, reconnecting");
    }

    TEST_ASSERT_EQUAL_UINT32(0, MqttEventQueue::instance().size());
}

void test_alert_suppression_keeps_event_mirroring() {
    Logger::logWithoutAlert(
        Logger::Warn,
        "FanControl",
        "DAC not detected after 5 startup attempts; retries stopped until reboot");

    TEST_ASSERT_EQUAL_UINT32(1, MqttEventQueue::instance().size());
    Logger::RecentEntry entry{};
    TEST_ASSERT_TRUE(MqttEventQueue::instance().pop(entry));
    TEST_ASSERT_EQUAL(Logger::Warn, entry.level);
    TEST_ASSERT_EQUAL_STRING("FanControl", entry.tag);
    TEST_ASSERT_EQUAL_STRING(
        "DAC not detected after 5 startup attempts; retries stopped until reboot",
        entry.message);
}

void test_gt911_info_remains_a_web_event_without_a_user_alert() {
    Logger::log(Logger::Info, "GT911DIAG", "configured identity valid");

    Logger::RecentEntry recent[2]{};
    TEST_ASSERT_EQUAL_UINT32(1, Logger::copyRecent(recent, 2));
    TEST_ASSERT_TRUE(SystemEventPolicy::shouldEmit(recent[0]));
    TEST_ASSERT_EQUAL(Logger::Info, recent[0].level);
    TEST_ASSERT_EQUAL_STRING("GT911DIAG", recent[0].tag);
    TEST_ASSERT_EQUAL_UINT32(0, Logger::copyRecentAlerts(recent, 2));

    Logger::RecentEntry mirrored{};
    TEST_ASSERT_EQUAL_UINT32(1, MqttEventQueue::instance().size());
    TEST_ASSERT_TRUE(MqttEventQueue::instance().pop(mirrored));
    TEST_ASSERT_EQUAL(Logger::Info, mirrored.level);
    TEST_ASSERT_EQUAL_STRING("GT911DIAG", mirrored.tag);
    TEST_ASSERT_EQUAL_STRING("configured identity valid", mirrored.message);
}

void test_gt911_warn_and_error_remain_web_events_and_user_alerts() {
    Logger::log(Logger::Warn, "GT911DIAG", "configured address read timeout");
    advanceMillis(1);
    Logger::log(Logger::Error, "GT911", "address select failed at INT");

    Logger::RecentEntry alerts[2]{};
    TEST_ASSERT_EQUAL_UINT32(2, Logger::copyRecentAlerts(alerts, 2));
    TEST_ASSERT_EQUAL(Logger::Warn, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("GT911DIAG", alerts[0].tag);
    TEST_ASSERT_TRUE(SystemEventPolicy::shouldEmit(alerts[0]));
    TEST_ASSERT_EQUAL(Logger::Error, alerts[1].level);
    TEST_ASSERT_EQUAL_STRING("GT911", alerts[1].tag);
    TEST_ASSERT_TRUE(SystemEventPolicy::shouldEmit(alerts[1]));
    TEST_ASSERT_EQUAL_UINT32(2, MqttEventQueue::instance().size());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_logger_mirrors_only_web_dashboard_events_to_mqtt_queue);
    RUN_TEST(test_logger_mirroring_respects_recent_dedup_window);
    RUN_TEST(test_capture_pause_prevents_recursive_mqtt_feedback);
    RUN_TEST(test_alert_suppression_keeps_event_mirroring);
    RUN_TEST(test_gt911_info_remains_a_web_event_without_a_user_alert);
    RUN_TEST(test_gt911_warn_and_error_remain_web_events_and_user_alerts);
    return UNITY_END();
}
