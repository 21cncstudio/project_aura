#include <unity.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "ArduinoMock.h"
#include "core/Logger.h"
#include "core/MqttEventQueue.h"

namespace {
bool hasValidConcurrentPayload(const Logger::RecentEntry &entry,
                               bool expect_alert_sequence) {
    if (!memchr(entry.tag, '\0', sizeof(entry.tag)) ||
        !memchr(entry.message, '\0', sizeof(entry.message)) ||
        strcmp(entry.tag, "Concurrency") != 0 ||
        entry.level != Logger::Warn) {
        return false;
    }

    unsigned producer = 0;
    unsigned item = 0;
    char trailing = '\0';
    if (sscanf(entry.message,
               "producer=%u item=%u%c",
               &producer,
               &item,
               &trailing) != 2 ||
        producer >= 4 || item >= 512) {
        return false;
    }

    return expect_alert_sequence ? entry.seq != 0 : entry.seq == 0;
}
}

void setUp() {
    setMillis(0);
    Logger::begin(Serial, Logger::Debug);
    Logger::setSerialOutputEnabled(false);
    Logger::setSensorsSerialOutputEnabled(true);
    Logger::resetRecentForTest();
}

void tearDown() {
    Logger::resetRecentForTest();
}

void test_alert_buffer_keeps_only_warn_and_error() {
    Logger::log(Logger::Info, "WiFi", "connected");
    advanceMillis(1);
    Logger::log(Logger::Warn, "MQTT", "publish delayed");
    advanceMillis(1);
    Logger::log(Logger::Error, "OTA", "write failed");

    Logger::RecentEntry recent[4];
    Logger::RecentEntry alerts[4];

    const size_t recent_count = Logger::copyRecent(recent, 4);
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(3, recent_count);
    TEST_ASSERT_EQUAL_UINT32(2, alert_count);

    TEST_ASSERT_EQUAL(Logger::Warn, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("MQTT", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("publish delayed", alerts[0].message);

    TEST_ASSERT_EQUAL(Logger::Error, alerts[1].level);
    TEST_ASSERT_EQUAL_STRING("OTA", alerts[1].tag);
    TEST_ASSERT_EQUAL_STRING("write failed", alerts[1].message);
}

void test_alert_buffer_survives_info_churn() {
    Logger::log(Logger::Warn, "WiFi", "link unstable");
    for (unsigned i = 0; i < 80; ++i) {
        advanceMillis(1);
        Logger::log(Logger::Info, "Sensors", "sample %u", i);
    }

    Logger::RecentEntry alerts[4];
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(1, alert_count);
    TEST_ASSERT_EQUAL(Logger::Warn, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("WiFi", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("link unstable", alerts[0].message);
}


void test_alert_duplicate_refreshes_sequence_without_adding_second_entry() {
    Logger::log(Logger::Warn, "WiFi", "link unstable");

    Logger::RecentEntry alerts[4];
    const size_t first_count = Logger::copyRecentAlerts(alerts, 4);
    TEST_ASSERT_EQUAL_UINT32(1, first_count);
    const uint32_t first_seq = alerts[0].seq;
    TEST_ASSERT_TRUE(first_seq > 0);

    advanceMillis(1000);
    Logger::log(Logger::Warn, "WiFi", "link unstable");

    const size_t second_count = Logger::copyRecentAlerts(alerts, 4);
    TEST_ASSERT_EQUAL_UINT32(1, second_count);
    TEST_ASSERT_TRUE(alerts[0].seq > first_seq);
    TEST_ASSERT_EQUAL_UINT32(alerts[0].seq, Logger::latestRecentAlertSeq());
}
void test_alert_buffer_excludes_soft_sensor_warnings() {
    Logger::log(Logger::Warn, "Sensors", "CO2 high: 1155 ppm");
    advanceMillis(1);
    Logger::log(Logger::Warn, "Sensors", "PM2.5 elevated: 19.7");
    advanceMillis(1);
    Logger::log(Logger::Error, "WiFi", "sta reconnect failed");

    Logger::RecentEntry alerts[4];
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(1, alert_count);
    TEST_ASSERT_EQUAL(Logger::Error, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("WiFi", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("sta reconnect failed", alerts[0].message);
}

void test_alert_buffer_preserves_hard_errors_during_soft_sensor_warn_churn() {
    Logger::log(Logger::Error, "MQTT", "connect timeout");
    for (unsigned i = 0; i < 80; ++i) {
        advanceMillis(1);
        Logger::log(Logger::Warn, "Sensors", "CO2 high: 1203 ppm");
    }

    Logger::RecentEntry alerts[4];
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(1, alert_count);
    TEST_ASSERT_EQUAL(Logger::Error, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("MQTT", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("connect timeout", alerts[0].message);
}

void test_alert_buffer_keeps_sen66_internal_faults() {
    Logger::log(Logger::Warn, "SEN66", "fan speed warning");
    advanceMillis(1);
    Logger::log(Logger::Error, "SEN66", "PM sensor error");

    Logger::RecentEntry alerts[4];
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(2, alert_count);
    TEST_ASSERT_EQUAL(Logger::Warn, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("SEN66", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("fan speed warning", alerts[0].message);
    TEST_ASSERT_EQUAL(Logger::Error, alerts[1].level);
    TEST_ASSERT_EQUAL_STRING("SEN66", alerts[1].tag);
    TEST_ASSERT_EQUAL_STRING("PM sensor error", alerts[1].message);
}

void test_alert_buffer_excludes_optional_absence_but_keeps_faults() {
    Logger::logWithoutAlert(
        Logger::Warn,
        "SEN0466",
        "addr=0x74 stage=address-probe err=-1(ESP_FAIL) lines before=1/1 after=1/1");
    advanceMillis(1);
    Logger::logWithoutAlert(
        Logger::Warn,
        "FanControl",
        "DAC not detected after 5 startup attempts; retries stopped until reboot");
    advanceMillis(1);
    TEST_ASSERT_EQUAL_UINT32(0, Logger::latestRecentAlertSeq());

    Logger::log(Logger::Warn,
                "SEN0466",
                "addr=0x74 stage=address-probe err=263(ESP_ERR_TIMEOUT) lines before=1/1 after=0/1");
    advanceMillis(1);
    Logger::log(Logger::Warn, "FanControl", "DAC init failed: range write failed");

    Logger::RecentEntry recent[4];
    Logger::RecentEntry alerts[4];
    const size_t recent_count = Logger::copyRecent(recent, 4);
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 4);

    TEST_ASSERT_EQUAL_UINT32(4, recent_count);
    TEST_ASSERT_EQUAL(Logger::Warn, recent[0].level);
    TEST_ASSERT_EQUAL_STRING("SEN0466", recent[0].tag);
    TEST_ASSERT_EQUAL_STRING(
        "addr=0x74 stage=address-probe err=-1(ESP_FAIL) lines before=1/1 after=1/1",
        recent[0].message);
    TEST_ASSERT_EQUAL(Logger::Warn, recent[1].level);
    TEST_ASSERT_EQUAL_STRING("FanControl", recent[1].tag);
    TEST_ASSERT_EQUAL_STRING(
        "DAC not detected after 5 startup attempts; retries stopped until reboot",
        recent[1].message);

    TEST_ASSERT_EQUAL_UINT32(2, alert_count);
    TEST_ASSERT_EQUAL(Logger::Warn, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("SEN0466", alerts[0].tag);
    TEST_ASSERT_EQUAL(Logger::Warn, alerts[1].level);
    TEST_ASSERT_EQUAL_STRING("FanControl", alerts[1].tag);
    TEST_ASSERT_EQUAL_STRING("DAC init failed: range write failed", alerts[1].message);
}

void test_error_cannot_be_suppressed_from_alert_buffer() {
    Logger::logWithoutAlert(Logger::Error, "I2C", "bus unavailable");

    Logger::RecentEntry alerts[1];
    const size_t alert_count = Logger::copyRecentAlerts(alerts, 1);
    TEST_ASSERT_EQUAL_UINT32(1, alert_count);
    TEST_ASSERT_EQUAL(Logger::Error, alerts[0].level);
    TEST_ASSERT_EQUAL_STRING("I2C", alerts[0].tag);
    TEST_ASSERT_EQUAL_STRING("bus unavailable", alerts[0].message);
}

void test_concurrent_writers_and_readers_keep_recent_snapshots_coherent() {
    constexpr unsigned kWriterCount = 4;
    constexpr unsigned kEntriesPerWriter = 512;

    std::atomic<unsigned> ready{0};
    std::atomic<unsigned> writers_finished{0};
    std::atomic<unsigned> concurrent_snapshots{0};
    std::atomic<bool> start{false};
    std::atomic<bool> snapshot_invalid{false};

    auto inspect_snapshots = [&]() {
        Logger::RecentEntry recent[64]{};
        const size_t recent_count = Logger::copyRecent(recent, 64);
        for (size_t i = 0; i < recent_count; ++i) {
            if (!hasValidConcurrentPayload(recent[i], false)) {
                return false;
            }
        }

        Logger::RecentEntry alerts[32]{};
        const size_t alert_count = Logger::copyRecentAlerts(alerts, 32);
        for (size_t i = 0; i < alert_count; ++i) {
            if (!hasValidConcurrentPayload(alerts[i], true)) {
                return false;
            }
            if (i > 0 && alerts[i].seq != alerts[i - 1].seq + 1) {
                return false;
            }
        }

        if (alert_count > 0 &&
            Logger::latestRecentAlertSeq() < alerts[alert_count - 1].seq) {
            return false;
        }
        return true;
    };

    // Keep MQTT mirroring out of this stress test so it exercises only the
    // Logger snapshots and their shared alert sequence.
    MqttEventQueue::CapturePause capture_pause;

    std::thread reader([&]() {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        while (writers_finished.load(std::memory_order_acquire) < kWriterCount) {
            if (!inspect_snapshots()) {
                snapshot_invalid.store(true, std::memory_order_release);
                return;
            }
            concurrent_snapshots.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    std::thread writers[kWriterCount];
    for (unsigned producer = 0; producer < kWriterCount; ++producer) {
        writers[producer] = std::thread([&, producer]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (unsigned item = 0; item < kEntriesPerWriter; ++item) {
                Logger::log(Logger::Warn,
                            "Concurrency",
                            "producer=%u item=%u",
                            producer,
                            item);
                if ((item % 32) == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            writers_finished.fetch_add(1, std::memory_order_release);
        });
    }

    while (ready.load(std::memory_order_acquire) < kWriterCount + 1) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto &writer : writers) {
        writer.join();
    }
    reader.join();

    TEST_ASSERT_FALSE(snapshot_invalid.load(std::memory_order_acquire));
    TEST_ASSERT_TRUE(concurrent_snapshots.load(std::memory_order_relaxed) > 0);
    TEST_ASSERT_TRUE(inspect_snapshots());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_alert_buffer_keeps_only_warn_and_error);
    RUN_TEST(test_alert_buffer_survives_info_churn);
    RUN_TEST(test_alert_duplicate_refreshes_sequence_without_adding_second_entry);
    RUN_TEST(test_alert_buffer_excludes_soft_sensor_warnings);
    RUN_TEST(test_alert_buffer_preserves_hard_errors_during_soft_sensor_warn_churn);
    RUN_TEST(test_alert_buffer_keeps_sen66_internal_faults);
    RUN_TEST(test_alert_buffer_excludes_optional_absence_but_keeps_faults);
    RUN_TEST(test_error_cannot_be_suppressed_from_alert_buffer);
    RUN_TEST(test_concurrent_writers_and_readers_keep_recent_snapshots_coherent);
    return UNITY_END();
}


