#include <unity.h>

#include "config/AppConfig.h"
#include "web/WebChartsUtils.h"

void setUp() {}
void tearDown() {}

void test_web_charts_utils_chart_window_points_normalizes_known_windows() {
    const char *window_name = "";
    TEST_ASSERT_EQUAL_UINT16(Config::CHART_HISTORY_1H_STEPS,
                             WebChartsUtils::chartWindowPoints(" 1H ", window_name));
    TEST_ASSERT_EQUAL_STRING("1h", window_name);

    TEST_ASSERT_EQUAL_UINT16(Config::CHART_HISTORY_24H_SAMPLES,
                             WebChartsUtils::chartWindowPoints("24h", window_name));
    TEST_ASSERT_EQUAL_STRING("24h", window_name);
}

void test_web_charts_utils_chart_window_points_falls_back_to_3h() {
    const char *window_name = "";
    TEST_ASSERT_EQUAL_UINT16(Config::CHART_HISTORY_3H_STEPS,
                             WebChartsUtils::chartWindowPoints("unexpected", window_name));
    TEST_ASSERT_EQUAL_STRING("3h", window_name);
}

void test_web_charts_utils_chart_group_metrics_returns_expected_series() {
    const char *group_name = "";
    const WebChartsUtils::ChartMetricSpec *metrics = nullptr;
    size_t metric_count = 0;

    WebChartsUtils::chartGroupMetrics(" gas ", group_name, metrics, metric_count);
    TEST_ASSERT_EQUAL_STRING("gases", group_name);
    TEST_ASSERT_EQUAL_UINT32(4, static_cast<uint32_t>(metric_count));
    TEST_ASSERT_EQUAL_STRING("co", metrics[0].key);
    TEST_ASSERT_EQUAL_STRING("hcho", metrics[3].key);

    WebChartsUtils::chartGroupMetrics("pm", group_name, metrics, metric_count);
    TEST_ASSERT_EQUAL_STRING("pm", group_name);
    TEST_ASSERT_EQUAL_UINT32(5, static_cast<uint32_t>(metric_count));
    TEST_ASSERT_EQUAL_STRING("pm05", metrics[0].key);
    TEST_ASSERT_EQUAL_STRING("pm10", metrics[4].key);
}

void test_web_charts_utils_chart_group_metrics_falls_back_to_core() {
    const char *group_name = "";
    const WebChartsUtils::ChartMetricSpec *metrics = nullptr;
    size_t metric_count = 0;

    WebChartsUtils::chartGroupMetrics("", group_name, metrics, metric_count);
    TEST_ASSERT_EQUAL_STRING("core", group_name);
    TEST_ASSERT_EQUAL_UINT32(4, static_cast<uint32_t>(metric_count));
    TEST_ASSERT_EQUAL_STRING("co2", metrics[0].key);
    TEST_ASSERT_EQUAL_STRING("pressure", metrics[3].key);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_charts_utils_chart_window_points_normalizes_known_windows);
    RUN_TEST(test_web_charts_utils_chart_window_points_falls_back_to_3h);
    RUN_TEST(test_web_charts_utils_chart_group_metrics_returns_expected_series);
    RUN_TEST(test_web_charts_utils_chart_group_metrics_falls_back_to_core);
    return UNITY_END();
}
