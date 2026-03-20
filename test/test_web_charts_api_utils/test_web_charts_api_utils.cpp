#include <unity.h>

#include <math.h>
#include <vector>

#include <ArduinoJson.h>

#include "config/AppConfig.h"
#include "web/WebChartsApiUtils.h"

namespace {

constexpr uint32_t kChartStepS = Config::CHART_HISTORY_STEP_MS / 1000UL;

struct FakeSample {
    bool valid[ChartsHistory::METRIC_COUNT]{};
    float values[ChartsHistory::METRIC_COUNT]{};
};

class FakeHistoryView : public WebChartsApiUtils::HistoryView {
public:
    std::vector<FakeSample> samples;
    uint32_t latest_epoch = 0;

    uint16_t count() const override {
        return static_cast<uint16_t>(samples.size());
    }

    uint32_t latestEpoch() const override {
        return latest_epoch;
    }

    bool latestMetric(ChartsHistory::Metric metric, float &out_value) const override {
        const size_t metric_index = static_cast<size_t>(metric);
        for (auto it = samples.rbegin(); it != samples.rend(); ++it) {
            if (it->valid[metric_index]) {
                out_value = it->values[metric_index];
                return true;
            }
        }
        return false;
    }

    bool metricValueFromOldest(uint16_t offset,
                               ChartsHistory::Metric metric,
                               float &value,
                               bool &valid) const override {
        if (offset >= samples.size()) {
            return false;
        }
        const FakeSample &sample = samples[offset];
        const size_t metric_index = static_cast<size_t>(metric);
        value = sample.values[metric_index];
        valid = sample.valid[metric_index];
        return true;
    }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_web_charts_api_utils_fill_json_populates_core_series_and_missing_prefix() {
    FakeHistoryView history;
    history.latest_epoch = Config::TIME_VALID_EPOCH + 1000U;

    FakeSample first{};
    first.valid[ChartsHistory::METRIC_CO2] = true;
    first.values[ChartsHistory::METRIC_CO2] = 500.0f;
    first.valid[ChartsHistory::METRIC_TEMPERATURE] = true;
    first.values[ChartsHistory::METRIC_TEMPERATURE] = 21.5f;

    FakeSample second{};
    second.valid[ChartsHistory::METRIC_CO2] = true;
    second.values[ChartsHistory::METRIC_CO2] = 510.0f;

    history.samples.push_back(first);
    history.samples.push_back(second);

    ArduinoJson::JsonDocument doc;
    WebChartsApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), history, "1h", "core");

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("core", doc["group"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("1h", doc["window"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT32(kChartStepS, doc["step_s"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(Config::CHART_HISTORY_1H_STEPS, doc["points"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, doc["available"].as<uint32_t>());

    ArduinoJson::JsonArrayConst timestamps = doc["timestamps"].as<ArduinoJson::JsonArrayConst>();
    TEST_ASSERT_EQUAL_UINT32(Config::CHART_HISTORY_1H_STEPS, timestamps.size());
    TEST_ASSERT_EQUAL_UINT32(history.latest_epoch - kChartStepS,
                             timestamps[Config::CHART_HISTORY_1H_STEPS - 2].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(history.latest_epoch,
                             timestamps[Config::CHART_HISTORY_1H_STEPS - 1].as<uint32_t>());

    ArduinoJson::JsonArrayConst series = doc["series"].as<ArduinoJson::JsonArrayConst>();
    TEST_ASSERT_EQUAL_UINT32(4, series.size());
    TEST_ASSERT_EQUAL_STRING("co2", series[0]["key"].as<const char *>());
    TEST_ASSERT_EQUAL_FLOAT(510.0f, series[0]["latest"].as<float>());
    TEST_ASSERT_TRUE(series[0]["values"][0].isNull());
    TEST_ASSERT_EQUAL_FLOAT(500.0f,
                            series[0]["values"][Config::CHART_HISTORY_1H_STEPS - 2].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(510.0f,
                            series[0]["values"][Config::CHART_HISTORY_1H_STEPS - 1].as<float>());

    TEST_ASSERT_EQUAL_STRING("temperature", series[1]["key"].as<const char *>());
    TEST_ASSERT_EQUAL_FLOAT(21.5f, series[1]["latest"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(21.5f,
                            series[1]["values"][Config::CHART_HISTORY_1H_STEPS - 2].as<float>());
    TEST_ASSERT_TRUE(series[1]["values"][Config::CHART_HISTORY_1H_STEPS - 1].isNull());
}

void test_web_charts_api_utils_fill_json_uses_null_timestamps_without_valid_epoch_and_null_latest_for_nan() {
    FakeHistoryView history;
    history.latest_epoch = 0;

    FakeSample sample{};
    sample.valid[ChartsHistory::METRIC_CO] = true;
    sample.values[ChartsHistory::METRIC_CO] = NAN;
    history.samples.push_back(sample);

    ArduinoJson::JsonDocument doc;
    WebChartsApiUtils::fillJson(doc.to<ArduinoJson::JsonObject>(), history, " 3H ", " gas ");

    TEST_ASSERT_EQUAL_STRING("gases", doc["group"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("3h", doc["window"].as<const char *>());

    ArduinoJson::JsonArrayConst timestamps = doc["timestamps"].as<ArduinoJson::JsonArrayConst>();
    TEST_ASSERT_EQUAL_UINT32(Config::CHART_HISTORY_3H_STEPS, timestamps.size());
    TEST_ASSERT_TRUE(timestamps[0].isNull());
    TEST_ASSERT_TRUE(timestamps[Config::CHART_HISTORY_3H_STEPS - 1].isNull());

    ArduinoJson::JsonArrayConst series = doc["series"].as<ArduinoJson::JsonArrayConst>();
    TEST_ASSERT_EQUAL_STRING("co", series[0]["key"].as<const char *>());
    TEST_ASSERT_TRUE(series[0]["latest"].isNull());
    TEST_ASSERT_TRUE(series[0]["values"][Config::CHART_HISTORY_3H_STEPS - 1].isNull());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_charts_api_utils_fill_json_populates_core_series_and_missing_prefix);
    RUN_TEST(test_web_charts_api_utils_fill_json_uses_null_timestamps_without_valid_epoch_and_null_latest_for_nan);
    return UNITY_END();
}
