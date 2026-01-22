// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stdarg.h>

class Logger {
public:
    enum Level {
        Error = 0,
        Warn = 1,
        Info = 2,
        Debug = 3
    };

    static void begin(HardwareSerial &serial = Serial, Level level = Info);
    static void setLevel(Level level);
    static Level level();
    static void log(Level level, const char *tag, const char *fmt, ...);

private:
    static const char *levelName(Level level);
    static void vlog(Level level, const char *tag, const char *fmt, va_list args);

    static HardwareSerial *serial_;
    static Level level_;
};

#define LOGE(tag, fmt, ...) Logger::log(Logger::Error, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) Logger::log(Logger::Warn, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) Logger::log(Logger::Info, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) Logger::log(Logger::Debug, tag, fmt, ##__VA_ARGS__)
