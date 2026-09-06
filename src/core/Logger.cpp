// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/Logger.h"
#include "core/MqttEventQueue.h"
#include "core/SystemEventPolicy.h"
#include "core/SystemLogFilter.h"

#include <stdio.h>
#include <string.h>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace {
constexpr size_t kLogBufferSize = 256;
constexpr uint32_t kRecentDedupWindowMs = 30000;

class LoggerMutex {
public:
#ifndef UNIT_TEST
    LoggerMutex()
        : mutex_(xSemaphoreCreateMutexStatic(&mutex_buffer_)) {}
#endif

    void lock() {
#ifdef UNIT_TEST
        mutex_.lock();
#else
        xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    }

    void unlock() {
#ifdef UNIT_TEST
        mutex_.unlock();
#else
        xSemaphoreGive(mutex_);
#endif
    }

private:
#ifdef UNIT_TEST
    std::mutex mutex_{};
#else
    StaticSemaphore_t mutex_buffer_{};
    SemaphoreHandle_t mutex_ = nullptr;
#endif
};

LoggerMutex &recentBufferMutex() {
    // Function-local construction keeps the FreeRTOS mutex out of static
    // initialization and initializes it on the first Logger buffer access.
    static LoggerMutex mutex;
    return mutex;
}

LoggerMutex &serialOutputMutex() {
    // Keep potentially blocking Print calls out of the recent-buffer critical
    // section while still emitting each logical serial line atomically.
    static LoggerMutex mutex;
    return mutex;
}

class LoggerLock {
public:
    explicit LoggerLock(LoggerMutex &mutex)
        : mutex_(mutex) {
        mutex_.lock();
    }

    ~LoggerLock() {
        mutex_.unlock();
    }

    LoggerLock(const LoggerLock &) = delete;
    LoggerLock &operator=(const LoggerLock &) = delete;

private:
    LoggerMutex &mutex_;
};

bool storeRecentInBuffer(Logger::RecentEntry *buffer,
                         size_t capacity,
                         size_t &head,
                         size_t &count,
                         Logger::Level level,
                         const char *tag,
                         const char *message,
                         uint32_t now_ms,
                         uint32_t seq = 0,
                         bool refresh_on_dedup = false) {
    if (!buffer || capacity == 0) {
        return false;
    }

    if (count > 0) {
        const size_t last_index = (head + capacity - 1) % capacity;
        Logger::RecentEntry &last = buffer[last_index];
        const bool same_event =
            last.level == level &&
            strcmp(last.tag, tag) == 0 &&
            strcmp(last.message, message) == 0;
        const bool within_dedup_window = (now_ms - last.ms) <= kRecentDedupWindowMs;
        if (same_event && within_dedup_window) {
            if (refresh_on_dedup) {
                last.ms = now_ms;
                last.seq = seq;
                return true;
            }
            return false;
        }
    }

    Logger::RecentEntry &entry = buffer[head];
    entry.ms = now_ms;
    entry.seq = seq;
    entry.level = level;
    strncpy(entry.tag, tag, sizeof(entry.tag) - 1);
    entry.tag[sizeof(entry.tag) - 1] = '\0';
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';

    head = (head + 1) % capacity;
    if (count < capacity) {
        count++;
    }
    return true;
}

size_t copyRecentFromBuffer(const Logger::RecentEntry *buffer,
                            size_t capacity,
                            size_t head,
                            size_t count,
                            Logger::RecentEntry *out,
                            size_t max_entries) {
    if (!buffer || !out || max_entries == 0 || count == 0 || capacity == 0) {
        return 0;
    }

    const size_t to_copy = (count < max_entries) ? count : max_entries;
    const size_t start = (head + capacity - to_copy) % capacity;
    for (size_t i = 0; i < to_copy; ++i) {
        out[i] = buffer[(start + i) % capacity];
    }
    return to_copy;
}
}

Print *Logger::output_ = &Serial;
Logger::Level Logger::level_ = Logger::Info;
bool Logger::serial_output_enabled_ = true;
bool Logger::sensors_serial_output_enabled_ = true;
Logger::RecentEntry Logger::recent_[Logger::kRecentCapacity];
size_t Logger::recent_head_ = 0;
size_t Logger::recent_count_ = 0;
Logger::RecentEntry Logger::recent_alerts_[Logger::kRecentAlertCapacity];
size_t Logger::recent_alert_head_ = 0;
size_t Logger::recent_alert_count_ = 0;
uint32_t Logger::recent_alert_seq_ = 0;

void Logger::begin(Print &output, Level level) {
    output_ = &output;
    level_ = level;
}

void Logger::setLevel(Level level) {
    level_ = level;
}

Logger::Level Logger::level() {
    return level_;
}

void Logger::setSerialOutputEnabled(bool enabled) {
    serial_output_enabled_ = enabled;
}

bool Logger::serialOutputEnabled() {
    return serial_output_enabled_;
}

void Logger::setSensorsSerialOutputEnabled(bool enabled) {
    sensors_serial_output_enabled_ = enabled;
}

bool Logger::sensorsSerialOutputEnabled() {
    return sensors_serial_output_enabled_;
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
    vlog(level, tag, fmt, args, true);
    va_end(args);
}

void Logger::logWithoutAlert(Level level, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(level, tag, fmt, args, false);
    va_end(args);
}

void Logger::vlog(Level level,
                  const char *tag,
                  const char *fmt,
                  va_list args,
                  bool allow_alert) {
    if (level > level_) {
        return;
    }

    char buffer[kLogBufferSize];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    bool print_to_serial = (output_ && serial_output_enabled_);
    if (print_to_serial && !sensors_serial_output_enabled_ && tag && strcmp(tag, "Sensors") == 0) {
        print_to_serial = false;
    }

    if (print_to_serial) {
        LoggerLock lock(serialOutputMutex());
        // Emit all fragments while holding one output-only lock so another
        // task cannot splice its level/tag/message into this logical line.
        if (output_ && serial_output_enabled_ &&
            (sensors_serial_output_enabled_ || !tag || strcmp(tag, "Sensors") != 0)) {
            output_->print('[');
            output_->print(levelName(level));
            output_->print(']');
            if (tag && tag[0] != '\0') {
                output_->print('[');
                output_->print(tag);
                output_->print(']');
            }
            output_->print(' ');
            output_->println(buffer);
        }
    }

    storeRecent(level, tag, buffer, allow_alert);
}

void Logger::storeRecent(Level level,
                         const char *tag,
                         const char *message,
                         bool allow_alert) {
    uint32_t now_ms = 0;
#if defined(ARDUINO)
    now_ms = millis();
#endif

    char tag_buf[sizeof(RecentEntry::tag)];
    char message_buf[sizeof(RecentEntry::message)];
    tag_buf[0] = '\0';
    message_buf[0] = '\0';

    if (tag) {
        strncpy(tag_buf, tag, sizeof(tag_buf) - 1);
        tag_buf[sizeof(tag_buf) - 1] = '\0';
    }
    if (message) {
        strncpy(message_buf, message, sizeof(message_buf) - 1);
        message_buf[sizeof(message_buf) - 1] = '\0';
    }

    RecentEntry event_entry{};
    event_entry.ms = now_ms;
    event_entry.level = level;
    strncpy(event_entry.tag, tag_buf, sizeof(event_entry.tag) - 1);
    event_entry.tag[sizeof(event_entry.tag) - 1] = '\0';
    strncpy(event_entry.message, message_buf, sizeof(event_entry.message) - 1);
    event_entry.message[sizeof(event_entry.message) - 1] = '\0';

    const bool emit_event = SystemEventPolicy::shouldEmit(event_entry);
    const bool store_alert =
        (allow_alert || level == Error) &&
        SystemLogFilter::shouldStoreAlert(level, tag_buf, message_buf);

    bool stored_recent = false;
    {
        LoggerLock lock(recentBufferMutex());
        stored_recent =
            storeRecentInBuffer(recent_, kRecentCapacity, recent_head_, recent_count_,
                                level, tag_buf, message_buf, now_ms);

        if (store_alert) {
            uint32_t next_alert_seq = recent_alert_seq_ + 1;
            if (next_alert_seq == 0) {
                next_alert_seq = 1;
            }
            if (storeRecentInBuffer(recent_alerts_,
                                    kRecentAlertCapacity,
                                    recent_alert_head_,
                                    recent_alert_count_,
                                    level,
                                    tag_buf,
                                    message_buf,
                                    now_ms,
                                    next_alert_seq,
                                    true)) {
                recent_alert_seq_ = next_alert_seq;
            }
        }
    }

    if (stored_recent && emit_event) {
        MqttEventQueue::instance().enqueueIfCapturing(event_entry);
    }
}

size_t Logger::copyRecent(RecentEntry *out, size_t max_entries) {
    LoggerLock lock(recentBufferMutex());
    return copyRecentFromBuffer(recent_, kRecentCapacity, recent_head_, recent_count_, out, max_entries);
}

size_t Logger::copyRecentAlerts(RecentEntry *out, size_t max_entries) {
    LoggerLock lock(recentBufferMutex());
    return copyRecentFromBuffer(recent_alerts_, kRecentAlertCapacity,
                                recent_alert_head_, recent_alert_count_, out, max_entries);
}

uint32_t Logger::latestRecentAlertSeq() {
    LoggerLock lock(recentBufferMutex());
    return recent_alert_seq_;
}

#ifdef UNIT_TEST
void Logger::resetRecentForTest() {
    LoggerLock lock(recentBufferMutex());
    memset(recent_, 0, sizeof(recent_));
    memset(recent_alerts_, 0, sizeof(recent_alerts_));
    recent_head_ = 0;
    recent_count_ = 0;
    recent_alert_head_ = 0;
    recent_alert_count_ = 0;
    recent_alert_seq_ = 0;
}
#endif
