#include <unity.h>

#include "ArduinoMock.h"
#include "TimeMock.h"
#include "config/AppConfig.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "modules/ChartsHistory.h"
#include "modules/StorageManager.h"

namespace {

constexpr uint32_t kStepMs = Config::CHART_HISTORY_STEP_MS;
constexpr uint32_t kStepS = Config::CHART_HISTORY_STEP_MS / 1000UL;
constexpr uint32_t kChartsHistoryMagic = 0x43524849; // "CRHI"
constexpr uint16_t kChartsHistoryVersion = 2;

struct ChartsHistoryBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint8_t optional_gas_type;
    uint8_t reserved2;
    uint32_t epoch;
    uint16_t index;
    uint16_t count;
    uint16_t valid_mask[ChartsHistory::kCapacity];
    float values[ChartsHistory::kMetricCount][ChartsHistory::kCapacity];
};

uint16_t metric_bit(ChartsHistory::Metric metric) {
    return static_cast<uint16_t>(1U << static_cast<uint8_t>(metric));
}

void advanceStep() {
    advanceMillis(kStepMs);
    advanceEpoch(kStepS);
}

void set_temp_pressure(SensorData &data, float temp, float pressure) {
    data = SensorData();
    data.temp_valid = true;
    data.temperature = temp;
    data.pressure_valid = true;
    data.pressure = pressure;
}

void set_optional_gas(SensorData &data,
                      DfrOptionalGasSensor::OptionalGasType type,
                      float ppm) {
    data = SensorData();
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_type = static_cast<uint8_t>(type);
    data.optional_gas_ppm = ppm;
}

} // namespace

void setUp() {
    StorageManager::setTestForceSaveFailure(false);
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    ChartsHistory::setNowEpochFn(&mockNow);
}

void tearDown() {
    StorageManager::setTestForceSaveFailure(false);
    ChartsHistory::setNowEpochFn(nullptr);
}

void test_charts_history_gap_marks_null_and_fills_pressure() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    set_temp_pressure(data, 20.0f, 1000.0f);
    advanceStep();
    history.update(data, storage, false, true);

    // 4 steps elapsed since last sample => 3 gap points + 1 current sample.
    advanceMillis(kStepMs * 4);
    advanceEpoch(kStepS * 4);
    set_temp_pressure(data, 24.0f, 1010.0f);
    history.update(data, storage, false, true);

    TEST_ASSERT_EQUAL_UINT16(5, history.count());

    ChartsHistory::Entry entry = {};
    TEST_ASSERT_TRUE(history.entryFromOldest(0, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_TEMPERATURE)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, entry.values[ChartsHistory::METRIC_PRESSURE]);

    TEST_ASSERT_TRUE(history.entryFromOldest(1, entry));
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_TEMPERATURE)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1002.5f, entry.values[ChartsHistory::METRIC_PRESSURE]);

    TEST_ASSERT_TRUE(history.entryFromOldest(2, entry));
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_TEMPERATURE)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1005.0f, entry.values[ChartsHistory::METRIC_PRESSURE]);

    TEST_ASSERT_TRUE(history.entryFromOldest(3, entry));
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_TEMPERATURE)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1007.5f, entry.values[ChartsHistory::METRIC_PRESSURE]);

    TEST_ASSERT_TRUE(history.entryFromOldest(4, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_TEMPERATURE)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, entry.values[ChartsHistory::METRIC_TEMPERATURE]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1010.0f, entry.values[ChartsHistory::METRIC_PRESSURE]);
}

void test_charts_history_quarantines_untrusted_time_until_reconcile() {
    StorageManager storage;
    storage.begin();

    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 21.0f, 1005.0f);

    // >= 30 min to trigger autosave.
    for (int i = 0; i < 8; ++i) {
        advanceStep();
        writer.update(data, storage, false, true);
    }
    TEST_ASSERT_TRUE(writer.count() > 0);
    ChartsHistory durable_before;
    durable_before.load(storage);
    const uint16_t saved_count = durable_before.count();
    const uint32_t saved_epoch = durable_before.latestEpoch();
    TEST_ASSERT_TRUE(saved_count > 0);

    // A plausible process epoch is not enough to delete a restored generation.
    advanceEpoch(Config::CHART_HISTORY_MAX_AGE_S + 5);

    ChartsHistory restored;
    restored.load(storage);
    TEST_ASSERT_EQUAL_UINT16(saved_count, restored.count());
    TEST_ASSERT_EQUAL_UINT32(saved_epoch, restored.latestEpoch());

    set_temp_pressure(data, 30.0f, 1020.0f);
    restored.update(data, storage, false, false);
    TEST_ASSERT_EQUAL_UINT16(saved_count, restored.count());
    TEST_ASSERT_EQUAL_UINT32(saved_epoch, restored.latestEpoch());

    ChartsHistory durable_check;
    durable_check.load(storage);
    TEST_ASSERT_EQUAL_UINT16(saved_count, durable_check.count());
    TEST_ASSERT_EQUAL_UINT32(saved_epoch, durable_check.latestEpoch());

    restored.update(data, storage, false, true);
    TEST_ASSERT_EQUAL_UINT16(1, restored.count());
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(mockNow()),
                             restored.latestEpoch());

    ChartsHistoryBlob replacement = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &replacement, sizeof(replacement)));
    TEST_ASSERT_EQUAL_UINT32(kChartsHistoryMagic, replacement.magic);
    TEST_ASSERT_EQUAL_UINT16(kChartsHistoryVersion, replacement.version);
    TEST_ASSERT_EQUAL_UINT16(1, replacement.count);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(mockNow()),
                             replacement.epoch);
}

void test_fresh_charts_history_samples_offline_without_trusted_time() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    set_temp_pressure(data, 20.0f, 1000.0f);
    history.update(data, storage, false, false);

    TEST_ASSERT_EQUAL_UINT16(1, history.count());
    TEST_ASSERT_EQUAL_UINT32(0, history.latestEpoch());
}

void test_charts_history_temporal_reset_preserves_old_blob_on_save_failure() {
    StorageManager storage;
    storage.begin();
    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 21.0f, 1005.0f);
    for (int i = 0; i < 6; ++i) {
        advanceStep();
        writer.update(data, storage, false, true);
    }

    ChartsHistoryBlob original = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &original, sizeof(original)));
    TEST_ASSERT_TRUE(original.count > 0);

    advanceEpoch(Config::CHART_HISTORY_MAX_AGE_S + 5U);
    ChartsHistory restored;
    restored.load(storage);
    StorageManager::setTestForceSaveFailure(true);
    restored.update(data, storage, false, true);

    ChartsHistoryBlob preserved = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &preserved, sizeof(preserved)));
    TEST_ASSERT_EQUAL_MEMORY(&original, &preserved, sizeof(original));

    StorageManager::setTestForceSaveFailure(false);
    advanceStep();
    restored.update(data, storage, false, true);
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &preserved, sizeof(preserved)));
    TEST_ASSERT_EQUAL_UINT16(2, preserved.count);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(mockNow()), preserved.epoch);
}

void test_unanchored_saved_history_starts_fresh_after_reboot_without_time() {
    StorageManager storage;
    storage.begin();
    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 20.0f, 1000.0f);
    for (int i = 0; i < 6; ++i) {
        advanceMillis(kStepMs);
        writer.update(data, storage, false, false);
    }

    ChartsHistoryBlob unanchored = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &unanchored, sizeof(unanchored)));
    TEST_ASSERT_TRUE(unanchored.count > 1);
    TEST_ASSERT_EQUAL_UINT32(0, unanchored.epoch);

    ChartsHistory rebooted;
    rebooted.load(storage);
    TEST_ASSERT_EQUAL_UINT16(0, rebooted.count());

    set_temp_pressure(data, 30.0f, 1020.0f);
    StorageManager::setTestForceSaveFailure(true);
    rebooted.update(data, storage, false, false);
    TEST_ASSERT_EQUAL_UINT16(1, rebooted.count());
    TEST_ASSERT_EQUAL_UINT32(0, rebooted.latestEpoch());

    ChartsHistoryBlob replacement = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &replacement, sizeof(replacement)));
    TEST_ASSERT_EQUAL_MEMORY(&unanchored, &replacement, sizeof(unanchored));

    StorageManager::setTestForceSaveFailure(false);
    advanceMillis(kStepMs);
    rebooted.update(data, storage, false, false);
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &replacement, sizeof(replacement)));
    TEST_ASSERT_EQUAL_UINT16(2, replacement.count);
    TEST_ASSERT_EQUAL_UINT32(0, replacement.epoch);
    TEST_ASSERT_FLOAT_WITHIN(
        0.01f,
        30.0f,
        replacement.values[ChartsHistory::METRIC_TEMPERATURE][0]);
}

void test_charts_history_small_backward_correction_holds_until_catchup() {
    StorageManager storage;
    storage.begin();
    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 21.0f, 1005.0f);
    for (int i = 0; i < 6; ++i) {
        advanceStep();
        writer.update(data, storage, false, true);
    }

    ChartsHistoryBlob original = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &original, sizeof(original)));

    ChartsHistory restored;
    restored.load(storage);
    setNowEpoch(original.epoch - 1U);
    restored.update(data, storage, false, true);
    TEST_ASSERT_EQUAL_UINT16(original.count, restored.count());

    ChartsHistoryBlob held = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &held, sizeof(held)));
    TEST_ASSERT_EQUAL_MEMORY(&original, &held, sizeof(original));

    setNowEpoch(original.epoch);
    restored.update(data, storage, false, true);
    TEST_ASSERT_EQUAL_UINT16(original.count + 1U, restored.count());

    ChartsHistoryBlob resumed = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &resumed, sizeof(resumed)));
    TEST_ASSERT_EQUAL_UINT16(original.count + 1U, resumed.count);
    TEST_ASSERT_EQUAL_UINT32(original.epoch, resumed.epoch);
}

void test_charts_history_large_backward_correction_replaces_atomically() {
    StorageManager storage;
    storage.begin();
    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 21.0f, 1005.0f);
    for (int i = 0; i < 6; ++i) {
        advanceStep();
        writer.update(data, storage, false, true);
    }

    ChartsHistoryBlob original = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &original, sizeof(original)));

    ChartsHistory restored;
    restored.load(storage);
    const uint32_t corrected_epoch = original.epoch - kStepS - 1U;
    setNowEpoch(corrected_epoch);
    StorageManager::setTestForceSaveFailure(true);
    restored.update(data, storage, false, true);
    TEST_ASSERT_EQUAL_UINT16(1, restored.count());

    ChartsHistoryBlob preserved = {};
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &preserved, sizeof(preserved)));
    TEST_ASSERT_EQUAL_MEMORY(&original, &preserved, sizeof(original));

    StorageManager::setTestForceSaveFailure(false);
    advanceStep();
    restored.update(data, storage, false, true);
    TEST_ASSERT_TRUE(storage.loadBlob(
        StorageManager::kChartsPath, &preserved, sizeof(preserved)));
    TEST_ASSERT_EQUAL_UINT16(2, preserved.count);
    TEST_ASSERT_EQUAL_UINT32(corrected_epoch + kStepS, preserved.epoch);
}

void test_charts_history_records_optional_gas_metric() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    set_optional_gas(data, DfrOptionalGasSensor::OptionalGasType::NH3, 12.5f);
    advanceStep();
    history.update(data, storage, false, true);

    TEST_ASSERT_EQUAL_UINT16(1, history.count());

    ChartsHistory::Entry entry = {};
    TEST_ASSERT_TRUE(history.entryFromOldest(0, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_OPTIONAL_GAS)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, entry.values[ChartsHistory::METRIC_OPTIONAL_GAS]);
}

void test_charts_history_clears_optional_gas_metric_when_type_changes() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    set_optional_gas(data, DfrOptionalGasSensor::OptionalGasType::NH3, 12.5f);
    data.pressure_valid = true;
    data.pressure = 1001.0f;
    advanceStep();
    history.update(data, storage, false, true);
    TEST_ASSERT_EQUAL_UINT16(1, history.count());

    set_optional_gas(data, DfrOptionalGasSensor::OptionalGasType::SO2, 0.08f);
    data.pressure_valid = true;
    data.pressure = 1002.0f;
    advanceStep();
    history.update(data, storage, false, true);

    TEST_ASSERT_EQUAL_UINT16(2, history.count());

    ChartsHistory::Entry entry = {};
    TEST_ASSERT_TRUE(history.entryFromOldest(0, entry));
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_OPTIONAL_GAS)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1001.0f, entry.values[ChartsHistory::METRIC_PRESSURE]);

    TEST_ASSERT_TRUE(history.entryFromOldest(1, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_OPTIONAL_GAS)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.08f, entry.values[ChartsHistory::METRIC_OPTIONAL_GAS]);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_PRESSURE)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1002.0f, entry.values[ChartsHistory::METRIC_PRESSURE]);
}

void test_charts_history_suppresses_reactive_gases_during_warmup() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    data.co2_valid = true;
    data.co2 = 725;
    data.voc_valid = true;
    data.voc_index = 140;
    data.nox_valid = true;
    data.nox_index = 35;

    advanceStep();
    history.update(data, storage, true, true);

    ChartsHistory::Entry entry = {};
    TEST_ASSERT_TRUE(history.entryFromOldest(0, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_CO2)) != 0);
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_VOC)) != 0);
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_NOX)) != 0);

    advanceStep();
    history.update(data, storage, false, true);

    TEST_ASSERT_TRUE(history.entryFromOldest(1, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_CO2)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_VOC)) != 0);
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_NOX)) != 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 140.0f, entry.values[ChartsHistory::METRIC_VOC]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.0f, entry.values[ChartsHistory::METRIC_NOX]);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_charts_history_gap_marks_null_and_fills_pressure);
    RUN_TEST(test_charts_history_quarantines_untrusted_time_until_reconcile);
    RUN_TEST(test_fresh_charts_history_samples_offline_without_trusted_time);
    RUN_TEST(test_charts_history_temporal_reset_preserves_old_blob_on_save_failure);
    RUN_TEST(test_unanchored_saved_history_starts_fresh_after_reboot_without_time);
    RUN_TEST(test_charts_history_small_backward_correction_holds_until_catchup);
    RUN_TEST(test_charts_history_large_backward_correction_replaces_atomically);
    RUN_TEST(test_charts_history_records_optional_gas_metric);
    RUN_TEST(test_charts_history_clears_optional_gas_metric_when_type_changes);
    RUN_TEST(test_charts_history_suppresses_reactive_gases_during_warmup);
    return UNITY_END();
}

