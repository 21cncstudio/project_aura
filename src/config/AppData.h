#pragma once
#include <Arduino.h>
#include <lvgl.h>

struct SensorData {
    float temperature = 0.0f;
    float humidity = 0.0f;
    float pm1 = 0.0f;
    float pm25 = 0.0f;
    float pm4 = 0.0f;
    float pm10 = 0.0f;
    float pressure = 0.0f;
    float pressure_delta_3h = 0.0f;
    float pressure_delta_24h = 0.0f;
    float hcho = 0.0f;
    int co2 = 0;
    int voc_index = 0;
    int nox_index = 0;
    bool temp_valid = false;
    bool hum_valid = false;
    bool pm_valid = false;
    bool pm25_valid = false;
    bool pm10_valid = false;
    bool co2_valid = false;
    bool voc_valid = false;
    bool nox_valid = false;
    bool hcho_valid = false;
    bool pressure_valid = false;
    bool pressure_delta_3h_valid = false;
    bool pressure_delta_24h_valid = false;
};

struct AirQuality {
    const char *status;
    int score;
    lv_color_t color;
};

struct ThemeColors {
    lv_color_t screen_bg;
    lv_color_t card_bg;
    lv_color_t card_border;
    lv_color_t text_primary;
    lv_color_t shadow_color;
    bool shadow_enabled;
    bool gradient_enabled;
    lv_color_t gradient_color;
    lv_grad_dir_t gradient_direction;
    bool screen_gradient_enabled;
    lv_color_t screen_gradient_color;
    lv_grad_dir_t screen_gradient_direction;
};

struct ThemeSwatch {
    lv_obj_t *btn;
    lv_obj_t *card;
    lv_obj_t *label;
};

struct TimeZoneEntry {
    const char *name;
    int16_t offset_min;
    const char *posix;
};

static const TimeZoneEntry kTimeZones[] = {
    { "Etc/GMT+12", -12 * 60, nullptr },
    { "Pacific/Midway", -11 * 60, nullptr },
    { "Pacific/Honolulu", -10 * 60, nullptr },
    { "America/Anchorage", -9 * 60, nullptr },
    { "America/Los_Angeles", -8 * 60, nullptr },
    { "America/Denver", -7 * 60, nullptr },
    { "America/Chicago", -6 * 60, nullptr },
    { "America/New_York", -5 * 60, "EST5EDT,M3.2.0,M11.1.0" },
    { "America/Santiago", -4 * 60, nullptr },
    { "America/St_Johns", -3 * 60 - 30, nullptr },
    { "America/Sao_Paulo", -3 * 60, nullptr },
    { "Atlantic/South_Georgia", -2 * 60, nullptr },
    { "Atlantic/Azores", -1 * 60, nullptr },
    { "Europe/London", 0, "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Europe/Paris", 1 * 60, "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Kiev", 2 * 60, "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Africa/Cairo", 2 * 60, nullptr },
    { "Europe/Moscow", 3 * 60, "MSK-3" },
    { "Asia/Tehran", 3 * 60 + 30, nullptr },
    { "Asia/Dubai", 4 * 60, nullptr },
    { "Asia/Kabul", 4 * 60 + 30, nullptr },
    { "Asia/Karachi", 5 * 60, nullptr },
    { "Asia/Kolkata", 5 * 60 + 30, nullptr },
    { "Asia/Kathmandu", 5 * 60 + 45, nullptr },
    { "Asia/Dhaka", 6 * 60, nullptr },
    { "Asia/Yangon", 6 * 60 + 30, nullptr },
    { "Asia/Bangkok", 7 * 60, nullptr },
    { "Asia/Shanghai", 8 * 60, "CST-8" },
    { "Asia/Singapore", 8 * 60, nullptr },
    { "Asia/Tokyo", 9 * 60, nullptr },
    { "Australia/Adelaide", 9 * 60 + 30, nullptr },
    { "Australia/Sydney", 10 * 60, nullptr },
    { "Pacific/Noumea", 11 * 60, nullptr },
    { "Pacific/Auckland", 12 * 60, nullptr },
    { "Pacific/Chatham", 12 * 60 + 45, nullptr },
    { "Pacific/Tongatapu", 13 * 60, nullptr },
    { "Pacific/Kiritimati", 14 * 60, nullptr }
};

constexpr size_t TIME_ZONE_COUNT = sizeof(kTimeZones) / sizeof(kTimeZones[0]);
