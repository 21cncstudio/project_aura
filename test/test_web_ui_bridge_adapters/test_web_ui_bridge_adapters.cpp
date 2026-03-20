#include <unity.h>

#include "web/WebUiBridgeAdapters.h"

void setUp() {}
void tearDown() {}

void test_web_ui_bridge_adapters_capture_settings_snapshot_copies_fields() {
    WebUiBridge::Snapshot snapshot{};
    snapshot.available = true;
    snapshot.night_mode = true;
    snapshot.night_mode_locked = true;
    snapshot.backlight_on = false;
    snapshot.units_c = false;
    snapshot.temp_offset = 1.5f;
    snapshot.hum_offset = -2.0f;
    snapshot.display_name = "Aura";

    const WebSettingsUtils::SettingsSnapshot result =
        WebUiBridgeAdapters::captureSettingsSnapshot(snapshot);

    TEST_ASSERT_TRUE(result.available);
    TEST_ASSERT_TRUE(result.night_mode);
    TEST_ASSERT_TRUE(result.night_mode_locked);
    TEST_ASSERT_FALSE(result.backlight_on);
    TEST_ASSERT_FALSE(result.units_c);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, result.temp_offset);
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, result.hum_offset);
    TEST_ASSERT_EQUAL_STRING("Aura", result.display_name.c_str());
}

void test_web_ui_bridge_adapters_to_ui_settings_update_copies_flags_and_values() {
    WebSettingsUtils::SettingsUpdate update{};
    update.has_night_mode = true;
    update.night_mode = true;
    update.has_backlight = true;
    update.backlight_on = false;
    update.has_units_c = true;
    update.units_c = false;
    update.has_temp_offset = true;
    update.temp_offset = 2.25f;
    update.has_hum_offset = true;
    update.hum_offset = -1.25f;
    update.has_display_name = true;
    update.display_name = "Aura";
    update.restart_requested = true;

    const WebUiBridge::SettingsUpdate result = WebUiBridgeAdapters::toUiSettingsUpdate(update);
    TEST_ASSERT_TRUE(result.has_night_mode);
    TEST_ASSERT_TRUE(result.night_mode);
    TEST_ASSERT_TRUE(result.has_backlight);
    TEST_ASSERT_FALSE(result.backlight_on);
    TEST_ASSERT_TRUE(result.has_units_c);
    TEST_ASSERT_FALSE(result.units_c);
    TEST_ASSERT_TRUE(result.has_temp_offset);
    TEST_ASSERT_EQUAL_FLOAT(2.25f, result.temp_offset);
    TEST_ASSERT_TRUE(result.has_hum_offset);
    TEST_ASSERT_EQUAL_FLOAT(-1.25f, result.hum_offset);
    TEST_ASSERT_TRUE(result.has_display_name);
    TEST_ASSERT_EQUAL_STRING("Aura", result.display_name.c_str());
    TEST_ASSERT_TRUE(result.restart_requested);
}

void test_web_ui_bridge_adapters_to_ui_save_updates_copy_payloads() {
    WebWifiSaveUtils::SaveUpdate wifi{};
    wifi.ssid = "AuraNet";
    wifi.pass = "secret";
    wifi.enabled = true;
    const WebUiBridge::WifiSaveUpdate wifi_result = WebUiBridgeAdapters::toUiWifiSaveUpdate(wifi);
    TEST_ASSERT_EQUAL_STRING("AuraNet", wifi_result.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", wifi_result.pass.c_str());
    TEST_ASSERT_TRUE(wifi_result.enabled);

    WebMqttSaveUtils::SaveUpdate mqtt{};
    mqtt.host = "broker";
    mqtt.port = 1884;
    mqtt.user = "user";
    mqtt.pass = "pass";
    mqtt.base_topic = "aura/main";
    mqtt.device_name = "Aura";
    mqtt.discovery = false;
    mqtt.anonymous = true;
    const WebUiBridge::MqttSaveUpdate mqtt_result = WebUiBridgeAdapters::toUiMqttSaveUpdate(mqtt);
    TEST_ASSERT_EQUAL_STRING("broker", mqtt_result.host.c_str());
    TEST_ASSERT_EQUAL_UINT16(1884, mqtt_result.port);
    TEST_ASSERT_EQUAL_STRING("user", mqtt_result.user.c_str());
    TEST_ASSERT_EQUAL_STRING("pass", mqtt_result.pass.c_str());
    TEST_ASSERT_EQUAL_STRING("aura/main", mqtt_result.base_topic.c_str());
    TEST_ASSERT_EQUAL_STRING("Aura", mqtt_result.device_name.c_str());
    TEST_ASSERT_FALSE(mqtt_result.discovery);
    TEST_ASSERT_TRUE(mqtt_result.anonymous);
}

void test_web_ui_bridge_adapters_to_ui_dac_updates_map_types_and_values() {
    WebDacApiUtils::DacActionUpdate action{};
    action.type = WebDacApiUtils::DacActionUpdate::Type::SetTimerSeconds;
    action.auto_mode = true;
    action.manual_step = 4;
    action.timer_seconds = 900;

    const WebUiBridge::DacActionUpdate action_result =
        WebUiBridgeAdapters::toUiDacActionUpdate(action);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebUiBridge::DacActionUpdate::Type::SetTimerSeconds),
                          static_cast<int>(action_result.type));
    TEST_ASSERT_TRUE(action_result.auto_mode);
    TEST_ASSERT_EQUAL_UINT8(4, action_result.manual_step);
    TEST_ASSERT_EQUAL_UINT32(900, action_result.timer_seconds);

    WebDacApiUtils::DacAutoUpdate auto_update{};
    auto_update.config.enabled = true;
    auto_update.rearm = true;
    const WebUiBridge::DacAutoUpdate auto_result =
        WebUiBridgeAdapters::toUiDacAutoUpdate(auto_update);
    TEST_ASSERT_TRUE(auto_result.config.enabled);
    TEST_ASSERT_TRUE(auto_result.rearm);
}

void test_web_ui_bridge_adapters_to_ui_theme_update_copies_colors() {
    ThemeColors colors{};
    colors.screen_bg = 0x123456;
    colors.card_bg = 0x234567;

    const WebUiBridge::ThemeUpdate result = WebUiBridgeAdapters::toUiThemeUpdate(colors);
    TEST_ASSERT_EQUAL_UINT32(0x123456, static_cast<uint32_t>(result.colors.screen_bg));
    TEST_ASSERT_EQUAL_UINT32(0x234567, static_cast<uint32_t>(result.colors.card_bg));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_ui_bridge_adapters_capture_settings_snapshot_copies_fields);
    RUN_TEST(test_web_ui_bridge_adapters_to_ui_settings_update_copies_flags_and_values);
    RUN_TEST(test_web_ui_bridge_adapters_to_ui_save_updates_copy_payloads);
    RUN_TEST(test_web_ui_bridge_adapters_to_ui_dac_updates_map_types_and_values);
    RUN_TEST(test_web_ui_bridge_adapters_to_ui_theme_update_copies_colors);
    return UNITY_END();
}
