#include <unity.h>

#include "ArduinoMock.h"
#include "TimeMock.h"
#include "config/AppConfig.h"
#include "modules/PressureHistory.h"
#include "modules/StorageManager.h"

namespace {

constexpr uint32_t kPressureHistoryMagic = 0x50524849; // "PRHI"
constexpr uint16_t kPressureHistoryVersion = 1;

struct PressureHistoryBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t epoch;
    uint16_t index;
    uint16_t count;
    float history[Config::PRESSURE_HISTORY_24H_SAMPLES];
};

} // namespace

void setUp() {
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    PressureHistory::setNowEpochFn(&mockNow);
}

void tearDown() {
    PressureHistory::setNowEpochFn(nullptr);
}

static void advanceStep() {
    advanceMillis(Config::PRESSURE_HISTORY_STEP_MS);
    advanceEpoch(Config::PRESSURE_HISTORY_STEP_MS / 1000UL);
}

static PressureHistoryBlob makeFullHistoryBlob(uint32_t epoch) {
    PressureHistoryBlob blob = {};
    blob.magic = kPressureHistoryMagic;
    blob.version = kPressureHistoryVersion;
    blob.epoch = epoch;
    blob.index = 0;
    blob.count = Config::PRESSURE_HISTORY_24H_SAMPLES;
    for (int i = 0; i < Config::PRESSURE_HISTORY_24H_SAMPLES; ++i) {
        blob.history[i] = 990.0f + static_cast<float>(i) * 0.01f;
    }
    return blob;
}

void test_pressure_history_deltas() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    const float base = 1000.0f;

    for (int i = 0; i <= Config::PRESSURE_HISTORY_3H_STEPS; ++i) {
        advanceStep();
        history.update(base + static_cast<float>(i), data, storage);
    }

    TEST_ASSERT_TRUE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,
                             static_cast<float>(Config::PRESSURE_HISTORY_3H_STEPS),
                             data.pressure_delta_3h);

    for (int i = Config::PRESSURE_HISTORY_3H_STEPS + 1;
         i < Config::PRESSURE_HISTORY_24H_SAMPLES;
         ++i) {
        advanceStep();
        history.update(base + static_cast<float>(i), data, storage);
    }

    TEST_ASSERT_TRUE(data.pressure_delta_24h_valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,
                             static_cast<float>(Config::PRESSURE_HISTORY_24H_SAMPLES - 1),
                             data.pressure_delta_24h);
}

void test_pressure_history_stale_reset_clears_storage() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    PressureHistoryBlob blob = {};
    blob.magic = kPressureHistoryMagic;
    blob.version = kPressureHistoryVersion;
    blob.epoch = Config::TIME_VALID_EPOCH + 100;
    blob.index = 10;
    blob.count = 10;
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(blob.epoch + Config::PRESSURE_HISTORY_MAX_AGE_S + 1);
    history.load(storage, data);

    PressureHistoryBlob check = {};
    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &check, sizeof(check)));
    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);
}

void test_restored_history_waits_for_ntp_before_accepting_first_sample() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;
    data.pressure_delta_3h_valid = true;
    data.pressure_delta_24h_valid = true;

    const uint32_t saved_epoch = Config::TIME_VALID_EPOCH + 1000;
    PressureHistoryBlob blob = makeFullHistoryBlob(saved_epoch);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(0);
    history.load(storage, data);
    history.update(1001.0f, data, storage);

    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);
    PressureHistoryBlob restored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_UINT32(saved_epoch, restored.epoch);

    setNowEpoch(saved_epoch + 3UL * 24UL * 60UL * 60UL);
    advanceMillis(1000);
    history.update(1001.0f, data, storage);

    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);
    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
}

void test_restored_history_resets_when_time_wait_expires() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    PressureHistoryBlob blob = makeFullHistoryBlob(Config::TIME_VALID_EPOCH + 1000);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(0);
    history.load(storage, data);
    advanceMillis(Config::PRESSURE_HISTORY_RESTORE_TIME_WAIT_MS - 1);
    history.update(1001.0f, data, storage);

    PressureHistoryBlob restored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);

    advanceMillis(1);
    history.update(1001.0f, data, storage);

    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);
}

void test_stored_history_without_timestamp_is_rejected() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    PressureHistoryBlob blob = makeFullHistoryBlob(0);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(0);
    history.load(storage, data);

    PressureHistoryBlob restored = {};
    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_pressure_history_deltas);
    RUN_TEST(test_pressure_history_stale_reset_clears_storage);
    RUN_TEST(test_restored_history_waits_for_ntp_before_accepting_first_sample);
    RUN_TEST(test_restored_history_resets_when_time_wait_expires);
    RUN_TEST(test_stored_history_without_timestamp_is_rejected);
    return UNITY_END();
}
