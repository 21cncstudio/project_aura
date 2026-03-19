// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/TimeManager.h"

#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include "core/Logger.h"
// [NEW] Included only for the real firmware build — TimeManager is also
// compiled in native unit tests which have no WiFi.h / MqttManager.
#ifndef UNIT_TEST
#include "modules/MqttManager.h"
#endif
#ifdef UNIT_TEST
#include "TimeMock.h"
#endif

namespace {

time_t nowEpoch() {
#ifdef UNIT_TEST
    return mockNow();
#else
    return time(nullptr);
#endif
}

bool setTimezoneEnv(const char *tz) {
#ifdef _WIN32
    return _putenv_s("TZ", tz ? tz : "") == 0;
#else
    return setenv("TZ", tz ? tz : "", 1) == 0;
#endif
}

bool gmtimeInto(const time_t &epoch, tm &out) {
#ifdef _WIN32
    return gmtime_s(&out, &epoch) == 0;
#else
    return gmtime_r(&epoch, &out) != nullptr;
#endif
}

bool localtimeInto(const time_t &epoch, tm &out) {
#ifdef _WIN32
    return localtime_s(&out, &epoch) == 0;
#else
    return localtime_r(&epoch, &out) != nullptr;
#endif
}

bool setSystemEpoch(time_t epoch) {
#ifdef UNIT_TEST
    setNowEpoch(epoch);
    return true;
#elif defined(_WIN32)
    (void)epoch;
    return false;
#else
    timeval tv = {};
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    return settimeofday(&tv, nullptr) == 0;
#endif
}

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const int adjusted_month = static_cast<int>(month) + (month > 2 ? -3 : 9);
    const unsigned day_of_year =
        (153U * static_cast<unsigned>(adjusted_month) + 2U) / 5U + day - 1U;
    const unsigned day_of_era =
        year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return static_cast<int64_t>(era) * 146097LL + static_cast<int64_t>(day_of_era) - 719468LL;
}

time_t utcTmToEpoch(const tm &utc_tm) {
    const int year = utc_tm.tm_year + 1900;
    const int month = utc_tm.tm_mon + 1;
    const int day = utc_tm.tm_mday;
    if (month < 1 || month > 12) {
        return static_cast<time_t>(-1);
    }
    if (day < 1 || day > TimeManager::daysInMonth(year, month)) {
        return static_cast<time_t>(-1);
    }
    if (utc_tm.tm_hour < 0 || utc_tm.tm_hour > 23 ||
        utc_tm.tm_min < 0 || utc_tm.tm_min > 59 ||
        utc_tm.tm_sec < 0 || utc_tm.tm_sec > 59) {
        return static_cast<time_t>(-1);
    }

    const int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    const int64_t seconds =
        days * 86400LL +
        static_cast<int64_t>(utc_tm.tm_hour) * 3600LL +
        static_cast<int64_t>(utc_tm.tm_min) * 60LL +
        static_cast<int64_t>(utc_tm.tm_sec);
    if (seconds < static_cast<int64_t>(std::numeric_limits<time_t>::min()) ||
        seconds > static_cast<int64_t>(std::numeric_limits<time_t>::max())) {
        return static_cast<time_t>(-1);
    }
    return static_cast<time_t>(seconds);
}

} // namespace

const char *TimeManager::rtcLabel() const {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return Pcf8523::label();
        case RtcType::Ds3231:
            return Ds3231::label();
        default:
            return "RTC";
    }
}

const char *TimeManager::rtcModeLabel(Config::RtcMode mode) {
    switch (mode) {
        case Config::RtcMode::Pcf8523:
            return Pcf8523::label();
        case Config::RtcMode::Ds3231:
            return Ds3231::label();
        default:
            return "Auto";
    }
}

void TimeManager::begin(StorageManager &storage) {
    storage_ = &storage;
    const auto &cfg = storage.config();
    ntp_enabled_pref_ = cfg.ntp_enabled;
    ntp_enabled_ = ntp_enabled_pref_;
    tz_index_ = cfg.tz_index;
    rtc_mode_ = Config::clampRtcMode(static_cast<int>(cfg.rtc_mode));
    if (tz_index_ < 0) {
        tz_index_ = findTimezoneIndex("Europe/London");
    }
    applyTimezone();
}

bool TimeManager::initRtc() {
    rtc_type_ = RtcType::None;
    rtc_present_ = false;
    rtc_valid_ = false;
    rtc_lost_power_ = false;
    rtc_battery_low_ = false;
    rtc_probe_needs_pcf_verification_ = false;
    last_rtc_status_poll_ms_ = 0;
    rtc_read_fail_count_ = 0;

    if (!detectRtc()) {
        return false;
    }
    rtc_present_ = true;
    if (!rtcBegin()) {
        LOGW("RTC", "%s init failed", rtcLabel());
        // [NEW] RTC hardware failed to initialise — mirrors "RTC init failed" in Events tab.
#ifndef UNIT_TEST
        {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s init failed", rtcLabel());
            MqttPublishSystemEvent("RTC", "warning", msg);
        }
#endif
        return false;
    }
    delay(500);
    tm utc_tm = {};
    bool osc_stop = false;
    bool time_valid = false;
    bool read_ok = readRtcInitState(utc_tm, osc_stop, time_valid);
    if (rtc_mode_ == Config::RtcMode::Auto &&
        rtc_type_ == RtcType::Ds3231 &&
        rtc_probe_needs_pcf_verification_ &&
        (!read_ok || (!osc_stop && !time_valid))) {
        if (retryWeakDs3231AsPcf8523(utc_tm, osc_stop, time_valid)) {
            read_ok = true;
        }
    }
    if (!read_ok) {
        return false;
    }
    rtc_lost_power_ = osc_stop;
    bool battery_low = false;
    if (rtcReadBatteryLow(battery_low)) {
        applyRtcBatteryLowState(battery_low, true);
    }
    last_rtc_status_poll_ms_ = millis();
    if (!time_valid) {
        rtc_valid_ = false;
        return false;
    }
    time_t epoch = makeUtcEpoch(utc_tm);
    if (epoch > Config::TIME_VALID_EPOCH) {
        if (osc_stop) {
            if (rtcClearLostPower()) {
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

bool TimeManager::readRtcInitState(tm &utc_tm, bool &osc_stop, bool &time_valid) {
    bool read_ok = false;
    for (uint8_t attempt = 0; attempt < Config::RTC_INIT_ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            delay(Config::RTC_INIT_RETRY_MS);
            LOGD("RTC", "retry %u", attempt);
        }
        if (!rtcReadTime(utc_tm, osc_stop, time_valid)) {
            continue;
        }
        read_ok = true;
        noteRtcReadSuccess(false);
        if (!osc_stop && time_valid) {
            break;
        }
    }
    return read_ok;
}

bool TimeManager::retryWeakDs3231AsPcf8523(tm &utc_tm, bool &osc_stop, bool &time_valid) {
    if (!pcf8523_.probeFallback()) {
        return false;
    }

    LOGW("RTC", "weak DS3231 probe did not validate, retrying as %s", Pcf8523::label());
    rtc_type_ = RtcType::Pcf8523;
    rtc_probe_needs_pcf_verification_ = false;

    if (!rtcBegin()) {
        LOGW("RTC", "%s init failed after weak DS3231 retry", rtcLabel());
        return false;
    }
    delay(500);
    return readRtcInitState(utc_tm, osc_stop, time_valid);
}

bool TimeManager::updateWifiState(bool wifi_enabled, bool wifi_connected) {
    bool was_connected = wifi_connected_;
    wifi_enabled_ = wifi_enabled;
    wifi_connected_ = wifi_connected;
    bool state_changed = syncNtpWithWifi();
    if (!was_connected && wifi_connected_ && ntp_enabled_ && !ntp_syncing_) {
        if (requestNtpSync()) {
            state_changed = true;
        }
    }
    return state_changed;
}

bool TimeManager::setNtpEnabledPref(bool enabled) {
    if (enabled == ntp_enabled_pref_) {
        return false;
    }
    ntp_enabled_pref_ = enabled;
    if (storage_) {
        storage_->config().ntp_enabled = ntp_enabled_pref_;
        if (!storage_->saveConfig(true)) {
            storage_->requestSave();
            LOGE("Time", "failed to persist NTP preference");
        }
    }
    return syncNtpWithWifi();
}

TimeManager::PollResult TimeManager::poll(uint32_t now_ms) {
    PollResult result = ntpPoll(now_ms);
    PollResult rtc_result = pollRtcStatus(now_ms);
    result.state_changed = result.state_changed || rtc_result.state_changed;
    result.time_updated = result.time_updated || rtc_result.time_updated;
    return result;
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
        if (!storage_->saveConfig(true)) {
            storage_->requestSave();
            LOGE("Time", "failed to persist timezone index");
        }
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

int TimeManager::currentUtcOffsetMinutes() const {
    time_t now = nowEpoch();
    if (now <= Config::TIME_VALID_EPOCH) {
        return getTimezone().offset_min;
    }
    tm utc_tm = {};
    if (!gmtimeInto(now, utc_tm)) {
        return getTimezone().offset_min;
    }
    utc_tm.tm_isdst = -1;
    time_t utc_as_local = mktime(&utc_tm);
    if (utc_as_local == static_cast<time_t>(-1)) {
        return getTimezone().offset_min;
    }
    long diff_sec = static_cast<long>(difftime(now, utc_as_local));
    return static_cast<int>(diff_sec / 60L);
}

bool TimeManager::isSystemTimeValid() const {
    time_t now = nowEpoch();
    return now > Config::TIME_VALID_EPOCH;
}

bool TimeManager::getLocalTime(tm &out) {
    time_t now = nowEpoch();
    if (now <= Config::TIME_VALID_EPOCH && rtc_present_) {
        uint32_t now_ms = millis();
        if (now_ms - last_rtc_restore_ms_ >= Config::RTC_RESTORE_INTERVAL_MS) {
            last_rtc_restore_ms_ = now_ms;
            tm utc_tm = {};
            bool osc_stop = false;
            bool time_valid = false;
            if (rtcReadTime(utc_tm, osc_stop, time_valid)) {
                noteRtcReadSuccess(false);
                rtc_lost_power_ = osc_stop;
                rtc_valid_ = time_valid && !osc_stop;
                if (rtc_valid_) {
                    time_t epoch = makeUtcEpoch(utc_tm);
                    if (epoch > Config::TIME_VALID_EPOCH && setSystemTime(epoch)) {
                        now = epoch;
                    }
                }
            } else {
                noteRtcReadFailure(false);
            }
        }
    }
    if (now <= Config::TIME_VALID_EPOCH) {
        return false;
    }
    return localtimeInto(now, out);
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
    char posix_tz[32] = { 0 };
    buildTimezonePosix(tz, posix_tz, sizeof(posix_tz));
    setTimezoneEnv(posix_tz);
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
    return utcTmToEpoch(utc_tm);
}

bool TimeManager::setSystemTime(time_t epoch) {
    return setSystemEpoch(epoch);
}

bool TimeManager::rtcWriteFromEpoch(time_t epoch) {
    if (!rtc_present_) {
        return false;
    }
    tm utc_tm = {};
    if (!gmtimeInto(epoch, utc_tm)) {
        return false;
    }
    if (!rtcWriteTime(utc_tm)) {
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
    const TimeZoneEntry &tz = getTimezone();
    char posix_tz[32] = { 0 };
    buildTimezonePosix(tz, posix_tz, sizeof(posix_tz));
    LOGI("Time", "NTP sync start (tz=%s, wifi=ON)", tz.name ? tz.name : "unknown");
    #ifndef UNIT_TEST
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "NTP sync start (tz=%s)", tz.name ? tz.name : "unknown");
        MqttPublishSystemEvent("TIME", "info", msg);
    }
    #endif
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(posix_tz, "pool.ntp.org", "time.nist.gov", "time.google.com");
    return true;
}

bool TimeManager::syncNtpWithWifi() {
    bool desired = ntp_enabled_pref_;
    bool effective = wifi_enabled_ && desired;
    if (effective == ntp_enabled_) {
        if (!effective) {
            stopNtpService();
            ntp_syncing_ = false;
            ntp_err_ = false;
        }
        return false;
    }
    ntp_enabled_ = effective;
    if (!ntp_enabled_) {
        if (ntp_syncing_) {
            LOGW("Time", "NTP sync canceled (WiFi disabled/disconnected)");
        }
        stopNtpService();
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
            LOGW("Time", "NTP sync canceled while waiting for network");
            ntp_syncing_ = false;
            result.state_changed = true;
        }
        return result;
    }

    if (ntp_syncing_) {
        sntp_sync_status_t sync_status = sntp_get_sync_status();
        if (sync_status == SNTP_SYNC_STATUS_COMPLETED) {
            time_t epoch = nowEpoch();
            if (epoch > Config::TIME_VALID_EPOCH) {
                tm local_tm = {};
                if (!localtimeInto(epoch, local_tm)) {
                    return result;
                }
                char buf[32];
                snprintf(buf,
                         sizeof(buf),
                         "%04d-%02d-%02d %02d:%02d:%02d",
                         local_tm.tm_year + 1900,
                         local_tm.tm_mon + 1,
                         local_tm.tm_mday,
                         local_tm.tm_hour,
                         local_tm.tm_min,
                         local_tm.tm_sec);
                LOGI("Time", "NTP sync completed, local time=%s", buf);
                // [NEW] Mirrors "NTP sync completed, local time=..." in web dashboard Events tab.
#ifndef UNIT_TEST
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "NTP sync completed, local time=%s", buf);
                    MqttPublishSystemEvent("TIME", "info", msg);
                }
#endif
                ntp_syncing_ = false;
                ntp_err_ = false;
                ntp_last_sync_ms_ = now_ms;
                rtcWriteFromEpoch(epoch);
                result.state_changed = true;
                result.time_updated = true;
                return result;
            }
        }
        const uint32_t elapsed = now_ms - ntp_sync_start_ms_;
        if (static_cast<int32_t>(elapsed) > static_cast<int32_t>(Config::NTP_SYNC_TIMEOUT_MS)) {
            LOGW("Time", "NTP sync timeout after %lu ms", static_cast<unsigned long>(elapsed));
            // [NEW] NTP timeout — mirrors "NTP sync timeout" in Events tab.
#ifndef UNIT_TEST
            {
                char msg[48];
                snprintf(msg, sizeof(msg), "NTP sync timeout after %lu ms",
                         static_cast<unsigned long>(elapsed));
                MqttPublishSystemEvent("TIME", "warning", msg);
            }
#endif
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

TimeManager::PollResult TimeManager::pollRtcStatus(uint32_t now_ms) {
    PollResult result;
    if (!rtc_present_) {
        return result;
    }
    if (last_rtc_status_poll_ms_ != 0 &&
        (now_ms - last_rtc_status_poll_ms_) < Config::RTC_STATUS_POLL_MS) {
        return result;
    }
    last_rtc_status_poll_ms_ = now_ms;

    tm utc_tm = {};
    bool osc_stop = false;
    bool time_valid = false;
    if (rtcReadTime(utc_tm, osc_stop, time_valid)) {
        noteRtcReadSuccess(true);
        bool rtc_valid = false;
        if (time_valid) {
            const time_t epoch = makeUtcEpoch(utc_tm);
            rtc_valid = !osc_stop && epoch > Config::TIME_VALID_EPOCH;
        }
        if (rtc_lost_power_ != osc_stop || rtc_valid_ != rtc_valid) {
            result.state_changed = true;
        }
        rtc_lost_power_ = osc_stop;
        rtc_valid_ = rtc_valid;
        rtc_present_ = true;
    } else if (noteRtcReadFailure(true)) {
        result.state_changed = true;
    }

    bool battery_low = false;
    if (rtcReadBatteryLow(battery_low)) {
        if (applyRtcBatteryLowState(battery_low, true)) {
            result.state_changed = true;
        }
        rtc_present_ = true;
    }

    return result;
}

void TimeManager::noteRtcReadSuccess(bool log_transition) {
    const bool had_comm_fault = rtc_read_fail_count_ >= Config::RTC_STATUS_READ_FAIL_LIMIT;
    rtc_read_fail_count_ = 0;
    if (had_comm_fault && log_transition) {
        LOGI("RTC", "%s communication restored", rtcLabel());
        // [NEW] RTC comm restored after repeated failures.
#ifndef UNIT_TEST
        {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s communication restored", rtcLabel());
            MqttPublishSystemEvent("RTC", "info", msg);
        }
#endif
    }
}

bool TimeManager::noteRtcReadFailure(bool log_transition) {
    if (!rtc_present_) {
        return false;
    }
    if (rtc_read_fail_count_ < UINT8_MAX) {
        ++rtc_read_fail_count_;
    }
    if (rtc_read_fail_count_ < Config::RTC_STATUS_READ_FAIL_LIMIT) {
        return false;
    }

    const bool crossed_threshold = rtc_read_fail_count_ == Config::RTC_STATUS_READ_FAIL_LIMIT;
    if (crossed_threshold && log_transition) {
        LOGW("RTC", "%s read failed repeatedly", rtcLabel());
        // [NEW] RTC repeated read failure — matches "RTC read failed repeatedly" in Events tab.
#ifndef UNIT_TEST
        {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s read failed repeatedly", rtcLabel());
            MqttPublishSystemEvent("RTC", "warning", msg);
        }
#endif
    }

    const bool state_changed = rtc_valid_;
    rtc_valid_ = false;
    rtc_present_ = true;
    return state_changed;
}

bool TimeManager::detectRtc() {
    rtc_probe_needs_pcf_verification_ = false;

    if (rtc_mode_ == Config::RtcMode::Pcf8523) {
        if (pcf8523_.probe() || pcf8523_.probeFallback()) {
            rtc_type_ = RtcType::Pcf8523;
            LOGI("RTC", "%s selected manually at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
            return true;
        }
        rtc_type_ = RtcType::None;
        return false;
    }

    if (rtc_mode_ == Config::RtcMode::Ds3231) {
        const Ds3231::ProbeStrength probe = ds3231_.probeStrength();
        if (probe != Ds3231::ProbeStrength::None) {
            rtc_type_ = RtcType::Ds3231;
            if (probe == Ds3231::ProbeStrength::Weak) {
                LOGI("RTC", "%s weak signature at 0x%02X (manual)", rtcLabel(), Config::DS3231_ADDR);
            } else {
                LOGI("RTC", "%s selected manually at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
            }
            return true;
        }
        rtc_type_ = RtcType::None;
        return false;
    }

    if (pcf8523_.probe()) {
        rtc_type_ = RtcType::Pcf8523;
        LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
        return true;
    }

    const Ds3231::ProbeStrength ds3231_probe = ds3231_.probeStrength();
    if (ds3231_probe == Ds3231::ProbeStrength::Strong) {
        rtc_type_ = RtcType::Ds3231;
        LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
        return true;
    }

    if (pcf8523_.probeFallback()) {
        if (ds3231_probe == Ds3231::ProbeStrength::Weak) {
            rtc_type_ = RtcType::Ds3231;
            rtc_probe_needs_pcf_verification_ = true;
            LOGI("RTC", "%s weak signature at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
            return true;
        }
        rtc_type_ = RtcType::Pcf8523;
        LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
        return true;
    }
    if (ds3231_probe == Ds3231::ProbeStrength::Weak) {
        rtc_type_ = RtcType::Ds3231;
        LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
        return true;
    }
    rtc_type_ = RtcType::None;
    return false;
}

bool TimeManager::rtcBegin() {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.begin();
        case RtcType::Ds3231:
            return ds3231_.begin();
        default:
            return false;
    }
}

bool TimeManager::rtcReadTime(tm &out, bool &osc_stop, bool &valid) {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.readTime(out, osc_stop, valid);
        case RtcType::Ds3231:
            return ds3231_.readTime(out, osc_stop, valid);
        default:
            return false;
    }
}

bool TimeManager::rtcWriteTime(const tm &utc_tm) {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.writeTime(utc_tm);
        case RtcType::Ds3231:
            return ds3231_.writeTime(utc_tm);
        default:
            return false;
    }
}

bool TimeManager::rtcClearLostPower() {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.clearOscillatorStop();
        case RtcType::Ds3231:
            return ds3231_.clearOscillatorStop();
        default:
            return false;
    }
}

bool TimeManager::rtcReadBatteryLow(bool &low) {
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.isBatteryLow(low);
        case RtcType::Ds3231:
            return ds3231_.isBatteryLow(low);
        default:
            low = false;
            return false;
    }
}

bool TimeManager::applyRtcBatteryLowState(bool battery_low, bool log_transition) {
    if (rtc_battery_low_ == battery_low) {
        return false;
    }
    const bool had_low = rtc_battery_low_;
    rtc_battery_low_ = battery_low;
    if (log_transition) {
        if (battery_low) {
            LOGW("RTC", "battery low");
            // [NEW] RTC battery low — useful for maintenance reminders in HA.
#ifndef UNIT_TEST
            MqttPublishSystemEvent("RTC", "warning", "RTC battery low");
#endif
        } else if (had_low) {
            LOGI("RTC", "battery status OK");
            // [NEW] Battery replaced / recovered.
#ifndef UNIT_TEST
            MqttPublishSystemEvent("RTC", "info", "RTC battery status OK");
#endif
        }
    }
    return true;
}

void TimeManager::stopNtpService() {
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
}

void TimeManager::buildTimezonePosix(const TimeZoneEntry &tz, char *out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    out[0] = '\0';
    if (tz.posix && tz.posix[0]) {
        snprintf(out, len, "%s", tz.posix);
        return;
    }
    buildFixedTzString(tz.offset_min, out, len);
}
