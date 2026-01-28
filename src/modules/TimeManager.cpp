// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/TimeManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "core/Logger.h"

void TimeManager::begin(StorageManager &storage) {
    storage_ = &storage;
    const auto &cfg = storage.config();
    ntp_enabled_pref_ = cfg.ntp_enabled;
    ntp_enabled_ = ntp_enabled_pref_;
    tz_index_ = cfg.tz_index;
    if (tz_index_ < 0) {
        tz_index_ = findTimezoneIndex("Europe/London");
    }
    applyTimezone();
}

bool TimeManager::initRtc() {
    rtc_present_ = false;
    rtc_valid_ = false;
    rtc_lost_power_ = false;

    rtc_.begin();
    delay(500);
    tm utc_tm = {};
    bool osc_stop = false;
    bool time_valid = false;
    bool read_ok = false;
    for (uint8_t attempt = 0; attempt < Config::RTC_INIT_ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            delay(Config::RTC_INIT_RETRY_MS);
            LOGD("RTC", "retry %u", attempt);
        }
        if (!rtc_.readTime(utc_tm, osc_stop, time_valid)) {
            continue;
        }
        read_ok = true;
        if (!osc_stop && time_valid) {
            break;
        }
    }
    if (!read_ok) {
        return false;
    }
    rtc_present_ = true;
    rtc_lost_power_ = osc_stop;
    if (!time_valid) {
        rtc_valid_ = false;
        return false;
    }
    time_t epoch = makeUtcEpoch(utc_tm);
    if (epoch > Config::TIME_VALID_EPOCH) {
        if (osc_stop) {
            if (rtc_.clearOscillatorStop()) {
                rtc_lost_power_ = false;
            } else {
                LOGW("RTC", "failed to clear OS bit");
            }
        }
        rtc_valid_ = true;
        setSystemTime(epoch);
        return true;
    }
    rtc_valid_ = false;
    return false;
}

bool TimeManager::updateWifiState(bool wifi_enabled, bool wifi_connected) {
    wifi_enabled_ = wifi_enabled;
    wifi_connected_ = wifi_connected;
    return syncNtpWithWifi();
}

bool TimeManager::setNtpEnabledPref(bool enabled) {
    if (enabled == ntp_enabled_pref_) {
        return false;
    }
    ntp_enabled_pref_ = enabled;
    if (storage_) {
        storage_->config().ntp_enabled = ntp_enabled_pref_;
        storage_->saveConfig(true);
    }
    return syncNtpWithWifi();
}

TimeManager::PollResult TimeManager::poll(uint32_t now_ms) {
    return ntpPoll(now_ms);
}

TimeManager::NtpUiState TimeManager::getNtpUiState(uint32_t now_ms) const {
    if (!ntp_enabled_) {
        return NTP_UI_OFF;
    }
    if (ntp_syncing_) {
        return NTP_UI_SYNCING;
    }
    if (!wifi_connected_) {
        return NTP_UI_OFF;
    }
    if (ntp_last_sync_ms_ != 0 && (now_ms - ntp_last_sync_ms_) < Config::NTP_FRESH_MS) {
        return NTP_UI_OK;
    }
    if (ntp_err_ || ntp_last_sync_ms_ == 0) {
        return NTP_UI_ERR;
    }
    return NTP_UI_ERR;
}

bool TimeManager::isManualLocked(uint32_t now_ms) const {
    NtpUiState state = getNtpUiState(now_ms);
    return (state == NTP_UI_OK || state == NTP_UI_SYNCING);
}

bool TimeManager::setLocalTime(int year, int month, int day, int hour, int minute) {
    tm local_tm = {};
    local_tm.tm_year = year - 1900;
    local_tm.tm_mon = month - 1;
    local_tm.tm_mday = day;
    local_tm.tm_hour = hour;
    local_tm.tm_min = minute;
    local_tm.tm_sec = 0;
    local_tm.tm_isdst = -1;
    time_t epoch = mktime(&local_tm);
    if (epoch == -1) {
        return false;
    }
    if (!setSystemTime(epoch)) {
        return false;
    }
    rtcWriteFromEpoch(epoch);
    ntp_err_ = false;
    ntp_last_sync_ms_ = 0;
    return true;
}

bool TimeManager::setTimezoneIndex(int index) {
    int clamped = index;
    if (clamped < 0 || clamped >= static_cast<int>(TIME_ZONE_COUNT)) {
        clamped = 0;
    }
    bool changed = (clamped != tz_index_);
    tz_index_ = clamped;
    applyTimezone();
    if (storage_) {
        storage_->config().tz_index = tz_index_;
        storage_->saveConfig(true);
    }
    return changed;
}

bool TimeManager::adjustTimezone(int delta) {
    if (TIME_ZONE_COUNT == 0) {
        return false;
    }
    int count = static_cast<int>(TIME_ZONE_COUNT);
    int next = tz_index_ + delta;
    next %= count;
    if (next < 0) {
        next += count;
    }
    return setTimezoneIndex(next);
}

const TimeZoneEntry &TimeManager::getTimezone() const {
    int idx = tz_index_;
    if (idx < 0 || idx >= static_cast<int>(TIME_ZONE_COUNT)) {
        idx = 0;
    }
    return kTimeZones[idx];
}

bool TimeManager::isSystemTimeValid() const {
    time_t now = time(nullptr);
    return now > Config::TIME_VALID_EPOCH;
}

bool TimeManager::getLocalTime(tm &out) {
    time_t now = time(nullptr);
    if (now <= Config::TIME_VALID_EPOCH && rtc_present_) {
        uint32_t now_ms = millis();
        if (now_ms - last_rtc_restore_ms_ >= Config::RTC_RESTORE_INTERVAL_MS) {
            last_rtc_restore_ms_ = now_ms;
            tm utc_tm = {};
            bool osc_stop = false;
            bool time_valid = false;
            if (rtc_.readTime(utc_tm, osc_stop, time_valid)) {
                rtc_lost_power_ = osc_stop;
                rtc_valid_ = time_valid && !osc_stop;
                if (rtc_valid_) {
                    time_t epoch = makeUtcEpoch(utc_tm);
                    if (epoch > Config::TIME_VALID_EPOCH && setSystemTime(epoch)) {
                        now = epoch;
                    }
                }
            }
        }
    }
    if (now <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    localtime_r(&now, &out);
    return true;
}

bool TimeManager::syncInputsFromSystem(int &hour, int &minute, int &day, int &month, int &year) {
    tm local_tm = {};
    if (!getLocalTime(local_tm)) {
        hour = 0;
        minute = 0;
        day = 1;
        month = 1;
        year = 2026;
        return false;
    }
    hour = local_tm.tm_hour;
    minute = local_tm.tm_min;
    day = local_tm.tm_mday;
    month = local_tm.tm_mon + 1;
    year = local_tm.tm_year + 1900;
    return true;
}

int TimeManager::findTimezoneIndex(const char *name) {
    if (!name) {
        return 0;
    }
    for (size_t i = 0; i < TIME_ZONE_COUNT; i++) {
        if (strcmp(kTimeZones[i].name, name) == 0) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void TimeManager::formatTzOffset(int offset_min, char *out, size_t len) {
    int abs_min = abs(offset_min);
    int hours = abs_min / 60;
    int mins = abs_min % 60;
    char sign = offset_min >= 0 ? '+' : '-';
    snprintf(out, len, "%c%02d:%02d", sign, hours, mins);
}

bool TimeManager::isLeapYear(int year) {
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return (year % 4) == 0;
}

int TimeManager::daysInMonth(int year, int month) {
    static const int kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 31;
    if (month == 2 && isLeapYear(year)) return 29;
    return kDays[month - 1];
}

void TimeManager::applyTimezone() {
    if (tz_index_ < 0 || tz_index_ >= static_cast<int>(TIME_ZONE_COUNT)) {
        tz_index_ = 0;
    }
    const TimeZoneEntry &tz = kTimeZones[tz_index_];
    char fixed_tz[24] = { 0 };
    const char *posix = tz.posix;
    if (!posix || !posix[0]) {
        buildFixedTzString(tz.offset_min, fixed_tz, sizeof(fixed_tz));
        posix = fixed_tz;
    }
    setenv("TZ", posix, 1);
    tzset();
}

void TimeManager::buildFixedTzString(int offset_min, char *out, size_t len) {
    int abs_min = abs(offset_min);
    int hours = abs_min / 60;
    int mins = abs_min % 60;
    char sign = offset_min >= 0 ? '-' : '+';
    if (mins == 0) {
        snprintf(out, len, "UTC%c%d", sign, hours);
    } else {
        snprintf(out, len, "UTC%c%d:%02d", sign, hours, mins);
    }
}

time_t TimeManager::makeUtcEpoch(const tm &utc_tm) {
    setenv("TZ", "UTC0", 1);
    tzset();
    tm tmp = utc_tm;
    time_t t = mktime(&tmp);
    applyTimezone();
    return t;
}

bool TimeManager::setSystemTime(time_t epoch) {
    timeval tv = {};
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    return settimeofday(&tv, nullptr) == 0;
}

bool TimeManager::rtcWriteFromEpoch(time_t epoch) {
    if (!rtc_present_) {
        return false;
    }
    tm utc_tm = {};
    gmtime_r(&epoch, &utc_tm);
    if (!rtc_.writeTime(utc_tm)) {
        return false;
    }
    rtc_valid_ = true;
    rtc_lost_power_ = false;
    return true;
}

bool TimeManager::requestNtpSync() {
    if (!ntp_enabled_ || !wifi_connected_) {
        return false;
    }
    if (ntp_syncing_) {
        return false;
    }
    ntp_syncing_ = true;
    ntp_err_ = false;
    ntp_sync_start_ms_ = millis();
    ntp_last_attempt_ms_ = ntp_sync_start_ms_;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    return true;
}

bool TimeManager::syncNtpWithWifi() {
    bool desired = ntp_enabled_pref_;
    bool effective = wifi_enabled_ && desired;
    if (effective == ntp_enabled_) {
        if (!effective) {
            ntp_syncing_ = false;
            ntp_err_ = false;
        }
        return false;
    }
    ntp_enabled_ = effective;
    if (!ntp_enabled_) {
        ntp_syncing_ = false;
        ntp_err_ = false;
    } else {
        requestNtpSync();
    }
    return true;
}

TimeManager::PollResult TimeManager::ntpPoll(uint32_t now_ms) {
    PollResult result;
    if (!ntp_enabled_ || !wifi_connected_) {
        if (ntp_syncing_) {
            ntp_syncing_ = false;
            result.state_changed = true;
        }
        return result;
    }

    if (ntp_syncing_) {
        tm info = {};
        if (::getLocalTime(&info, 10)) {
            time_t epoch = time(nullptr);
            if (epoch > Config::TIME_VALID_EPOCH) {
                ntp_syncing_ = false;
                ntp_err_ = false;
                ntp_last_sync_ms_ = now_ms;
                rtcWriteFromEpoch(epoch);
                result.state_changed = true;
                result.time_updated = true;
                return result;
            }
        }
        if (now_ms - ntp_sync_start_ms_ > Config::NTP_SYNC_TIMEOUT_MS) {
            ntp_syncing_ = false;
            ntp_err_ = true;
            result.state_changed = true;
        }
        return result;
    }

    if (ntp_last_sync_ms_ == 0) {
        if (ntp_last_attempt_ms_ == 0 || (now_ms - ntp_last_attempt_ms_) >= Config::NTP_RETRY_MS) {
            if (requestNtpSync()) {
                result.state_changed = true;
            }
        }
    } else if ((now_ms - ntp_last_sync_ms_) >= Config::NTP_SYNC_INTERVAL_MS) {
        if (requestNtpSync()) {
            result.state_changed = true;
        }
    }
    return result;
}
