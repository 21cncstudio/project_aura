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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_logger_mirrors_only_web_dashboard_events_to_mqtt_queue);
    RUN_TEST(test_logger_mirroring_respects_recent_dedup_window);
    RUN_TEST(test_capture_pause_prevents_recursive_mqtt_feedback);
    RUN_TEST(test_alert_suppression_keeps_event_mirroring);
    return UNITY_END();
}
