#pragma once
#include <Arduino.h>
#include <driver/i2c.h>
#include <time.h>

// Optional local secrets (ignored by git). See include/secrets.h.example.
#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#else
namespace Secrets {
    constexpr const char *WIFI_SSID = "";
    constexpr const char *WIFI_PASS = "";
    constexpr bool WIFI_ENABLED = false;
    constexpr const char *MQTT_HOST = "mqtt.local";
    constexpr uint16_t MQTT_PORT = 1883;
    constexpr const char *MQTT_USER = "";
    constexpr const char *MQTT_PASS = "";
    constexpr const char *MQTT_BASE = "project_aura/room1";
    constexpr const char *MQTT_NAME = "Project Aura";
    constexpr bool MQTT_USER_ENABLED = false;
    constexpr bool MQTT_DISCOVERY = true;
    constexpr bool MQTT_ANONYMOUS = false;
} // namespace Secrets
#endif
#else
namespace Secrets {
    constexpr const char *WIFI_SSID = "";
    constexpr const char *WIFI_PASS = "";
    constexpr bool WIFI_ENABLED = false;
    constexpr const char *MQTT_HOST = "mqtt.local";
    constexpr uint16_t MQTT_PORT = 1883;
    constexpr const char *MQTT_USER = "";
    constexpr const char *MQTT_PASS = "";
    constexpr const char *MQTT_BASE = "project_aura/room1";
    constexpr const char *MQTT_NAME = "Project Aura";
    constexpr bool MQTT_USER_ENABLED = false;
    constexpr bool MQTT_DISCOVERY = true;
    constexpr bool MQTT_ANONYMOUS = false;
} // namespace Secrets
#endif

namespace Config {
    constexpr uint8_t I2C_SDA_PIN = 8;
    constexpr uint8_t I2C_SCL_PIN = 9;
    constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
    constexpr uint32_t I2C_FREQ_HZ = 100000;
    constexpr uint32_t I2C_TIMEOUT_MS = 50;
    constexpr uint8_t LOG_LEVEL = 3; // 0=error, 1=warn, 2=info, 3=debug
    constexpr uint32_t MEM_LOG_INTERVAL_MS = 15UL * 60UL * 1000UL;
    constexpr uint32_t SAFE_BOOT_STABLE_MS = 60UL * 1000UL;
    constexpr uint8_t SAFE_BOOT_MAX_REBOOTS = 5;
    constexpr uint32_t LAST_GOOD_COMMIT_DELAY_MS = 3UL * 60UL * 1000UL;

    enum class Language : uint8_t {
        EN = 0,
        DE = 1,
        ES = 2,
        FR = 3,
        IT = 4,
        PT = 5,
        NL = 6,
        ZH = 7,
        COUNT
    };

    inline Language clampLanguage(int value) {
        if (value < 0 || value >= static_cast<int>(Language::COUNT)) {
            return Language::EN;
        }
        return static_cast<Language>(value);
    }

    constexpr uint8_t SEN66_ADDR = 0x6B;
    constexpr uint16_t SEN66_CMD_START = 0x0021;
    constexpr uint16_t SEN66_CMD_STOP = 0x0104;
    constexpr uint16_t SEN66_CMD_DATA_READY = 0x0202;
    constexpr uint16_t SEN66_CMD_READ_VALUES = 0x0300;
    constexpr uint16_t SEN66_CMD_READ_STATUS = 0xD206;
    constexpr uint16_t SEN66_CMD_FRC = 0x6707;
    constexpr uint16_t SEN66_CMD_ASC = 0x6711;
    constexpr uint16_t SEN66_CMD_AMBIENT_PRESSURE = 0x6720;
    constexpr uint16_t SEN66_CMD_VOC_STATE = 0x6181;
    constexpr uint16_t SEN66_CMD_TEMP_OFFSET = 0x60B2;
    constexpr uint16_t SEN66_CMD_DEVICE_RESET = 0xD304;
    constexpr uint8_t GT911_ADDR_PRIMARY = 0x5D;
    constexpr uint8_t GT911_ADDR_ALT = 0x14;
    constexpr uint16_t GT911_REG_PRODUCT_ID = 0x8140;
    constexpr uint8_t SFA3X_ADDR = 0x5D;
    constexpr uint16_t SFA3X_CMD_START = 0x0006;
    constexpr uint16_t SFA3X_CMD_STOP = 0x0104;
    constexpr uint16_t SFA3X_CMD_READ_VALUES = 0x0327;
    constexpr uint8_t PCF8523_ADDR = 0x68;
    constexpr uint8_t PCF8523_REG_SECONDS = 0x03;
    constexpr uint8_t DPS310_ADDR_PRIMARY = 0x77;
    constexpr uint8_t DPS310_ADDR_ALT = 0x76;
    constexpr uint8_t DPS310_PRSB2 = 0x00;
    constexpr uint8_t DPS310_TMPB2 = 0x03;
    constexpr uint8_t DPS310_PRSCFG = 0x06;
    constexpr uint8_t DPS310_TMPCFG = 0x07;
    constexpr uint8_t DPS310_MEASCFG = 0x08;
    constexpr uint8_t DPS310_CFGREG = 0x09;
    constexpr uint8_t DPS310_RESET = 0x0C;
    constexpr uint8_t DPS310_PRODREVID = 0x0D;
    constexpr uint8_t DPS310_TMPCOEFSRCE = 0x28;
    constexpr uint8_t DPS310_MODE_CONT_PRESTEMP = 0x07;
    constexpr uint8_t BMP580_ADDR_PRIMARY = 0x46;
    constexpr uint8_t BMP580_ADDR_ALT = 0x47;
    constexpr uint8_t BMP580_REG_CHIP_ID = 0x01;
    constexpr uint8_t BMP580_REG_STATUS = 0x28;
    constexpr uint8_t BMP580_REG_DSP_IIR = 0x31;
    constexpr uint8_t BMP580_REG_TEMP_XLSB = 0x1D;
    constexpr uint8_t BMP580_REG_PRESS_XLSB = 0x20;
    constexpr uint8_t BMP580_REG_OSR_CONFIG = 0x36;
    constexpr uint8_t BMP580_REG_ODR_CONFIG = 0x37;
    constexpr uint8_t BMP580_REG_CMD = 0x7E;
    constexpr uint8_t BMP580_CHIP_ID_PRIMARY = 0x50;
    constexpr uint8_t BMP580_CHIP_ID_SECONDARY = 0x51;
    constexpr uint8_t BMP580_SOFT_RESET_CMD = 0xB6;
    constexpr uint8_t BMP580_STATUS_NVM_RDY = 0x02;
    constexpr uint8_t BMP580_ODR_1_HZ = 0x1C;
    constexpr uint8_t BMP580_OSR_4X = 0x02;
    constexpr uint8_t BMP580_IIR_BYPASS = 0x00;
    constexpr uint8_t BMP580_POWERMODE_CONTINUOUS = 0x03;

    constexpr uint32_t SEN66_START_DELAY_MS = 50;
    constexpr uint32_t SEN66_STOP_DELAY_MS = 1400;
    constexpr uint32_t SEN66_CMD_DELAY_MS = 20;
    constexpr uint32_t SEN66_FRC_DELAY_MS = 500;
    constexpr uint32_t SEN66_DEVICE_RESET_DELAY_MS = 1200;
    constexpr uint32_t SEN66_START_RETRY_MS = 2000;
    constexpr uint32_t SEN66_STARTUP_GRACE_MS = 5000;
    constexpr uint32_t SEN66_POLL_MS = 1000;
    constexpr uint32_t SEN66_STALE_MS = 6000;
    constexpr uint32_t SEN66_STATUS_MS = 5000;
    constexpr uint32_t SEN66_CO2_INVALID_MS = 15000;
    constexpr uint16_t SEN66_FRC_REF_PPM = 420;
    constexpr uint32_t SEN66_PRESSURE_UPDATE_MS = 60000;
    constexpr uint16_t SEN66_PRESSURE_MIN_HPA = 700;
    constexpr uint16_t SEN66_PRESSURE_MAX_HPA = 1200;
    constexpr uint32_t SEN66_VOC_STATE_SAVE_MS = 60UL * 60UL * 1000UL;
    constexpr size_t SEN66_VOC_STATE_LEN = 8;
    constexpr uint32_t SEN66_GAS_WARMUP_MS = 300UL * 1000UL;
    constexpr float SEN66_TEMP_OFFSET_SLOPE = 0.0f;
    constexpr uint16_t SEN66_TEMP_OFFSET_TIME_S = 0;
    constexpr uint16_t SEN66_TEMP_OFFSET_SLOT = 0;
    constexpr uint32_t SFA3X_START_DELAY_MS = 1;
    constexpr uint32_t SFA3X_STOP_DELAY_MS = 50;
    constexpr uint32_t SFA3X_READ_DELAY_MS = 5;
    constexpr uint32_t SFA3X_POLL_MS = 1000;
    constexpr uint32_t SFA3X_STALE_MS = 3000;
    // Sensor sanity filter ranges (hard limits from datasheets).
    constexpr float SEN66_TEMP_MIN_C = -10.0f;
    constexpr float SEN66_TEMP_MAX_C = 60.0f;
    constexpr float SEN66_TEMP_RECOMM_MIN_C = 10.0f;
    constexpr float SEN66_TEMP_RECOMM_MAX_C = 40.0f;
    constexpr float SEN66_HUM_MIN = 0.0f;
    constexpr float SEN66_HUM_MAX = 100.0f;
    constexpr float SEN66_HUM_RECOMM_MIN = 20.0f;
    constexpr float SEN66_HUM_RECOMM_MAX = 80.0f;
    constexpr float SEN66_PM_MIN_UGM3 = 0.0f;
    constexpr float SEN66_PM_MAX_UGM3 = 999.0f;
    constexpr int SEN66_CO2_MIN_PPM = 0;
    constexpr int SEN66_CO2_MAX_PPM = 40000;
    constexpr int SEN66_VOC_MIN = 1;
    constexpr int SEN66_VOC_MAX = 500;
    constexpr int SEN66_NOX_MIN = 1;
    constexpr int SEN66_NOX_MAX = 500;
    constexpr float DPS310_PRESSURE_MIN_HPA = 300.0f;
    constexpr float DPS310_PRESSURE_MAX_HPA = 1200.0f;
    constexpr float SFA3X_HCHO_MIN_PPB = 0.0f;
    constexpr float SFA3X_HCHO_MAX_PPB = 1000.0f;

    constexpr uint32_t CLOCK_TICK_MS = 1000;
    constexpr uint32_t NTP_SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
    constexpr uint32_t NTP_FRESH_MS = 12UL * 60UL * 60UL * 1000UL;
    constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 10000;
    constexpr uint32_t NTP_RETRY_MS = 5UL * 60UL * 1000UL;
    constexpr uint32_t RTC_RESTORE_INTERVAL_MS = 5000;
    constexpr time_t TIME_VALID_EPOCH = 1577836800;
    constexpr uint32_t MQTT_PUBLISH_MS = 30000;
    constexpr uint32_t MQTT_RETRY_MS = 30000;
    constexpr uint32_t MQTT_RETRY_LONG_MS = 600000;
    constexpr uint32_t MQTT_RETRY_HOURLY_MS = 60UL * 60UL * 1000UL;
    constexpr uint16_t MQTT_BUFFER_SIZE = 1024;
    constexpr uint16_t MQTT_DEFAULT_PORT = Secrets::MQTT_PORT;
    constexpr const char *MQTT_DEFAULT_HOST = Secrets::MQTT_HOST;
    constexpr const char *MQTT_DEFAULT_USER = Secrets::MQTT_USER;
    constexpr const char *MQTT_DEFAULT_BASE = Secrets::MQTT_BASE;
    constexpr const char *MQTT_DEFAULT_NAME = Secrets::MQTT_NAME;
    constexpr char MQTT_AVAIL_ONLINE[] = "online";
    constexpr char MQTT_AVAIL_OFFLINE[] = "offline";
    constexpr uint32_t BACKLIGHT_TIMEOUT_30S = 30UL * 1000UL;
    constexpr uint32_t BACKLIGHT_TIMEOUT_1M = 60UL * 1000UL;
    constexpr uint32_t BACKLIGHT_TIMEOUT_5M = 5UL * 60UL * 1000UL;
    constexpr uint32_t BACKLIGHT_SCHEDULE_WAKE_MS = 30UL * 1000UL;
    constexpr uint32_t BACKLIGHT_WAKE_BLOCK_MS = 250;
    constexpr uint32_t AUTO_NIGHT_POLL_MS = 1000;
    constexpr uint32_t BLINK_PERIOD_MS = 500;
    constexpr uint32_t UI_TICK_MS = 30;
    constexpr uint32_t BOOT_LOGO_MS = 5000;
    constexpr uint32_t BOOT_DIAG_MS = 3000;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 45000;
    constexpr uint32_t WIFI_CONNECT_RETRY_DELAY_MS = 1000;
    constexpr uint8_t WIFI_CONNECT_MAX_RETRIES = 3;
    constexpr uint32_t WIFI_UI_UPDATE_MS = 500;
    constexpr char WIFI_AP_SSID[] = "ProjectAura-Setup";
    constexpr uint32_t DPS310_POLL_MS = 1000;
    constexpr uint32_t DPS310_STALE_MS = 5000;
    constexpr uint32_t DPS310_RECOVER_MS = 30UL * 1000UL;
    constexpr uint32_t DPS310_RECOVER_COOLDOWN_MS = 60UL * 1000UL;
    constexpr float DPS310_PRESSURE_ALPHA = 0.12f;
    constexpr uint32_t BMP580_POLL_MS = 1000;
    constexpr uint32_t BMP580_STALE_MS = 5000;
    constexpr uint32_t BMP580_RECOVER_MS = 30UL * 1000UL;
    constexpr uint32_t BMP580_RECOVER_COOLDOWN_MS = 60UL * 1000UL;
    constexpr float BMP580_PRESSURE_ALPHA = 0.12f;
    constexpr uint32_t PRESSURE_HISTORY_STEP_MS = 5UL * 60UL * 1000UL;
    constexpr int PRESSURE_HISTORY_24H_SAMPLES = 288;
    constexpr int PRESSURE_HISTORY_3H_STEPS = 36;
    constexpr uint32_t PRESSURE_HISTORY_SAVE_MS = 30UL * 60UL * 1000UL;
    constexpr uint32_t PRESSURE_HISTORY_MAX_AGE_S =
        (PRESSURE_HISTORY_STEP_MS / 1000UL) * PRESSURE_HISTORY_24H_SAMPLES;
    constexpr uint32_t PRESSURE_HISTORY_FILL_SHORT_S = 15UL * 60UL;
    constexpr uint32_t PRESSURE_HISTORY_FILL_LONG_S = 4UL * 60UL * 60UL;
    constexpr size_t THEME_SWATCH_COUNT = 12;

    constexpr float BASE_TEMP_OFFSET = 0.0f;
    constexpr float HUM_OFFSET_STEP = 1.0f;
    constexpr float HUM_OFFSET_MIN = -10.0f;
    constexpr float HUM_OFFSET_MAX = 10.0f;
    constexpr uint8_t MQTT_MAX_FAILS = 1;
    constexpr uint8_t MQTT_CONNECT_MAX_FAILS = 3;

    struct ThemeConfig {
        bool valid = false;
        uint32_t screen_bg = 0;
        uint32_t card_bg = 0;
        uint32_t card_border = 0;
        uint32_t text_primary = 0;
        uint32_t shadow_color = 0;
        bool shadow_enabled = true;
        bool gradient_enabled = false;
        uint32_t gradient_color = 0;
        uint32_t gradient_direction = 0;
        bool screen_gradient_enabled = false;
        uint32_t screen_gradient_color = 0;
        uint32_t screen_gradient_direction = 0;
    };

    struct StoredConfig {
        String wifi_ssid = Secrets::WIFI_SSID;
        String wifi_pass = Secrets::WIFI_PASS;
        bool wifi_enabled = Secrets::WIFI_ENABLED;

        String mqtt_host = MQTT_DEFAULT_HOST;
        uint16_t mqtt_port = MQTT_DEFAULT_PORT;
        String mqtt_user = MQTT_DEFAULT_USER;
        String mqtt_pass = Secrets::MQTT_PASS;
        String mqtt_base_topic = MQTT_DEFAULT_BASE;
        String mqtt_device_name = MQTT_DEFAULT_NAME;
        bool mqtt_user_enabled = Secrets::MQTT_USER_ENABLED;
        bool mqtt_discovery = Secrets::MQTT_DISCOVERY;
        bool mqtt_anonymous = Secrets::MQTT_ANONYMOUS;

        float temp_offset = 0.0f;
        float hum_offset = 0.0f;
        bool units_c = true;
        bool night_mode = false;
        bool led_indicators = true;
        bool alert_blink = true;
        bool asc_enabled = true;
        Language language = Language::EN;

        uint32_t backlight_timeout_s = 0;
        bool backlight_schedule_enabled = false;
        int backlight_sleep_hour = 23;
        int backlight_sleep_minute = 0;
        int backlight_wake_hour = 6;
        int backlight_wake_minute = 0;

        bool auto_night_enabled = false;
        int auto_night_start_hour = 21;
        int auto_night_start_minute = 0;
        int auto_night_end_hour = 7;
        int auto_night_end_minute = 0;

        bool ntp_enabled = true;
        int tz_index = -1;

        ThemeConfig theme{};
    };
}
