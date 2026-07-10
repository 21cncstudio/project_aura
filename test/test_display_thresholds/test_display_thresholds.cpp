#include <unity.h>

#include <ArduinoJson.h>

#include "modules/DisplayThresholds.h"

void setUp() {}
void tearDown() {}

void test_display_threshold_defaults_classify_current_visual_bands() {
    const DisplayThresholds::Config cfg = DisplayThresholds::defaults();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(22.0f, cfg.temp)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyRange(18.5f, cfg.temp)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyRange(16.5f, cfg.temp)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Red),
                          static_cast<int>(DisplayThresholds::classifyRange(15.5f, cfg.temp)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(12.0f, cfg.dew_point)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(10.5f, cfg.dew_point)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyRange(8.5f, cfg.dew_point)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(10.0f, cfg.ah)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyHigh(700.0f, cfg.co2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Red),
                          static_cast<int>(DisplayThresholds::classifyHigh(101.0f, cfg.hcho)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyHigh(200.0f, cfg.voc)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyHigh(150.0f, cfg.nox)));
}

void test_display_threshold_boundaries_are_inclusive() {
    const DisplayThresholds::Config cfg = DisplayThresholds::defaults();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyHigh(800.0f, cfg.co2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyHigh(1000.0f, cfg.co2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyHigh(1500.0f, cfg.co2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Red),
                          static_cast<int>(DisplayThresholds::classifyHigh(1500.1f, cfg.co2)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyHigh(150.0f, cfg.voc)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyHigh(250.0f, cfg.voc)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyHigh(350.0f, cfg.voc)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Red),
                          static_cast<int>(DisplayThresholds::classifyHigh(350.1f, cfg.voc)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyHigh(50.0f, cfg.nox)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyHigh(100.0f, cfg.nox)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyHigh(200.0f, cfg.nox)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Red),
                          static_cast<int>(DisplayThresholds::classifyHigh(200.1f, cfg.nox)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyRange(20.0f, cfg.rh)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyRange(30.0f, cfg.rh)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(40.0f, cfg.rh)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Green),
                          static_cast<int>(DisplayThresholds::classifyRange(60.0f, cfg.rh)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Yellow),
                          static_cast<int>(DisplayThresholds::classifyRange(65.0f, cfg.rh)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayThresholds::Band::Orange),
                          static_cast<int>(DisplayThresholds::classifyRange(70.0f, cfg.rh)));
}

void test_display_threshold_validation_rejects_bad_order() {
    DisplayThresholds::Config cfg = DisplayThresholds::defaults();
    String error;

    cfg.co2.green = 1200.0f;
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.length() > 0);

    cfg = DisplayThresholds::defaults();
    cfg.voc.yellow = cfg.voc.green;
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.length() > 0);

    cfg = DisplayThresholds::defaults();
    cfg.nox.orange = cfg.nox.yellow;
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.length() > 0);

    cfg = DisplayThresholds::defaults();
    cfg.rh.good_min = cfg.rh.good_max;
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.length() > 0);
}

void test_display_threshold_validation_rejects_negative_rh_and_ah_only() {
    DisplayThresholds::Config cfg = DisplayThresholds::defaults();
    String error;

    cfg.rh = {-1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.find("non-negative") != String::npos);

    cfg = DisplayThresholds::defaults();
    cfg.ah = {-1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    TEST_ASSERT_FALSE(DisplayThresholds::validate(cfg, &error));
    TEST_ASSERT_TRUE(error.find("non-negative") != String::npos);

    cfg = DisplayThresholds::defaults();
    cfg.temp = {-30.0f, -20.0f, -10.0f, 10.0f, 20.0f, 30.0f};
    cfg.dew_point = {-20.0f, -15.0f, -10.0f, 5.0f, 10.0f, 15.0f};
    TEST_ASSERT_TRUE(DisplayThresholds::validate(cfg, &error));
}

void test_display_threshold_serialize_roundtrip_preserves_values_and_switches() {
    DisplayThresholds::Config cfg = DisplayThresholds::defaults();
    cfg.co.orange = 77.0f;
    cfg.voc.yellow = 240.0f;
    cfg.nox.orange = 180.0f;
    cfg.temp.good_min = 19.0f;
    cfg.background_alerts.hcho_enabled = false;
    cfg.background_alerts.co2_enabled = false;

    const String json = DisplayThresholds::serialize(cfg);
    DisplayThresholds::Config parsed{};
    TEST_ASSERT_TRUE(DisplayThresholds::deserialize(json, parsed));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 77.0f, parsed.co.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 240.0f, parsed.voc.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, parsed.nox.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 19.0f, parsed.temp.good_min);
    TEST_ASSERT_FALSE(parsed.background_alerts.hcho_enabled);
    TEST_ASSERT_FALSE(parsed.background_alerts.co2_enabled);
    TEST_ASSERT_TRUE(parsed.background_alerts.co_enabled);
}

void test_display_threshold_deserialize_migrates_legacy_v1_without_voc_and_nox() {
    const String legacy_json =
        "{\"version\":1,\"metrics\":{"
        "\"temp\":{\"orange_min\":15,\"yellow_min\":17,\"good_min\":19,"
        "\"good_max\":24,\"yellow_max\":26,\"orange_max\":29},"
        "\"rh\":{\"orange_min\":20,\"yellow_min\":30,\"good_min\":40,"
        "\"good_max\":60,\"yellow_max\":65,\"orange_max\":70},"
        "\"dew_point\":{\"orange_min\":5,\"yellow_min\":8,\"good_min\":10,"
        "\"good_max\":16,\"yellow_max\":18,\"orange_max\":21},"
        "\"ah\":{\"orange_min\":4,\"yellow_min\":5,\"good_min\":7,"
        "\"good_max\":15,\"yellow_max\":18,\"orange_max\":20},"
        "\"co2\":{\"green\":700,\"yellow\":900,\"orange\":1400},"
        "\"hcho\":{\"green\":25,\"yellow\":55,\"orange\":95},"
        "\"co\":{\"green\":8,\"yellow\":30,\"orange\":90}},"
        "\"background_alerts\":{\"hcho_enabled\":false,\"co_enabled\":true,"
        "\"co2_enabled\":false}}";

    DisplayThresholds::Config parsed{};
    TEST_ASSERT_TRUE(DisplayThresholds::deserialize(legacy_json, parsed));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 19.0f, parsed.temp.good_min);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 700.0f, parsed.co2.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 95.0f, parsed.hcho.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, parsed.co.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, parsed.voc.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, parsed.voc.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 350.0f, parsed.voc.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, parsed.nox.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, parsed.nox.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, parsed.nox.orange);
    TEST_ASSERT_FALSE(parsed.background_alerts.hcho_enabled);
    TEST_ASSERT_FALSE(parsed.background_alerts.co2_enabled);
}

void test_display_threshold_partial_update_keeps_unspecified_values() {
    DisplayThresholds::Config current = DisplayThresholds::defaults();
    ArduinoJson::JsonDocument doc;
    deserializeJson(doc,
                    "{\"metrics\":{\"co2\":{\"green\":700,\"yellow\":900,\"orange\":1400},"
                    "\"voc\":{\"green\":120},\"nox\":{\"orange\":180}},"
                    "\"background_alerts\":{\"co_enabled\":false}}");

    DisplayThresholds::Config updated{};
    TEST_ASSERT_TRUE(DisplayThresholds::applyUpdateJson(doc.as<ArduinoJson::JsonVariantConst>(),
                                                        current,
                                                        updated));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 700.0f, updated.co2.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, updated.voc.green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, current.voc.yellow, updated.voc.yellow);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, updated.nox.orange);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, current.hcho.orange, updated.hcho.orange);
    TEST_ASSERT_FALSE(updated.background_alerts.co_enabled);
    TEST_ASSERT_TRUE(updated.background_alerts.hcho_enabled);
}

void test_display_threshold_manager_falls_back_to_defaults_and_persists_reset() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveTextAtomic(StorageManager::kDisplayThresholdsPath, "{bad"));

    DisplayThresholdManager manager;
    manager.begin(storage);
    DisplayThresholds::Config cfg = manager.snapshot();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, cfg.co2.green);

    cfg.co2.green = 700.0f;
    cfg.co2.yellow = 900.0f;
    cfg.co2.orange = 1400.0f;
    TEST_ASSERT_TRUE(manager.apply(cfg));

    DisplayThresholdManager loaded;
    loaded.begin(storage);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 700.0f, loaded.snapshot().co2.green);

    TEST_ASSERT_TRUE(loaded.reset());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, loaded.snapshot().co2.green);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_display_threshold_defaults_classify_current_visual_bands);
    RUN_TEST(test_display_threshold_boundaries_are_inclusive);
    RUN_TEST(test_display_threshold_validation_rejects_bad_order);
    RUN_TEST(test_display_threshold_validation_rejects_negative_rh_and_ah_only);
    RUN_TEST(test_display_threshold_serialize_roundtrip_preserves_values_and_switches);
    RUN_TEST(test_display_threshold_deserialize_migrates_legacy_v1_without_voc_and_nox);
    RUN_TEST(test_display_threshold_partial_update_keeps_unspecified_values);
    RUN_TEST(test_display_threshold_manager_falls_back_to_defaults_and_persists_reset);
    return UNITY_END();
}
