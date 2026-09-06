// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdarg.h>

class Logger {
public:
    enum Level {
        Error = 0,
        Warn = 1,
        Info = 2,
        Debug = 3
    };

    struct RecentEntry {
        uint32_t ms = 0;
        uint32_t seq = 0;
        Level level = Info;
        char tag[24] = {0};
        char message[192] = {0};
    };

    static void begin(Print &output = Serial, Level level = Info);
    static void setLevel(Level level);
    static Level level();
    static void setSerialOutputEnabled(bool enabled);
    static bool serialOutputEnabled();
    static void setSensorsSerialOutputEnabled(bool enabled);
    static bool sensorsSerialOutputEnabled();
    static void log(Level level, const char *tag, const char *fmt, ...);
    // Preserve serial output, full recent diagnostics, and event mirroring,
    // but omit an expected condition from the user-facing alert buffer.
    // Error-level entries are always retained as alerts.
    static void logWithoutAlert(Level level, const char *tag, const char *fmt, ...);
    static size_t copyRecent(RecentEntry *out, size_t max_entries);
    static size_t copyRecentAlerts(RecentEntry *out, size_t max_entries);
    static uint32_t latestRecentAlertSeq();

#ifdef UNIT_TEST
    static void resetRecentForTest();
#endif

private:
    static const char *levelName(Level level);
    static void vlog(Level level,
                     const char *tag,
                     const char *fmt,
                     va_list args,
                     bool allow_alert);
    static void storeRecent(Level level,
                            const char *tag,
                            const char *message,
                            bool allow_alert);

    static Print *output_;
    static Level level_;
    static bool serial_output_enabled_;
    static bool sensors_serial_output_enabled_;
    static constexpr size_t kRecentCapacity = 64;
    static constexpr size_t kRecentAlertCapacity = 32;
    static RecentEntry recent_[kRecentCapacity];
    static size_t recent_head_;
    static size_t recent_count_;
    static RecentEntry recent_alerts_[kRecentAlertCapacity];
    static size_t recent_alert_head_;
    static size_t recent_alert_count_;
    static uint32_t recent_alert_seq_;
};

#define LOGE(tag, fmt, ...) Logger::log(Logger::Error, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) Logger::log(Logger::Warn, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) Logger::log(Logger::Info, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) Logger::log(Logger::Debug, tag, fmt, ##__VA_ARGS__)
