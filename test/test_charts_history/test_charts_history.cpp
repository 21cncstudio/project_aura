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
    setMillis(0);
    setNowEpoch(Config::TIME_VALID_EPOCH + 1000);
    ChartsHistory::setNowEpochFn(&mockNow);
}

void tearDown() {
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
    history.update(data, storage, false);

    // 4 steps elapsed since last sample => 3 gap points + 1 current sample.
    advanceMillis(kStepMs * 4);
    advanceEpoch(kStepS * 4);
    set_temp_pressure(data, 24.0f, 1010.0f);
    history.update(data, storage, false);

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

void test_charts_history_stale_load_resets_history() {
    StorageManager storage;
    storage.begin();

    ChartsHistory writer;
    writer.load(storage);

    SensorData data;
    set_temp_pressure(data, 21.0f, 1005.0f);

    // >= 30 min to trigger autosave.
    for (int i = 0; i < 8; ++i) {
        advanceStep();
        writer.update(data, storage, false);
    }
    TEST_ASSERT_TRUE(writer.count() > 0);

    // Move time far enough so persisted history becomes stale on next load.
    advanceEpoch(Config::CHART_HISTORY_MAX_AGE_S + 5);

    ChartsHistory restored;
    restored.load(storage);
    TEST_ASSERT_EQUAL_UINT16(0, restored.count());
}

void test_charts_history_records_optional_gas_metric() {
    StorageManager storage;
    storage.begin();
    ChartsHistory history;
    history.load(storage);

    SensorData data;
    set_optional_gas(data, DfrOptionalGasSensor::OptionalGasType::NH3, 12.5f);
    advanceStep();
    history.update(data, storage, false);

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
    history.update(data, storage, false);
    TEST_ASSERT_EQUAL_UINT16(1, history.count());

    set_optional_gas(data, DfrOptionalGasSensor::OptionalGasType::SO2, 0.08f);
    data.pressure_valid = true;
    data.pressure = 1002.0f;
    advanceStep();
    history.update(data, storage, false);

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
    history.update(data, storage, true);

    ChartsHistory::Entry entry = {};
    TEST_ASSERT_TRUE(history.entryFromOldest(0, entry));
    TEST_ASSERT_TRUE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_CO2)) != 0);
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_VOC)) != 0);
    TEST_ASSERT_FALSE((entry.valid_mask & metric_bit(ChartsHistory::METRIC_NOX)) != 0);

    advanceStep();
    history.update(data, storage, false);

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
    RUN_TEST(test_charts_history_stale_load_resets_history);
    RUN_TEST(test_charts_history_records_optional_gas_metric);
    RUN_TEST(test_charts_history_clears_optional_gas_metric_when_type_changes);
    RUN_TEST(test_charts_history_suppresses_reactive_gases_during_warmup);
    return UNITY_END();
}

