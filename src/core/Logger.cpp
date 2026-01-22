#include "core/Logger.h"

#include <stdio.h>

namespace {
constexpr size_t kLogBufferSize = 256;
}

HardwareSerial *Logger::serial_ = &Serial;
Logger::Level Logger::level_ = Logger::Info;

void Logger::begin(HardwareSerial &serial, Level level) {
    serial_ = &serial;
    level_ = level;
}

void Logger::setLevel(Level level) {
    level_ = level;
}

Logger::Level Logger::level() {
    return level_;
}

const char *Logger::levelName(Level level) {
    switch (level) {
        case Error:
            return "E";
        case Warn:
            return "W";
        case Info:
            return "I";
        case Debug:
            return "D";
        default:
            return "?";
    }
}

void Logger::log(Level level, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(level, tag, fmt, args);
    va_end(args);
}

void Logger::vlog(Level level, const char *tag, const char *fmt, va_list args) {
    if (level > level_ || serial_ == nullptr) {
        return;
    }

    char buffer[kLogBufferSize];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    serial_->print('[');
    serial_->print(levelName(level));
    serial_->print(']');
    if (tag && tag[0] != '\0') {
        serial_->print('[');
        serial_->print(tag);
        serial_->print(']');
    }
    serial_->print(' ');
    serial_->println(buffer);
}
