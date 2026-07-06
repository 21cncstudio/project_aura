#include <unity.h>

#include <map>
#include <string>
#include <vector>
#include <cstring>

#include "ArduinoMock.h"
#include "TimeMock.h"
#include "config/AppConfig.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "modules/DailyExtremaHistory.h"

namespace {

constexpr time_t kDayOneNoon = 1782561600; // 2026-06-27 12:00:00 UTC
constexpr time_t kDayTwoNoon = 1782648000; // 2026-06-28 12:00:00 UTC

class FakeDailyStorage final : public DailyHistoryStorage {
public:
    bool ready = true;
    bool fail_append = false;
    bool fail_write = false;
    bool fail_remove = false;
    uint32_t write_count = 0;
    std::map<std::string, std::string> text_files;
    std::map<std::string, std::vector<uint8_t>> binary_files;

    bool isReady() const override { return ready; }

    bool fileInfo(const char *path, bool &exists, size_t &out_size) const override {
        exists = false;
        out_size = 0;
        if (!ready || !path) {
            return false;
        }
        auto text_it = text_files.find(path);
        if (text_it != text_files.end()) {
            exists = true;
            out_size = text_it->second.size();
            return true;
        }
        auto bin_it = binary_files.find(path);
        if (bin_it != binary_files.end()) {
            exists = true;
            out_size = bin_it->second.size();
            return true;
        }
        return true;
    }

    bool fileExists(const char *path) const override {
        bool exists = false;
        size_t size = 0;
        return fileInfo(path, exists, size) && exists;
    }

    bool fileSize(const char *path, size_t &out_size) const override {
        bool exists = false;
        return fileInfo(path, exists, out_size) && exists;
    }

    bool appendText(const char *path, const char *text) override {
        if (!ready || fail_append || !path || !text) {
            return false;
        }
        text_files[path] += text;
        return true;
    }

    bool readBinary(const char *path, void *out, size_t len, size_t &out_len) const override {
        out_len = 0;
        if (!ready || !path || !out) {
            return false;
        }
        auto it = binary_files.find(path);
        if (it == binary_files.end()) {
            return false;
        }
        out_len = it->second.size() < len ? it->second.size() : len;
        memcpy(out, it->second.data(), out_len);
        return true;
    }

    bool writeBinaryAtomic(const char *path, const void *data, size_t len) override {
        ++write_count;
        if (fail_write) {
            return false;
        }
        if (!ready || !path || !data) {
            return false;
        }
        const auto *bytes = static_cast<const uint8_t *>(data);
        binary_files[path] = std::vector<uint8_t>(bytes, bytes + len);
        return true;
    }

    bool removeFile(const char *path) override {
        if (!path || fail_remove) {
            return false;
        }
        const size_t text_removed = text_files.erase(path);
        const size_t binary_removed = binary_files.erase(path);
        return text_removed > 0 || binary_removed > 0;
    }
};

void set_basic_data(SensorData &data, int co2, float temp, float pm25) {
    data = SensorData{};
    data.co2_valid = true;
    data.co2 = co2;
    data.temp_valid = true;
    data.temperature = temp;
    data.pm25_valid = true;
    data.pm25 = pm25;
}

void set_pressure(SensorData &data, float pressure_hpa) {
    data.pressure_valid = true;
    data.pressure = pressure_hpa;
}

void downgrade_current_state_to_v1_metric(FakeDailyStorage &storage) {
    auto &blob = storage.binary_files[DailyExtremaHistory::kStatePath];
    TEST_ASSERT_TRUE(blob.size() > 13);
    blob[4] = 1;
    blob[5] = 0;
    blob[13] = 0;
}

} // namespace

void setUp() {
    setMillis(0);
    setNowEpoch(kDayOneNoon);
    DailyExtremaHistory::setNowEpochFn(&mockNow);
}

void tearDown() {
    DailyExtremaHistory::setNowEpochFn(nullptr);
}

void test_daily_extrema_tracks_min_max_and_peak_times() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 500, 21.5f, 5.0f);
    set_pressure(data, 1000.0f);
    history.update(data, getMillis());

    advanceEpoch(3600);
    advanceMillis(3600UL * 1000UL);
    set_basic_data(data, 1200, 19.0f, 42.0f);
    set_pressure(data, 1003.4f);
    history.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    advanceMillis(24UL * 60UL * 60UL * 1000UL);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("date,metric,unit,min,min_time,max,max_time,sample_count\n") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,500") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",1200,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,C,19.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pressure,hPa,1000.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",1003.4,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pm25,ug/m3,5.0") != std::string::npos);
}

void test_daily_extrema_ignores_invalid_values() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 700, 20.0f, 10.0f);
    history.update(data, getMillis());

    advanceEpoch(60);
    data.co2_valid = false;
    data.co2 = 3000;
    data.temp_valid = true;
    data.temperature = 25.0f;
    history.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,700") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("3000") == std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,C,20.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",25.0,") != std::string::npos);
}

void test_daily_extrema_exports_current_day_without_finalizing() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());

    String current_csv;
    TEST_ASSERT_TRUE(history.currentDayCsv(current_csv));

    const std::string csv = current_csv.c_str();
    TEST_ASSERT_TRUE(csv.find("date,metric,unit,min,min_time,max,max_time,sample_count\n") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,650") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,C,21.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pm25,ug/m3,8.5") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(0, storage.text_files.count(DailyExtremaHistory::kDailyCsvPath));
    TEST_ASSERT_EQUAL_UINT32(20260627U, history.currentDayKey());
}

void test_daily_extrema_clear_current_day_resets_live_state() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());

    TEST_ASSERT_TRUE(history.hasCurrentDay());
    history.clearCurrentDay();

    String current_csv;
    TEST_ASSERT_FALSE(history.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(0, history.currentSampleCount());
    TEST_ASSERT_FALSE(history.currentDayCsv(current_csv));
}

void test_daily_extrema_clear_current_day_removes_persisted_state() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());

    TEST_ASSERT_TRUE(storage.fileExists(DailyExtremaHistory::kStatePath));

    const DailyExtremaHistory::ClearCurrentDayResult result = history.clearCurrentDay(true);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(result.state_existed);
    TEST_ASSERT_FALSE(storage.fileExists(DailyExtremaHistory::kStatePath));
    TEST_ASSERT_FALSE(history.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(0, history.currentSampleCount());
}

void test_daily_extrema_clear_current_day_preserves_ram_when_state_remove_fails() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());

    storage.fail_remove = true;
    const DailyExtremaHistory::ClearCurrentDayResult result = history.clearCurrentDay(true);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_TRUE(result.state_existed);
    TEST_ASSERT_TRUE(storage.fileExists(DailyExtremaHistory::kStatePath));
    TEST_ASSERT_TRUE(history.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(3, history.currentSampleCount());
}

void test_daily_extrema_flush_persists_dirty_state_before_interval() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    advanceMillis(1000);
    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());
    TEST_ASSERT_EQUAL_UINT32(1, storage.write_count);

    advanceEpoch(60);
    advanceMillis(60UL * 1000UL);
    set_basic_data(data, 650, 30.0f, 8.5f);
    history.update(data, getMillis());
    TEST_ASSERT_EQUAL_UINT32(1, storage.write_count);

    history.flush();
    TEST_ASSERT_EQUAL_UINT32(2, storage.write_count);

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData empty_data{};
    restored.update(empty_data, getMillis());

    String current_csv;
    TEST_ASSERT_TRUE(restored.currentDayCsv(current_csv));
    const std::string csv = current_csv.c_str();
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,C,21.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",30.0,") != std::string::npos);
}

void test_daily_extrema_restores_current_day_state() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 800, 22.0f, 12.0f);
        writer.update(data, getMillis());
        writer.flush();
    }

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    advanceEpoch(120);
    SensorData data;
    set_basic_data(data, 450, 23.0f, 9.0f);
    restored.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    restored.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,450") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",800,") != std::string::npos);
}

void test_daily_extrema_exports_imperial_temperature_and_pressure() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, false);

    SensorData data;
    set_basic_data(data, 500, 0.0f, 5.0f);
    set_pressure(data, 1013.25f);
    history.update(data, getMillis());

    advanceEpoch(3600);
    set_basic_data(data, 1200, 10.0f, 42.0f);
    set_pressure(data, 1000.0f);
    history.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,F,32.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",50.0,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pressure,inHg,29.53") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",29.92,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,500") != std::string::npos);
}

void test_daily_extrema_unit_switch_applies_to_next_day_only() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, false);

    SensorData data;
    set_basic_data(data, 500, 0.0f, 5.0f);
    set_pressure(data, 1013.25f);
    history.update(data, getMillis());

    history.setPreferredUnitsC(true);
    TEST_ASSERT_FALSE(history.currentDayUnitsC());
    TEST_ASSERT_TRUE(history.preferredUnitsC());

    advanceEpoch(3600);
    set_basic_data(data, 1200, 10.0f, 42.0f);
    set_pressure(data, 1000.0f);
    history.update(data, getMillis());

    String current_csv;
    TEST_ASSERT_TRUE(history.currentDayCsv(current_csv));
    TEST_ASSERT_TRUE(std::string(current_csv.c_str()).find("2026-06-27,temperature,F,32.0") != std::string::npos);

    setNowEpoch(kDayTwoNoon);
    set_basic_data(data, 700, 20.0f, 7.0f);
    set_pressure(data, 990.0f);
    history.update(data, getMillis());

    const std::string daily_csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(daily_csv.find("2026-06-27,temperature,F,32.0") != std::string::npos);

    String next_csv;
    TEST_ASSERT_TRUE(history.currentDayCsv(next_csv));
    TEST_ASSERT_TRUE(std::string(next_csv.c_str()).find("2026-06-28,temperature,C,20.0") != std::string::npos);
    TEST_ASSERT_TRUE(history.currentDayUnitsC());
}

void test_daily_extrema_restores_v2_units_from_state() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, false);
        SensorData data;
        set_basic_data(data, 800, 0.0f, 12.0f);
        set_pressure(data, 1013.25f);
        writer.update(data, getMillis());
        writer.flush();
    }

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    advanceEpoch(120);
    SensorData data;
    set_basic_data(data, 450, 10.0f, 9.0f);
    set_pressure(data, 1000.0f);
    restored.update(data, getMillis());

    TEST_ASSERT_FALSE(restored.currentDayUnitsC());
    TEST_ASSERT_TRUE(restored.preferredUnitsC());

    setNowEpoch(kDayTwoNoon);
    restored.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,F,32.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pressure,inHg,29.53") != std::string::npos);
}

void test_daily_extrema_migrates_v1_state_as_metric() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, false);
        SensorData data;
        set_basic_data(data, 800, 0.0f, 12.0f);
        set_pressure(data, 1013.25f);
        writer.update(data, getMillis());
        writer.flush();
    }
    downgrade_current_state_to_v1_metric(storage);

    DailyExtremaHistory restored;
    restored.begin(storage, false);
    advanceEpoch(120);
    SensorData data;
    set_basic_data(data, 450, 10.0f, 9.0f);
    set_pressure(data, 1000.0f);
    restored.update(data, getMillis());

    TEST_ASSERT_TRUE(restored.currentDayUnitsC());

    setNowEpoch(kDayTwoNoon);
    restored.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,temperature,C,0.0") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,pressure,hPa,1000.0") != std::string::npos);
}

void test_daily_extrema_resets_optional_gas_when_type_changes() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::NH3);
    data.optional_gas_ppm = 12.5f;
    history.update(data, getMillis());

    advanceEpoch(60);
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::SO2);
    data.optional_gas_ppm = 0.08f;
    history.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,optional_gas,ppm,0.08") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("12.50") == std::string::npos);
}

void test_daily_extrema_keeps_previous_day_when_csv_append_fails() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 700, 21.0f, 7.0f);
    history.update(data, getMillis());

    storage.fail_append = true;
    setNowEpoch(kDayTwoNoon);
    advanceMillis(24UL * 60UL * 60UL * 1000UL);
    set_basic_data(data, 900, 23.0f, 12.0f);
    history.update(data, getMillis());

    TEST_ASSERT_EQUAL_UINT32(20260627U, history.currentDayKey());
    TEST_ASSERT_FALSE(history.lastWriteOk());
    TEST_ASSERT_EQUAL_UINT32(0, storage.text_files.count(DailyExtremaHistory::kDailyCsvPath));

    String current_csv;
    TEST_ASSERT_TRUE(history.currentDayCsv(current_csv));
    const std::string stale_csv = current_csv.c_str();
    TEST_ASSERT_TRUE(stale_csv.find("2026-06-27,co2,ppm,700") != std::string::npos);
    TEST_ASSERT_TRUE(stale_csv.find("2026-06-28") == std::string::npos);

    storage.fail_append = false;
    history.update(data, getMillis());

    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    const std::string daily_csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(daily_csv.find("2026-06-27,co2,ppm,700") != std::string::npos);
}

void test_daily_extrema_failed_state_save_retries_after_short_backoff() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    advanceMillis(1000);
    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());
    TEST_ASSERT_EQUAL_UINT32(1, storage.write_count);

    storage.fail_write = true;
    advanceEpoch(60);
    advanceMillis(DailyExtremaHistory::kStateSaveIntervalMs);
    set_basic_data(data, 900, 19.0f, 12.0f);
    history.update(data, getMillis());
    TEST_ASSERT_EQUAL_UINT32(2, storage.write_count);
    TEST_ASSERT_FALSE(history.lastWriteOk());

    storage.fail_write = false;
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs - 1);
    history.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(2, storage.write_count);
    TEST_ASSERT_FALSE(history.lastWriteOk());

    advanceMillis(1);
    history.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(3, storage.write_count);
    TEST_ASSERT_TRUE(history.lastWriteOk());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_daily_extrema_tracks_min_max_and_peak_times);
    RUN_TEST(test_daily_extrema_ignores_invalid_values);
    RUN_TEST(test_daily_extrema_exports_current_day_without_finalizing);
    RUN_TEST(test_daily_extrema_clear_current_day_resets_live_state);
    RUN_TEST(test_daily_extrema_clear_current_day_removes_persisted_state);
    RUN_TEST(test_daily_extrema_clear_current_day_preserves_ram_when_state_remove_fails);
    RUN_TEST(test_daily_extrema_flush_persists_dirty_state_before_interval);
    RUN_TEST(test_daily_extrema_restores_current_day_state);
    RUN_TEST(test_daily_extrema_exports_imperial_temperature_and_pressure);
    RUN_TEST(test_daily_extrema_unit_switch_applies_to_next_day_only);
    RUN_TEST(test_daily_extrema_restores_v2_units_from_state);
    RUN_TEST(test_daily_extrema_migrates_v1_state_as_metric);
    RUN_TEST(test_daily_extrema_resets_optional_gas_when_type_changes);
    RUN_TEST(test_daily_extrema_keeps_previous_day_when_csv_append_fails);
    RUN_TEST(test_daily_extrema_failed_state_save_retries_after_short_backoff);
    return UNITY_END();
}
