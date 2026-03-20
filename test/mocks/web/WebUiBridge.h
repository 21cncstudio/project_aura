// Mocked for native_test only.
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "config/AppData.h"
#include "modules/DacAutoConfig.h"

class WebUiBridge {
public:
    struct Snapshot {
        bool available = false;
        bool night_mode = false;
        bool night_mode_locked = false;
        bool backlight_on = false;
        bool units_c = true;
        float temp_offset = 0.0f;
        float hum_offset = 0.0f;
        String display_name;
        bool mqtt_screen_open = false;
        bool theme_screen_open = false;
        bool theme_custom_screen_open = false;
        ThemeColors theme_preview_colors{};
    };

    struct SettingsUpdate {
        bool has_night_mode = false;
        bool night_mode = false;
        bool has_backlight = false;
        bool backlight_on = false;
        bool has_units_c = false;
        bool units_c = true;
        bool has_temp_offset = false;
        float temp_offset = 0.0f;
        bool has_hum_offset = false;
        float hum_offset = 0.0f;
        bool has_display_name = false;
        String display_name;
        bool restart_requested = false;
    };

    struct ThemeUpdate {
        ThemeColors colors{};
    };

    struct DacActionUpdate {
        enum class Type : uint8_t {
            SetMode = 0,
            SetManualStep,
            SetTimerSeconds,
            Start,
            Stop,
            StartAuto,
        };

        Type type = Type::Start;
        bool auto_mode = false;
        uint8_t manual_step = 1;
        uint32_t timer_seconds = 0;
    };

    struct DacAutoUpdate {
        DacAutoConfig config{};
        bool rearm = false;
    };

    struct WifiSaveUpdate {
        String ssid;
        String pass;
        bool enabled = true;
    };

    struct MqttSaveUpdate {
        String host;
        uint16_t port = 1883;
        String user;
        String pass;
        String base_topic;
        String device_name;
        bool discovery = true;
        bool anonymous = false;
    };
};
