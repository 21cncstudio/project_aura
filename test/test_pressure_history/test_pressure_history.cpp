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
    StorageManager::setTestForceSaveFailure(false);
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    PressureHistory::setNowEpochFn(&mockNow);
}

void tearDown() {
    StorageManager::setTestForceSaveFailure(false);
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

void test_pressure_history_stale_reset_atomically_starts_new_history() {
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
    history.update(1001.0f, data, storage);

    PressureHistoryBlob check = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &check, sizeof(check)));
    TEST_ASSERT_EQUAL_UINT16(1, check.count);
    TEST_ASSERT_EQUAL_UINT32(blob.epoch + Config::PRESSURE_HISTORY_MAX_AGE_S + 1,
                             check.epoch);
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
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_UINT16(1, restored.count);
    TEST_ASSERT_EQUAL_UINT32(saved_epoch + 3UL * 24UL * 60UL * 60UL,
                             restored.epoch);
}

void test_temporal_reset_preserves_old_blob_when_atomic_replace_fails() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    const uint32_t saved_epoch = Config::TIME_VALID_EPOCH + 1000;
    PressureHistoryBlob blob = makeFullHistoryBlob(saved_epoch);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(saved_epoch + 3UL * 24UL * 60UL * 60UL);
    history.load(storage, data);
    StorageManager::setTestForceSaveFailure(true);
    history.update(1001.0f, data, storage);

    PressureHistoryBlob restored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_MEMORY(&blob, &restored, sizeof(blob));

    StorageManager::setTestForceSaveFailure(false);
    advanceStep();
    history.update(1002.0f, data, storage);

    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_UINT16(2, restored.count);
    TEST_ASSERT_EQUAL_UINT32(saved_epoch + 3UL * 24UL * 60UL * 60UL +
                                 Config::PRESSURE_HISTORY_STEP_MS / 1000UL,
                             restored.epoch);
}

void test_flush_commits_pending_samples_and_preserves_previous_blob_on_failure() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    advanceStep();
    history.update(1000.0f, data, storage);

    PressureHistoryBlob stored = {};
    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &stored, sizeof(stored)));
    TEST_ASSERT_TRUE(history.flush(storage));
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &stored, sizeof(stored)));
    const PressureHistoryBlob first_committed = stored;

    advanceStep();
    history.update(1001.0f, data, storage);
    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(history.flush(storage));

    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &stored, sizeof(stored)));
    TEST_ASSERT_EQUAL_MEMORY(&first_committed, &stored, sizeof(stored));
}

void test_restored_history_survives_time_outage_past_save_interval() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    const uint32_t saved_epoch = Config::TIME_VALID_EPOCH + 1000;
    PressureHistoryBlob blob = makeFullHistoryBlob(saved_epoch);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(0);
    history.load(storage, data);
    for (int i = 0; i < 8; ++i) {
        advanceMillis(Config::PRESSURE_HISTORY_STEP_MS);
        history.update(1001.0f + static_cast<float>(i), data, storage);
    }

    PressureHistoryBlob restored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_MEMORY(&blob, &restored, sizeof(blob));
    TEST_ASSERT_FALSE(data.pressure_delta_3h_valid);
    TEST_ASSERT_FALSE(data.pressure_delta_24h_valid);

    const uint32_t reconciled_epoch = saved_epoch + 40UL * 60UL;
    setNowEpoch(reconciled_epoch);
    history.update(1001.0f, data, storage);

    TEST_ASSERT_TRUE(data.pressure_delta_3h_valid);
    TEST_ASSERT_TRUE(data.pressure_delta_24h_valid);
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_UINT32(reconciled_epoch, restored.epoch);
}

void test_load_does_not_trust_pre_rtc_process_time() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    const uint32_t saved_epoch = Config::TIME_VALID_EPOCH + 1000;
    PressureHistoryBlob blob = makeFullHistoryBlob(saved_epoch);
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    // load() happens before TimeManager initializes RTC. A plausible but stale
    // process clock must not delete otherwise valid persisted history.
    setNowEpoch(saved_epoch + Config::PRESSURE_HISTORY_MAX_AGE_S + 1);
    history.load(storage, data);

    PressureHistoryBlob restored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_UINT32(saved_epoch, restored.epoch);

    setNowEpoch(saved_epoch + 10UL * 60UL);
    history.update(1001.0f, data, storage);

    TEST_ASSERT_TRUE(data.pressure_delta_3h_valid);
    TEST_ASSERT_TRUE(data.pressure_delta_24h_valid);
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

void test_empty_stored_history_does_not_block_offline_sampling() {
    StorageManager storage;
    storage.begin();
    PressureHistory history;
    SensorData data;

    PressureHistoryBlob blob = {};
    blob.magic = kPressureHistoryMagic;
    blob.version = kPressureHistoryVersion;
    storage.saveBlobAtomic(StorageManager::kPressurePath, &blob, sizeof(blob));

    setNowEpoch(0);
    history.load(storage, data);
    history.update(1000.0f, data, storage);

    PressureHistoryBlob restored = {};
    TEST_ASSERT_FALSE(storage.loadBlob(StorageManager::kPressurePath, &restored, sizeof(restored)));

    for (int i = 0; i <= Config::PRESSURE_HISTORY_3H_STEPS; ++i) {
        advanceMillis(Config::PRESSURE_HISTORY_STEP_MS);
        history.update(1000.0f + static_cast<float>(i), data, storage);
    }
    TEST_ASSERT_TRUE(data.pressure_delta_3h_valid);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_pressure_history_deltas);
    RUN_TEST(test_pressure_history_stale_reset_atomically_starts_new_history);
    RUN_TEST(test_load_does_not_trust_pre_rtc_process_time);
    RUN_TEST(test_restored_history_waits_for_ntp_before_accepting_first_sample);
    RUN_TEST(test_temporal_reset_preserves_old_blob_when_atomic_replace_fails);
    RUN_TEST(test_flush_commits_pending_samples_and_preserves_previous_blob_on_failure);
    RUN_TEST(test_restored_history_survives_time_outage_past_save_interval);
    RUN_TEST(test_stored_history_without_timestamp_is_rejected);
    RUN_TEST(test_empty_stored_history_does_not_block_offline_sampling);
    return UNITY_END();
}
