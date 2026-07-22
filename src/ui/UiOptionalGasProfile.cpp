// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiOptionalGasProfile.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/AppConfig.h"

namespace UiOptionalGasProfile {
namespace {

constexpr Profile kFallbackProfile{
    OptionalGasType::None,
    "Gas",
    "Optional gas",
    1,
    1,
    1.0f,
    2.0f,
    3.0f,
    0.0f,
    1.0f,
    100.0f,
    "ppm",
    ClassificationMode::Increasing,
    0.0f,
    0.0f,
    "Optional DFRobot electrochemical gas sensor.",
};

// Keep in sync with dashboard OPTIONAL_GAS_PROFILES until these values are served by firmware state/API.
constexpr Profile kProfiles[] = {
    {OptionalGasType::NH3, "NH3", "Ammonia (NH3)", 0, 0, 5.0f, 25.0f, 35.0f, 5.0f, 10.0f, 100.0f, "ppm", ClassificationMode::Increasing, 0.0f, 0.0f, "Optional DFRobot electrochemical gas sensor. Higher ppm means higher gas concentration."},
    {OptionalGasType::SO2, "SO2", "Sulfur dioxide (SO2)", 1, 2, 0.05f, 0.10f, 2.0f, 0.05f, 0.5f, 100.0f, "ppm", ClassificationMode::Increasing, 0.0f, 0.0f, "Optional DFRobot electrochemical gas sensor. Higher ppm means higher gas concentration."},
    {OptionalGasType::NO2, "NO2", "Nitrogen dioxide (NO2)", 1, 2, 0.05f, 0.10f, 1.0f, 0.05f, 0.3f, 100.0f, "ppm", ClassificationMode::Increasing, 0.0f, 0.0f, "Optional DFRobot electrochemical gas sensor. Higher ppm means higher gas concentration."},
    {OptionalGasType::H2S, "H2S", "Hydrogen sulfide (H2S)", 0, 1, 0.5f, 1.0f, 10.0f, 0.5f, 2.0f, 100.0f, "ppm", ClassificationMode::Increasing, 0.0f, 0.0f, "Optional DFRobot electrochemical gas sensor. Higher ppm means higher gas concentration."},
    {OptionalGasType::O3, "O3", "Ozone (O3)", 1, 2, 0.05f, 0.10f, 0.50f, 0.05f, 0.2f, 100.0f, "ppm", ClassificationMode::Increasing, 0.0f, 0.0f, "Optional DFRobot electrochemical gas sensor. Higher ppm means higher gas concentration."},
    {OptionalGasType::O2, "O2", "Oxygen (O2)", 1, 1, 0.0f, 0.0f, 25.0f, 21.5f, 5.0f, 10.0f, "%Vol", ClassificationMode::NormalRange, Config::SEN0465_O2_NORMAL_MIN_PERCENT_VOL, Config::SEN0465_O2_NORMAL_MAX_PERCENT_VOL, "SEN0465 ambient oxygen reading (0-25 %Vol). Reference only; Aura is not a certified safety monitor or breathing-gas analyzer."},
};

void trim_decimal(char *buf) {
    if (!buf) {
        return;
    }
    char *dot = strchr(buf, '.');
    if (!dot) {
        return;
    }
    char *end = buf + strlen(buf);
    while (end > dot + 1 && *(end - 1) == '0') {
        --end;
        *end = '\0';
    }
    if (end == dot + 1 && *dot == '.') {
        *dot = '\0';
    }
}

void format_number(float value, uint8_t decimals, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return;
    }
    if (!isfinite(value)) {
        snprintf(buf, buf_size, "--");
        return;
    }
    switch (decimals) {
        case 0:
            snprintf(buf, buf_size, "%.0f", value);
            break;
        case 1:
            snprintf(buf, buf_size, "%.1f", value);
            break;
        case 2:
        default:
            snprintf(buf, buf_size, "%.2f", value);
            break;
    }
    trim_decimal(buf);
}

uint8_t display_decimals_for_value(float value, uint8_t decimals, uint8_t fallback) {
    uint8_t normalized = fallback <= 2 ? fallback : 1;
    if (decimals <= 2) {
        normalized = decimals;
    }
    if (normalized == 2 && isfinite(value) && value >= 1.0f) {
        return 1;
    }
    return normalized;
}

} // namespace

const Profile &forType(OptionalGasType type) {
    for (const Profile &profile : kProfiles) {
        if (profile.type == type) {
            return profile;
        }
    }
    return kFallbackProfile;
}

bool isKnown(OptionalGasType type) {
    return forType(type).type != OptionalGasType::None;
}

Band classify(const Profile &profile, float value, bool valid) {
    if (!valid || !isfinite(value) || value < 0.0f) {
        return Band::Inactive;
    }
    if (profile.classification == ClassificationMode::NormalRange) {
        return value >= profile.normal_min && value <= profile.normal_max
                   ? Band::Green
                   : Band::Red;
    }
    if (value <= profile.green_max_ppm) return Band::Green;
    if (value <= profile.yellow_max_ppm) return Band::Yellow;
    if (value <= profile.orange_max_ppm) return Band::Orange;
    return Band::Red;
}

void formatValue(const Profile &profile, float ppm, char *buf, size_t buf_size) {
    format_number(ppm, profile.value_decimals, buf, buf_size);
}

void formatValue(const Profile &profile, float ppm, uint8_t decimals, char *buf, size_t buf_size) {
    format_number(ppm,
                  display_decimals_for_value(ppm, decimals, profile.value_decimals),
                  buf,
                  buf_size);
}

void formatThreshold(const Profile &profile, float ppm, char *buf, size_t buf_size) {
    format_number(ppm, profile.threshold_decimals, buf, buf_size);
}

void formatBandLabel(const Profile &profile, uint8_t band, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return;
    }

    if (profile.classification == ClassificationMode::NormalRange) {
        char normal_min[16];
        char normal_max[16];
        formatThreshold(profile, profile.normal_min, normal_min, sizeof(normal_min));
        formatThreshold(profile, profile.normal_max, normal_max, sizeof(normal_max));
        switch (band) {
            case 0:
                snprintf(buf, buf_size, "Normal reference: %s-%s %s\nWithin the reference range", normal_min, normal_max, profile.unit);
                break;
            case 1:
                snprintf(buf, buf_size, "Oxygen-deficient: <%s %s\nLeave the area and verify with safety equipment", normal_min, profile.unit);
                break;
            case 2:
                snprintf(buf, buf_size, "Oxygen-enriched: >%s %s\nIncreased fire risk; verify with safety equipment", normal_max, profile.unit);
                break;
            case 3:
            default:
                snprintf(buf, buf_size, "Reference only\nNot a certified safety monitor or breathing-gas analyzer");
                break;
        }
        return;
    }

    char green[16];
    char yellow[16];
    char orange[16];
    formatThreshold(profile, profile.green_max_ppm, green, sizeof(green));
    formatThreshold(profile, profile.yellow_max_ppm, yellow, sizeof(yellow));
    formatThreshold(profile, profile.orange_max_ppm, orange, sizeof(orange));

    switch (band) {
        case 0:
            snprintf(buf,
                     buf_size,
                     "Low: <=%s %s\nLowest reference band for %s",
                     green,
                     profile.unit,
                     profile.label);
            break;
        case 1:
            snprintf(buf,
                     buf_size,
                     "Slight elevation: >%s-%s %s\nKeep air moving and watch trend",
                     green,
                     yellow,
                     profile.unit);
            break;
        case 2:
            snprintf(buf,
                     buf_size,
                     "Elevated: >%s-%s %s\nVentilate and check source",
                     yellow,
                     orange,
                     profile.unit);
            break;
        case 3:
        default:
            snprintf(buf,
                     buf_size,
                     "High: >%s %s\nReduce exposure; verify with safety equipment",
                     orange,
                     profile.unit);
            break;
    }
}

} // namespace UiOptionalGasProfile
