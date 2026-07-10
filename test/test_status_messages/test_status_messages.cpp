#include <unity.h>

#include "modules/DisplayThresholds.h"
#include "ui/StatusMessages.h"

void setUp() {}
void tearDown() {}

void test_status_messages_temperature_uses_display_thresholds() {
    SensorData data{};
    data.temp_valid = true;
    data.temperature = 19.0f;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_TEMP, result.messages[0].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_YELLOW, result.messages[0].severity);

    thresholds.temp = {14.0f, 15.0f, 16.0f, 30.0f, 31.0f, 32.0f};
    result = StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

void test_status_messages_co_safety_override_uses_display_thresholds() {
    SensorData data{};
    data.co_sensor_present = true;
    data.co_valid = true;
    data.co_ppm = 2.5f;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    thresholds.co = {1.0f, 2.0f, 3.0f};

    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_CO, result.messages[0].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_ORANGE, result.messages[0].severity);

    thresholds.co = {3.0f, 4.0f, 5.0f};
    result = StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

void test_status_messages_co2_uses_display_thresholds() {
    SensorData data{};
    data.co2_valid = true;
    data.co2 = 750;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    thresholds.co2 = {700.0f, 900.0f, 1100.0f};

    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_CO2, result.messages[0].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_YELLOW, result.messages[0].severity);

    thresholds.co2 = {800.0f, 900.0f, 1100.0f};
    result = StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

void test_status_messages_voc_and_nox_use_display_thresholds() {
    SensorData data{};
    data.voc_valid = true;
    data.voc_index = 120;
    data.nox_valid = true;
    data.nox_index = 40;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    thresholds.voc = {100.0f, 150.0f, 200.0f};
    thresholds.nox = {30.0f, 60.0f, 90.0f};

    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(2, result.count);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_NOX, result.messages[0].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_YELLOW, result.messages[0].severity);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_VOC, result.messages[1].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_YELLOW, result.messages[1].severity);

    thresholds.voc = {120.0f, 150.0f, 200.0f};
    thresholds.nox = {40.0f, 60.0f, 90.0f};
    result = StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

void test_status_messages_humidity_uses_display_thresholds() {
    SensorData data{};
    data.hum_valid = true;
    data.humidity = 25.0f;

    DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(1, result.count);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_SENSOR_HUM, result.messages[0].sensor);
    TEST_ASSERT_EQUAL_UINT8(StatusMessages::STATUS_ORANGE, result.messages[0].severity);

    thresholds.rh = {10.0f, 15.0f, 20.0f, 80.0f, 85.0f, 90.0f};
    result = StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

void test_status_messages_dew_point_defaults_match_display_thresholds() {
    SensorData data{};
    data.temp_valid = true;
    data.hum_valid = true;
    data.temperature = 20.0f;
    data.humidity = 55.0f;

    const DisplayThresholds::Config thresholds = DisplayThresholds::defaults();
    StatusMessages::StatusMessageResult result =
        StatusMessages::build_status_messages(data, false, thresholds);
    TEST_ASSERT_TRUE(result.has_valid);
    TEST_ASSERT_EQUAL_UINT(0, result.count);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_status_messages_temperature_uses_display_thresholds);
    RUN_TEST(test_status_messages_co_safety_override_uses_display_thresholds);
    RUN_TEST(test_status_messages_co2_uses_display_thresholds);
    RUN_TEST(test_status_messages_voc_and_nox_use_display_thresholds);
    RUN_TEST(test_status_messages_humidity_uses_display_thresholds);
    RUN_TEST(test_status_messages_dew_point_defaults_match_display_thresholds);
    return UNITY_END();
}
