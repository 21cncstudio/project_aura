#pragma once
#include <Arduino.h>
#include <stdarg.h>
// Lab replacement: records what production logs so scenarios can assert on it.
class Logger {
public:
    enum Level { Error = 0, Warn = 1, Info = 2, Debug = 3 };
    static void log(Level level, const char *tag, const char *fmt, ...);
};
#define LOGE(tag, ...) Logger::log(Logger::Error, tag, __VA_ARGS__)
#define LOGW(tag, ...) Logger::log(Logger::Warn, tag, __VA_ARGS__)
#define LOGI(tag, ...) Logger::log(Logger::Info, tag, __VA_ARGS__)
#define LOGD(tag, ...) Logger::log(Logger::Debug, tag, __VA_ARGS__)
