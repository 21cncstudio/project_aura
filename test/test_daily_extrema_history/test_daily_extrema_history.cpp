#include <unity.h>

#include <map>
#include <sstream>
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

time_t local_epoch(int year, int month, int day, int hour, int minute) {
    tm local_tm{};
    local_tm.tm_year = year - 1900;
    local_tm.tm_mon = month - 1;
    local_tm.tm_mday = day;
    local_tm.tm_hour = hour;
    local_tm.tm_min = minute;
    local_tm.tm_isdst = -1;
    return mktime(&local_tm);
}

class FakeDailyStorage final : public DailyHistoryStorage {
public:
    bool ready = true;
    bool fail_append = false;
    bool fail_prune = false;
    bool fail_prune_sidecar_cleanup = false;
    bool fail_write = false;
    uint32_t fail_write_on_count = 0;
    bool fail_remove = false;
    std::string fail_info_path;
    std::string fail_read_path;
    DailyStorageFailureKind failure_kind = DailyStorageFailureKind::None;
    uint32_t write_count = 0;
    uint32_t prune_count = 0;
    std::map<std::string, std::string> text_files;
    std::map<std::string, std::vector<uint8_t>> binary_files;

    bool isReady() const override { return ready; }

    bool fileInfo(const char *path, bool &exists, size_t &out_size) const override {
        exists = false;
        out_size = 0;
        if (!ready || !path || fail_info_path == path) {
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
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        text_files[path] += text;
        failure_kind = DailyStorageFailureKind::None;
        return true;
    }

    bool appendUniqueTextBlockAtomic(const char *path,
                                     const char *unique_line_prefix,
                                     const char *header,
                                     const char *block) override {
        if (!ready || fail_append || !path || !unique_line_prefix || !block) {
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        std::string rewritten;
        std::istringstream source(text_files[path]);
        std::string line;
        while (std::getline(source, line)) {
            if (line.rfind(unique_line_prefix, 0) == 0) {
                continue;
            }
            rewritten += line;
            rewritten += '\n';
        }
        if (rewritten.empty() && header) {
            rewritten = header;
        }
        if (!rewritten.empty() && rewritten.back() != '\n') {
            rewritten += '\n';
        }
        rewritten += block;
        text_files[path] = rewritten;
        failure_kind = DailyStorageFailureKind::None;
        return true;
    }

    bool removeDailyCsvRowsOnOrAfterAtomic(const char *path,
                                            const char *cutoff_iso_day) override {
        ++prune_count;
        if (!ready || fail_prune || !path || !cutoff_iso_day ||
            strlen(cutoff_iso_day) != 10U) {
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        auto it = text_files.find(path);
        if (it != text_files.end()) {
            std::string rewritten;
            std::istringstream source(it->second);
            std::string line;
            while (std::getline(source, line)) {
                const bool dated_row =
                    line.size() >= 11U && line[4] == '-' && line[7] == '-' &&
                    line[10] == ',';
                if (dated_row && line.compare(0, 10, cutoff_iso_day) >= 0) {
                    continue;
                }
                rewritten += line;
                rewritten += '\n';
            }
            it->second = rewritten;
        }
        if (fail_prune_sidecar_cleanup) {
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        text_files.erase(std::string(path) + ".tmp");
        text_files.erase(std::string(path) + ".bak");
        failure_kind = DailyStorageFailureKind::None;
        return true;
    }

    DailyStorageFailureKind lastFailureKind() const override { return failure_kind; }

    bool readBinary(const char *path, void *out, size_t len, size_t &out_len) const override {
        out_len = 0;
        if (!ready || !path || !out || fail_read_path == path) {
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
        if (fail_write ||
            (fail_write_on_count != 0 && write_count == fail_write_on_count)) {
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        if (!ready || !path || !data) {
            failure_kind = DailyStorageFailureKind::Invalid;
            return false;
        }
        const auto *bytes = static_cast<const uint8_t *>(data);
        binary_files[path] = std::vector<uint8_t>(bytes, bytes + len);
        failure_kind = DailyStorageFailureKind::None;
        return true;
    }

    bool removeFile(const char *path) override {
        if (!path || fail_remove) {
            failure_kind = DailyStorageFailureKind::Io;
            return false;
        }
        const std::string final_path(path);
        const std::string backup_path = final_path + ".bak";
        const std::string tmp_path = final_path + ".tmp";
        text_files.erase(tmp_path);
        binary_files.erase(tmp_path);
        text_files.erase(backup_path);
        binary_files.erase(backup_path);
        text_files.erase(final_path);
        binary_files.erase(final_path);
        failure_kind = DailyStorageFailureKind::None;
        return true;
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
    constexpr size_t kSnapshotV2Size = 2848U;
    constexpr size_t kSnapshotV2CurrentOffset = 28U;
    constexpr size_t kPersistedStateSize = 352U;
    const char *snapshot_path =
        storage.binary_files.count(DailyExtremaHistory::kStatePathB)
            ? DailyExtremaHistory::kStatePathB
            : DailyExtremaHistory::kStatePathA;
    const auto &snapshot = storage.binary_files[snapshot_path];
    TEST_ASSERT_EQUAL_UINT32(kSnapshotV2Size,
                             static_cast<uint32_t>(snapshot.size()));
    std::vector<uint8_t> legacy(
        snapshot.begin() + kSnapshotV2CurrentOffset,
        snapshot.begin() + kSnapshotV2CurrentOffset + kPersistedStateSize);
    legacy[4] = 1;
    legacy[5] = 0;
    legacy[13] = 0;
    storage.binary_files.clear();
    storage.binary_files[DailyExtremaHistory::kLegacyStatePath] = legacy;
}

bool state_snapshot_exists(const FakeDailyStorage &storage) {
    return storage.binary_files.count(DailyExtremaHistory::kStatePathA) > 0 ||
           storage.binary_files.count(DailyExtremaHistory::kStatePathB) > 0;
}

size_t count_occurrences(const std::string &text, const std::string &needle) {
    size_t count = 0;
    size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

uint32_t test_crc32(const uint8_t *bytes, size_t length) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

void append_u32_le(std::vector<uint8_t> &bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8U));
    bytes.push_back(static_cast<uint8_t>(value >> 16U));
    bytes.push_back(static_cast<uint8_t>(value >> 24U));
}

void downgrade_snapshot_to_v1(std::vector<uint8_t> &snapshot) {
    constexpr size_t kSnapshotV1Size = 2840U;
    constexpr size_t kSnapshotV2Size = 2848U;
    constexpr size_t kSnapshotV1HeaderSize = 20U;
    constexpr size_t kSnapshotV2StateOffset = 28U;
    constexpr size_t kSnapshotV2CrcOffset = 2844U;

    TEST_ASSERT_EQUAL_UINT32(kSnapshotV2Size,
                             static_cast<uint32_t>(snapshot.size()));
    std::vector<uint8_t> v1;
    v1.reserve(kSnapshotV1Size);
    v1.insert(v1.end(), snapshot.begin(), snapshot.begin() + kSnapshotV1HeaderSize);
    v1.insert(v1.end(),
              snapshot.begin() + kSnapshotV2StateOffset,
              snapshot.begin() + kSnapshotV2CrcOffset);
    TEST_ASSERT_EQUAL_UINT32(kSnapshotV1Size - sizeof(uint32_t),
                             static_cast<uint32_t>(v1.size()));
    v1[4] = 1U;
    v1[5] = 0U;
    v1[6] = static_cast<uint8_t>(kSnapshotV1Size);
    v1[7] = static_cast<uint8_t>(kSnapshotV1Size >> 8U);
    append_u32_le(v1, test_crc32(v1.data(), v1.size()));
    snapshot = v1;
}

void downgrade_snapshots_to_v1(FakeDailyStorage &storage) {
    for (auto &entry : storage.binary_files) {
        if (entry.first != DailyExtremaHistory::kStatePathA &&
            entry.first != DailyExtremaHistory::kStatePathB) {
            continue;
        }
        downgrade_snapshot_to_v1(entry.second);
    }
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

    TEST_ASSERT_TRUE(state_snapshot_exists(storage));

    const DailyExtremaHistory::ClearCurrentDayResult result = history.clearCurrentDay(true);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(result.state_existed);
    TEST_ASSERT_FALSE(state_snapshot_exists(storage));
    TEST_ASSERT_FALSE(storage.fileExists(DailyExtremaHistory::kLegacyStatePath));
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
    TEST_ASSERT_TRUE(state_snapshot_exists(storage));
    TEST_ASSERT_TRUE(history.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(3, history.currentSampleCount());
}

void test_daily_extrema_clear_current_day_purges_atomic_file_families() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 650, 21.0f, 8.5f);
    history.update(data, getMillis());
    TEST_ASSERT_TRUE(
        storage.binary_files.count(DailyExtremaHistory::kStatePathA) > 0U);

    const std::string state_a(DailyExtremaHistory::kStatePathA);
    const std::string state_b(DailyExtremaHistory::kStatePathB);
    const std::string legacy(DailyExtremaHistory::kLegacyStatePath);
    storage.binary_files[state_a + ".bak"] = {1U};
    storage.binary_files[state_a + ".tmp"] = {2U};
    storage.binary_files[state_b + ".bak"] = {3U};
    storage.binary_files[legacy + ".tmp"] = {4U};

    DailyExtremaHistory::ClearCurrentDayResult result = history.clearCurrentDay(true);
    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(result.state_existed);
    for (const std::string &path : {state_a, state_b, legacy}) {
        TEST_ASSERT_EQUAL_UINT32(0U, storage.binary_files.count(path));
        TEST_ASSERT_EQUAL_UINT32(0U, storage.binary_files.count(path + ".bak"));
        TEST_ASSERT_EQUAL_UINT32(0U, storage.binary_files.count(path + ".tmp"));
    }

    result = history.clearCurrentDay(true);
    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_FALSE(result.state_existed);
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

void test_daily_extrema_defers_restore_and_rollover_until_time_is_trusted() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData day_one_data;
        set_basic_data(day_one_data, 800, 22.0f, 12.0f);
        writer.update(day_one_data, getMillis(), true);
        writer.flush();
    }
    const uint32_t writes_before_reboot = storage.write_count;
    TEST_ASSERT_TRUE(state_snapshot_exists(storage));
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    setNowEpoch(kDayTwoNoon);
    SensorData day_two_data;
    set_basic_data(day_two_data, 900, 23.0f, 13.0f);

    restored.update(day_two_data, getMillis(), false);
    TEST_ASSERT_FALSE(restored.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT8(0, restored.pendingDayCount());
    TEST_ASSERT_EQUAL_UINT32(writes_before_reboot, storage.write_count);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    restored.update(day_two_data, getMillis(), true);
    TEST_ASSERT_TRUE(restored.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(20260628U, restored.currentDayKey());
    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,800") != std::string::npos);
}

void test_daily_extrema_runtime_small_backward_date_holds_until_catchup() {
    const time_t day_one_sample = local_epoch(2026, 6, 27, 23, 57);
    const time_t day_one_corrected = local_epoch(2026, 6, 27, 23, 58);
    const time_t day_two_early = local_epoch(2026, 6, 28, 0, 1);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_one_sample);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_one_corrected);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_two_early);

    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    setNowEpoch(day_one_sample);
    set_basic_data(data, 800, 22.0f, 12.0f);
    history.update(data, getMillis(), true);
    storage.fail_append = true;

    setNowEpoch(day_two_early);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(1, history.pendingDayCount());
    const uint32_t samples_before_hold = history.currentSampleCount();
    const uint32_t writes_before_hold = storage.write_count;

    storage.fail_append = false;
    setNowEpoch(day_one_corrected);
    set_basic_data(data, 3000, 40.0f, 99.0f);
    history.update(data, getMillis(), true);
    history.poll(getMillis());
    history.flush();

    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT32(samples_before_hold, history.currentSampleCount());
    TEST_ASSERT_EQUAL_UINT8(1, history.pendingDayCount());
    TEST_ASSERT_EQUAL_UINT32(writes_before_hold, storage.write_count);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    setNowEpoch(day_two_early);
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs + 1U);
    history.update(data, getMillis(), true);
    TEST_ASSERT_TRUE(history.currentSampleCount() > samples_before_hold);
    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(0, history.pendingDayCount());
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-27,co2,ppm,800") != std::string::npos);
}

void test_daily_extrema_restored_far_future_date_is_replaced() {
    const time_t day_one_late = local_epoch(2026, 6, 27, 23, 58);
    const time_t day_two_evening = local_epoch(2026, 6, 28, 20, 0);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_one_late);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_two_evening);

    FakeDailyStorage storage;
    setNowEpoch(day_two_evening);
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 900, 23.0f, 13.0f);
        writer.update(data, getMillis(), true);
        writer.flush();
    }

    const uint32_t writes_before_reboot = storage.write_count;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    setNowEpoch(day_one_late);
    SensorData data;
    set_basic_data(data, 3000, 40.0f, 99.0f);
    restored.update(data, getMillis(), true);

    TEST_ASSERT_TRUE(restored.hasCurrentDay());
    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_TRUE(restored.currentSampleCount() > 0);
    TEST_ASSERT_EQUAL_UINT8(0, restored.pendingDayCount());
    TEST_ASSERT_TRUE(storage.write_count > writes_before_reboot);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    DailyExtremaHistory verified;
    verified.begin(storage, true);
    verified.update(SensorData{}, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, verified.currentDayKey());
}

void test_daily_extrema_small_backward_hold_survives_reboot() {
    const time_t day_one_sample = local_epoch(2026, 6, 27, 23, 57);
    const time_t day_one_corrected = local_epoch(2026, 6, 27, 23, 58);
    const time_t day_two_early = local_epoch(2026, 6, 28, 0, 1);

    FakeDailyStorage storage;
    storage.fail_append = true;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        setNowEpoch(day_one_sample);
        set_basic_data(data, 800, 22.0f, 12.0f);
        writer.update(data, getMillis(), true);
        setNowEpoch(day_two_early);
        set_basic_data(data, 900, 23.0f, 13.0f);
        writer.update(data, getMillis(), true);
        TEST_ASSERT_EQUAL_UINT8(1, writer.pendingDayCount());
        writer.flush();
    }

    storage.fail_append = false;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    setNowEpoch(day_one_corrected);
    SensorData data;
    set_basic_data(data, 3000, 40.0f, 99.0f);
    restored.update(data, getMillis(), true);
    restored.flush();
    TEST_ASSERT_EQUAL_UINT32(20260628U, restored.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(1, restored.pendingDayCount());
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    DailyExtremaHistory restored_again;
    restored_again.begin(storage, true);
    restored_again.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260628U, restored_again.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(1, restored_again.pendingDayCount());

    setNowEpoch(day_two_early);
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs + 1U);
    restored_again.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT8(0, restored_again.pendingDayCount());
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-27,co2,ppm,800") != std::string::npos);
}

void test_daily_extrema_v1_snapshot_does_not_invent_runtime_anchor() {
    const time_t day_one_late = local_epoch(2026, 6, 27, 23, 58);
    const time_t day_two_early = local_epoch(2026, 6, 28, 0, 1);
    FakeDailyStorage storage;
    setNowEpoch(day_two_early);
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 900, 23.0f, 13.0f);
        writer.update(data, getMillis(), true);
        writer.flush();
    }
    downgrade_snapshots_to_v1(storage);

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    setNowEpoch(day_one_late);
    SensorData data;
    set_basic_data(data, 700, 21.0f, 11.0f);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(0, restored.pendingDayCount());

    bool wrote_v2 = false;
    for (const auto &entry : storage.binary_files) {
        wrote_v2 = wrote_v2 || entry.second.size() == 2848U;
    }
    TEST_ASSERT_TRUE(wrote_v2);
}

void test_daily_extrema_v1_read_failure_defers_restore_without_overwrite() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 800, 21.0f, 7.0f);
        writer.update(data, getMillis(), true);
        advanceEpoch(60);
        advanceMillis(DailyExtremaHistory::kStateSaveIntervalMs);
        set_basic_data(data, 1200, 23.0f, 12.0f);
        writer.update(data, getMillis(), true);
    }
    auto &newer = storage.binary_files[DailyExtremaHistory::kStatePathB];
    TEST_ASSERT_FALSE(newer.empty());
    downgrade_snapshot_to_v1(newer);
    const auto files_before_failure = storage.binary_files;
    const uint32_t writes_before_failure = storage.write_count;
    storage.fail_read_path = DailyExtremaHistory::kStatePathB;

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData data;
    set_basic_data(data, 500, 20.0f, 5.0f);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_FALSE(restored.hasCurrentDay());
    TEST_ASSERT_FALSE(restored.lastWriteOk());
    TEST_ASSERT_EQUAL_UINT32(writes_before_failure, storage.write_count);
    TEST_ASSERT_TRUE(storage.binary_files == files_before_failure);

    storage.fail_read_path.clear();
    restored.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_EQUAL_UINT32(9U, restored.currentSampleCount());
}

void test_daily_extrema_invalid_v1_snapshot_falls_back_to_valid_peer() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 800, 21.0f, 7.0f);
        writer.update(data, getMillis(), true);
        advanceEpoch(60);
        advanceMillis(DailyExtremaHistory::kStateSaveIntervalMs);
        set_basic_data(data, 1200, 23.0f, 12.0f);
        writer.update(data, getMillis(), true);
    }
    auto &newer = storage.binary_files[DailyExtremaHistory::kStatePathB];
    TEST_ASSERT_FALSE(newer.empty());
    downgrade_snapshot_to_v1(newer);
    newer.back() ^= 0x5AU;

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData data;
    set_basic_data(data, 500, 20.0f, 5.0f);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_EQUAL_UINT32(6U, restored.currentSampleCount());
}

void test_daily_extrema_legacy_read_failure_defers_restore_without_overwrite() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 800, 21.0f, 7.0f);
        writer.update(data, getMillis(), true);
    }
    downgrade_current_state_to_v1_metric(storage);
    const auto files_before_failure = storage.binary_files;
    const uint32_t writes_before_failure = storage.write_count;
    storage.fail_read_path = DailyExtremaHistory::kLegacyStatePath;

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData data;
    set_basic_data(data, 500, 20.0f, 5.0f);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_FALSE(restored.hasCurrentDay());
    TEST_ASSERT_FALSE(restored.lastWriteOk());
    TEST_ASSERT_EQUAL_UINT32(writes_before_failure, storage.write_count);
    TEST_ASSERT_TRUE(storage.binary_files == files_before_failure);

    storage.fail_read_path.clear();
    restored.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_EQUAL_UINT32(6U, restored.currentSampleCount());
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        storage.binary_files.count(DailyExtremaHistory::kLegacyStatePath));
}

void test_daily_extrema_runtime_backward_hold_escalates_to_replacement() {
    const time_t day_one_near = local_epoch(2026, 6, 27, 23, 58);
    const time_t day_one_far = local_epoch(2026, 6, 27, 23, 50);
    const time_t day_two_early = local_epoch(2026, 6, 28, 0, 1);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_one_near);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_one_far);
    TEST_ASSERT_NOT_EQUAL(static_cast<time_t>(-1), day_two_early);

    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);
    SensorData data;

    setNowEpoch(day_two_early);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);
    const uint32_t samples_before_hold = history.currentSampleCount();

    setNowEpoch(day_one_near);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT32(samples_before_hold, history.currentSampleCount());

    setNowEpoch(day_one_far);
    set_basic_data(data, 700, 21.0f, 11.0f);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, history.currentDayKey());
    TEST_ASSERT_TRUE(history.currentSampleCount() > 0);
    TEST_ASSERT_EQUAL_UINT8(0, history.pendingDayCount());
}

void test_daily_extrema_runtime_large_backward_date_starts_new_generation() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    setNowEpoch(kDayTwoNoon);
    SensorData data;
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());

    setNowEpoch(kDayOneNoon);
    set_basic_data(data, 700, 21.0f, 11.0f);
    history.update(data, getMillis(), true);

    TEST_ASSERT_EQUAL_UINT32(20260627U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(0, history.pendingDayCount());
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));
}

void test_daily_extrema_large_future_snapshot_is_replaced_atomically() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        setNowEpoch(kDayOneNoon);
        set_basic_data(data, 700, 21.0f, 11.0f);
        writer.update(data, getMillis(), true);
        storage.fail_append = true;
        setNowEpoch(kDayTwoNoon);
        set_basic_data(data, 900, 23.0f, 13.0f);
        writer.update(data, getMillis(), true);
        TEST_ASSERT_EQUAL_UINT8(1, writer.pendingDayCount());
        writer.flush();
    }

    const auto snapshots_before = storage.binary_files;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    setNowEpoch(kDayOneNoon);
    SensorData data;
    set_basic_data(data, 700, 21.0f, 11.0f);
    storage.fail_append = false;
    storage.fail_write = true;
    restored.update(data, getMillis(), true);

    TEST_ASSERT_EQUAL_UINT32(20260627U, restored.currentDayKey());
    TEST_ASSERT_TRUE(restored.currentSampleCount() > 0);
    TEST_ASSERT_EQUAL_UINT8(0, restored.pendingDayCount());
    TEST_ASSERT_TRUE(storage.binary_files == snapshots_before);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        static_cast<uint32_t>(
            storage.text_files.count(DailyExtremaHistory::kDailyCsvPath)));

    storage.fail_write = false;
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs + 1U);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_FALSE(storage.binary_files == snapshots_before);

    DailyExtremaHistory verified;
    verified.begin(storage, true);
    verified.update(SensorData{}, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(20260627U, verified.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(0, verified.pendingDayCount());
}

void test_daily_extrema_csv_prune_waits_for_durable_intent() {
    FakeDailyStorage storage;
    storage.text_files[DailyExtremaHistory::kDailyCsvPath] =
        "date,metric,unit,min,min_time,max,max_time,sample_count\n"
        "2026-06-26,co2,ppm,500,00:00:00,600,23:00:00,2\n"
        "2026-06-27,co2,ppm,700,00:00:00,800,23:00:00,2\n"
        "2026-06-28,co2,ppm,900,00:00:00,1000,23:00:00,2\n";
    const std::string csv_before =
        storage.text_files[DailyExtremaHistory::kDailyCsvPath];

    DailyExtremaHistory history;
    history.begin(storage, true);
    SensorData data;
    setNowEpoch(kDayTwoNoon);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);

    storage.fail_write = true;
    setNowEpoch(kDayOneNoon);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(0, storage.prune_count);
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath] == csv_before);

    storage.fail_write = false;
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs + 1U);
    history.poll(getMillis());
    TEST_ASSERT_EQUAL_UINT32(1, storage.prune_count);
    const std::string &csv =
        storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-26,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,") == std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-28,") == std::string::npos);
}

void test_daily_extrema_csv_prune_retries_while_samples_are_dirty() {
    FakeDailyStorage storage;
    storage.text_files[DailyExtremaHistory::kDailyCsvPath] =
        "date,metric,unit,min,min_time,max,max_time,sample_count\n"
        "2026-06-26,co2,ppm,500,00:00:00,600,23:00:00,2\n"
        "2026-06-27,co2,ppm,700,00:00:00,800,23:00:00,2\n";

    DailyExtremaHistory history;
    history.begin(storage, true);
    SensorData data;
    setNowEpoch(kDayTwoNoon);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);

    storage.fail_prune = true;
    setNowEpoch(kDayOneNoon);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(1, storage.prune_count);
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-27,") != std::string::npos);

    set_basic_data(data, 650, 20.0f, 10.0f);
    history.update(data, getMillis(), true);
    storage.fail_prune = false;
    advanceMillis(DailyExtremaHistory::kStateSaveRetryIntervalMs + 1U);
    history.poll(getMillis());

    TEST_ASSERT_EQUAL_UINT32(2, storage.prune_count);
    const std::string &csv =
        storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-26,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("2026-06-27,") == std::string::npos);
}

void test_daily_extrema_csv_prune_keeps_intent_until_sidecars_are_gone() {
    FakeDailyStorage storage;
    const std::string csv =
        "date,metric,unit,min,min_time,max,max_time,sample_count\n"
        "2026-06-26,co2,ppm,500,00:00:00,600,23:00:00,2\n"
        "2026-06-28,co2,ppm,900,00:00:00,1000,23:00:00,2\n";
    storage.text_files[DailyExtremaHistory::kDailyCsvPath] = csv;
    storage.text_files[
        std::string(DailyExtremaHistory::kDailyCsvPath) + ".bak"] = csv;

    DailyExtremaHistory history;
    history.begin(storage, true);
    SensorData data;
    setNowEpoch(kDayTwoNoon);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);

    storage.fail_prune_sidecar_cleanup = true;
    setNowEpoch(kDayOneNoon);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(1U, storage.prune_count);
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-28,") == std::string::npos);
    TEST_ASSERT_TRUE(
        storage.text_files[
            std::string(DailyExtremaHistory::kDailyCsvPath) + ".bak"]
            .find("2026-06-28,") != std::string::npos);

    storage.fail_prune_sidecar_cleanup = false;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    restored.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(2U, storage.prune_count);
    TEST_ASSERT_EQUAL_UINT32(
        0U,
        storage.text_files.count(
            std::string(DailyExtremaHistory::kDailyCsvPath) + ".bak"));

    DailyExtremaHistory verified;
    verified.begin(storage, true);
    verified.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(2U, storage.prune_count);
}

void test_daily_extrema_csv_prune_clear_is_durable_before_future_appends() {
    FakeDailyStorage storage;
    storage.text_files[DailyExtremaHistory::kDailyCsvPath] =
        "date,metric,unit,min,min_time,max,max_time,sample_count\n"
        "2026-06-26,co2,ppm,500,00:00:00,600,23:00:00,2\n"
        "2026-06-28,co2,ppm,900,00:00:00,1000,23:00:00,2\n";

    DailyExtremaHistory history;
    history.begin(storage, true);
    SensorData data;
    setNowEpoch(kDayTwoNoon);
    set_basic_data(data, 900, 23.0f, 13.0f);
    history.update(data, getMillis(), true);

    storage.fail_write_on_count = storage.write_count + 2U;
    setNowEpoch(kDayOneNoon);
    set_basic_data(data, 700, 21.0f, 11.0f);
    history.update(data, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(1, storage.prune_count);
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-28,") == std::string::npos);

    storage.text_files[DailyExtremaHistory::kDailyCsvPath] +=
        "2026-06-28,co2,ppm,1,00:00:00,2,00:01:00,2\n";
    storage.fail_write_on_count = 0;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    restored.update(SensorData{}, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(2, storage.prune_count);
    TEST_ASSERT_TRUE(
        storage.text_files[DailyExtremaHistory::kDailyCsvPath].find(
            "2026-06-28,") == std::string::npos);

    DailyExtremaHistory verified;
    verified.begin(storage, true);
    verified.update(SensorData{}, getMillis(), true);
    TEST_ASSERT_EQUAL_UINT32(2, storage.prune_count);
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

void test_daily_extrema_queues_previous_day_when_csv_append_fails() {
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

    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(1, history.pendingDayCount());
    TEST_ASSERT_EQUAL_UINT32(20260627U, history.oldestPendingDayKey());
    TEST_ASSERT_FALSE(history.lastWriteOk());
    TEST_ASSERT_EQUAL_UINT32(0, storage.text_files.count(DailyExtremaHistory::kDailyCsvPath));

    String current_csv;
    TEST_ASSERT_TRUE(history.currentDayCsv(current_csv));
    const std::string current_day_csv = current_csv.c_str();
    TEST_ASSERT_TRUE(current_day_csv.find("2026-06-28,co2,ppm,900") != std::string::npos);
    TEST_ASSERT_TRUE(current_day_csv.find("2026-06-27") == std::string::npos);

    storage.fail_append = false;
    advanceMillis(5000);
    history.update(data, getMillis());

    TEST_ASSERT_EQUAL_UINT32(20260628U, history.currentDayKey());
    TEST_ASSERT_EQUAL_UINT8(0, history.pendingDayCount());
    TEST_ASSERT_TRUE(history.lastWriteOk());
    const std::string daily_csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(daily_csv.find("2026-06-27,co2,ppm,700") != std::string::npos);
}

void test_daily_extrema_exports_o2_as_percent_volume() {
    FakeDailyStorage storage;
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    data.optional_gas_sensor_present = true;
    data.optional_gas_valid = true;
    data.optional_gas_type = static_cast<uint8_t>(DfrOptionalGasSensor::OptionalGasType::O2);
    data.optional_gas_ppm = 20.9f;
    history.update(data, getMillis());

    setNowEpoch(kDayTwoNoon);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,optional_gas,%Vol,20.9") != std::string::npos);
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

void test_daily_extrema_pending_day_survives_reboot_and_retries() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 700, 21.0f, 7.0f);
        writer.update(data, getMillis());

        storage.fail_append = true;
        setNowEpoch(kDayTwoNoon);
        advanceMillis(24UL * 60UL * 60UL * 1000UL);
        set_basic_data(data, 900, 23.0f, 12.0f);
        writer.update(data, getMillis());
        TEST_ASSERT_EQUAL_UINT8(1, writer.pendingDayCount());
    }

    storage.fail_append = false;
    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData data;
    set_basic_data(data, 950, 24.0f, 13.0f);
    restored.update(data, getMillis());

    TEST_ASSERT_EQUAL_UINT8(0, restored.pendingDayCount());
    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,700") != std::string::npos);
}

void test_daily_extrema_replaces_partial_rows_for_same_day() {
    FakeDailyStorage storage;
    storage.text_files[DailyExtremaHistory::kDailyCsvPath] =
        "date,metric,unit,min,min_time,max,max_time,sample_count\n"
        "2026-06-27,partial,row\n";
    DailyExtremaHistory history;
    history.begin(storage, true);

    SensorData data;
    set_basic_data(data, 700, 21.0f, 7.0f);
    history.update(data, getMillis());
    setNowEpoch(kDayTwoNoon);
    history.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("partial,row") == std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(3, count_occurrences(csv, "2026-06-27,"));
}

void test_daily_extrema_uses_older_snapshot_when_newest_is_corrupt() {
    FakeDailyStorage storage;
    {
        DailyExtremaHistory writer;
        writer.begin(storage, true);
        SensorData data;
        set_basic_data(data, 800, 21.0f, 7.0f);
        writer.update(data, getMillis());

        advanceEpoch(60);
        advanceMillis(DailyExtremaHistory::kStateSaveIntervalMs);
        set_basic_data(data, 1200, 23.0f, 12.0f);
        writer.update(data, getMillis());
    }
    auto &newest = storage.binary_files[DailyExtremaHistory::kStatePathB];
    TEST_ASSERT_FALSE(newest.empty());
    newest.back() ^= 0x5A;

    DailyExtremaHistory restored;
    restored.begin(storage, true);
    SensorData data;
    set_basic_data(data, 500, 20.0f, 5.0f);
    restored.update(data, getMillis());
    setNowEpoch(kDayTwoNoon);
    restored.update(data, getMillis());

    const std::string csv = storage.text_files[DailyExtremaHistory::kDailyCsvPath];
    TEST_ASSERT_TRUE(csv.find("2026-06-27,co2,ppm,500") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find(",800,") != std::string::npos);
    TEST_ASSERT_TRUE(csv.find("1200") == std::string::npos);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_daily_extrema_tracks_min_max_and_peak_times);
    RUN_TEST(test_daily_extrema_ignores_invalid_values);
    RUN_TEST(test_daily_extrema_exports_current_day_without_finalizing);
    RUN_TEST(test_daily_extrema_clear_current_day_resets_live_state);
    RUN_TEST(test_daily_extrema_clear_current_day_removes_persisted_state);
    RUN_TEST(test_daily_extrema_clear_current_day_preserves_ram_when_state_remove_fails);
    RUN_TEST(test_daily_extrema_clear_current_day_purges_atomic_file_families);
    RUN_TEST(test_daily_extrema_flush_persists_dirty_state_before_interval);
    RUN_TEST(test_daily_extrema_restores_current_day_state);
    RUN_TEST(test_daily_extrema_defers_restore_and_rollover_until_time_is_trusted);
    RUN_TEST(test_daily_extrema_runtime_small_backward_date_holds_until_catchup);
    RUN_TEST(test_daily_extrema_restored_far_future_date_is_replaced);
    RUN_TEST(test_daily_extrema_small_backward_hold_survives_reboot);
    RUN_TEST(test_daily_extrema_v1_snapshot_does_not_invent_runtime_anchor);
    RUN_TEST(test_daily_extrema_v1_read_failure_defers_restore_without_overwrite);
    RUN_TEST(test_daily_extrema_invalid_v1_snapshot_falls_back_to_valid_peer);
    RUN_TEST(test_daily_extrema_legacy_read_failure_defers_restore_without_overwrite);
    RUN_TEST(test_daily_extrema_runtime_backward_hold_escalates_to_replacement);
    RUN_TEST(test_daily_extrema_runtime_large_backward_date_starts_new_generation);
    RUN_TEST(test_daily_extrema_large_future_snapshot_is_replaced_atomically);
    RUN_TEST(test_daily_extrema_csv_prune_waits_for_durable_intent);
    RUN_TEST(test_daily_extrema_csv_prune_retries_while_samples_are_dirty);
    RUN_TEST(test_daily_extrema_csv_prune_keeps_intent_until_sidecars_are_gone);
    RUN_TEST(test_daily_extrema_csv_prune_clear_is_durable_before_future_appends);
    RUN_TEST(test_daily_extrema_exports_imperial_temperature_and_pressure);
    RUN_TEST(test_daily_extrema_unit_switch_applies_to_next_day_only);
    RUN_TEST(test_daily_extrema_restores_v2_units_from_state);
    RUN_TEST(test_daily_extrema_migrates_v1_state_as_metric);
    RUN_TEST(test_daily_extrema_resets_optional_gas_when_type_changes);
    RUN_TEST(test_daily_extrema_exports_o2_as_percent_volume);
    RUN_TEST(test_daily_extrema_queues_previous_day_when_csv_append_fails);
    RUN_TEST(test_daily_extrema_failed_state_save_retries_after_short_backoff);
    RUN_TEST(test_daily_extrema_pending_day_survives_reboot_and_retries);
    RUN_TEST(test_daily_extrema_replaces_partial_rows_for_same_day);
    RUN_TEST(test_daily_extrema_uses_older_snapshot_when_newest_is_corrupt);
    return UNITY_END();
}
