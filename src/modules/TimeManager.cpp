// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "modules/TimeManager.h"

#include <ctype.h>
#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include "core/Logger.h"
#ifdef UNIT_TEST
#include "TimeMock.h"
#endif

namespace {

String trim_copy(const String &value) {
    const char *begin = value.c_str();
    if (!begin) {
        return String();
    }

    const char *end = begin;
    while (*end != '\0') {
        ++end;
    }
    while (begin < end && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    String out;
    while (begin < end) {
        out += *begin++;
    }
    return out;
}

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

bool rtcTimeLooksUnset(bool osc_stop) {
    return osc_stop;
}

// Frozen compatibility list for configs saved before named timezones.
// Do not reorder or insert here; update kTimeZones independently.
const char *legacyTimezoneNameFromIndex(int index) {
    static const char *const kLegacyTimeZoneNames[] = {
        "Etc/GMT+12",
        "Pacific/Midway",
        "Pacific/Honolulu",
        "America/Anchorage",
        "America/Los_Angeles",
        "America/Denver",
        "America/Chicago",
        "America/New_York",
        "America/Santiago",
        "America/St_Johns",
        "America/Sao_Paulo",
        "Atlantic/South_Georgia",
        "Atlantic/Azores",
        "Europe/London",
        "Europe/Paris",
        "Europe/Kiev",
        "Africa/Cairo",
        "Europe/Moscow",
        "Asia/Tehran",
        "Asia/Dubai",
        "Asia/Kabul",
        "Asia/Karachi",
        "Asia/Kolkata",
        "Asia/Kathmandu",
        "Asia/Dhaka",
        "Asia/Yangon",
        "Asia/Bangkok",
        "Asia/Shanghai",
        "Asia/Singapore",
        "Asia/Tokyo",
        "Australia/Adelaide",
        "Australia/Brisbane",
        "Australia/Sydney",
        "Pacific/Noumea",
        "Pacific/Auckland",
        "Pacific/Chatham",
        "Pacific/Tongatapu",
        "Pacific/Kiritimati",
    };
    if (index < 0 || index >= static_cast<int>(sizeof(kLegacyTimeZoneNames) / sizeof(kLegacyTimeZoneNames[0]))) {
        return nullptr;
    }
    return kLegacyTimeZoneNames[index];
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
    (void)shared_i2c_runtime_gate_.resetForBoot();
    storage_ = &storage;
    const auto &cfg = storage.config();
    ntp_enabled_pref_ = cfg.ntp_enabled;
    ntp_server_pref_ = trim_copy(cfg.ntp_server);
    ntp_enabled_ = ntp_enabled_pref_;
    rtc_mode_ = Config::clampRtcMode(static_cast<int>(cfg.rtc_mode));
    const bool had_stored_timezone = (cfg.tz_name.length() > 0) || (cfg.tz_index >= 0);

    int resolved_tz_index = -1;
    if (cfg.tz_name.length() > 0) {
        const int named_index = findTimezoneIndex(cfg.tz_name.c_str());
        if (named_index >= 0 &&
            named_index < static_cast<int>(TIME_ZONE_COUNT) &&
            strcmp(kTimeZones[named_index].name, cfg.tz_name.c_str()) == 0) {
            resolved_tz_index = named_index;
        }
    }
    if (resolved_tz_index < 0 &&
        cfg.tz_index >= 0 &&
        cfg.tz_name.length() == 0) {
        const char *legacy_name = legacyTimezoneNameFromIndex(cfg.tz_index);
        const int legacy_named_index = legacy_name ? findTimezoneIndex(legacy_name) : -1;
        if (legacy_named_index >= 0 &&
            legacy_named_index < static_cast<int>(TIME_ZONE_COUNT) &&
            strcmp(kTimeZones[legacy_named_index].name, legacy_name) == 0) {
            resolved_tz_index = legacy_named_index;
        }
    }
    if (resolved_tz_index < 0 &&
        cfg.tz_index >= 0 &&
        cfg.tz_index < static_cast<int>(TIME_ZONE_COUNT)) {
        resolved_tz_index = cfg.tz_index;
    }
    if (resolved_tz_index < 0) {
        resolved_tz_index = findTimezoneIndex("Europe/London");
    }

    tz_index_ = resolved_tz_index;
    applyTimezone();
    if (had_stored_timezone) {
        persistTimezoneSelection();
    }
}

void TimeManager::disableSharedI2cRuntime() {
    if (!shared_i2c_runtime_gate_.disable()) {
        return;
    }
    LOGW("RTC", "shared I2C bus offline; RTC transactions disabled until reboot");
}

bool TimeManager::finalizeSharedI2cRuntimeDisable(uint32_t timeout_ms) {
    (void)shared_i2c_runtime_gate_.disable();
    const uint32_t started_ms = millis();
    while (!shared_i2c_runtime_gate_.idle()) {
        if (static_cast<uint32_t>(millis() - started_ms) >= timeout_ms) {
            return false;
        }
        delay(1);
    }

    resetDeferredRtcState();
    rtc_pending_write_ = false;
    rtc_pending_write_epoch_ = 0;
    rtc_pending_write_due_ms_ = 0;
    return true;
}

bool TimeManager::initRtc() {
    RtcAccess access = shared_i2c_runtime_gate_.acquire();
    if (!access) {
        return false;
    }
    rtc_probe_.reset(millis());
    resetDeferredRtcState();
    const bool valid_time_loaded = initRtcAttempt(access, false);
    rtc_probe_.recordAttempt(rtc_initialized_);
    return valid_time_loaded;
}

bool TimeManager::initRtcAttempt(const RtcAccess &access,
                                 bool defer_initial_read) {
    if (!access) return false;
    if (defer_initial_read) {
        startDeferredRtcAttempt();
        return false;
    }

    resetDeferredRtcState();
    rtc_type_ = RtcType::None;
    rtc_present_ = false;
    rtc_initialized_ = false;
    rtc_valid_ = false;
    rtc_lost_power_ = false;
    rtc_time_unset_ = false;
    rtc_battery_low_ = false;
    rtc_probe_needs_pcf_verification_ = false;
    last_rtc_status_poll_ms_ = 0;
    rtc_read_fail_count_ = 0;

    if (!detectRtc(access)) {
        return false;
    }
    rtc_present_ = true;
    if (!rtcBegin(access)) {
        LOGW("RTC", "%s init failed", rtcLabel());
        return false;
    }
    rtc_initialized_ = true;
    delay(500);
    tm utc_tm = {};
    bool osc_stop = false;
    bool time_valid = false;
    bool read_ok = readRtcInitState(utc_tm, osc_stop, time_valid, access);
    if (rtc_mode_ == Config::RtcMode::Auto &&
        rtc_type_ == RtcType::Ds3231 &&
        rtc_probe_needs_pcf_verification_ &&
        (!read_ok || (!osc_stop && !time_valid))) {
        if (retryWeakDs3231AsPcf8523(utc_tm, osc_stop, time_valid, access)) {
            read_ok = true;
        } else {
            // A weak DS3231-shaped response that cannot be verified as
            // PCF8523 is unresolved, not an initialized RTC. Keep the bounded
            // startup detector active so the complete probe can run again.
            rtc_initialized_ = false;
            return false;
        }
    }
    if (!read_ok) {
        return false;
    }
    return applyRtcInitState(utc_tm, osc_stop, time_valid, access);
}

bool TimeManager::applyRtcInitState(const tm &utc_tm,
                                    bool osc_stop,
                                    bool time_valid,
                                    const RtcAccess &access) {
    rtc_lost_power_ = osc_stop;
    bool battery_low = false;
    if (rtcReadBatteryLow(battery_low, access)) {
        applyRtcBatteryLowState(battery_low, true);
    }
    last_rtc_status_poll_ms_ = millis();
    if (!time_valid) {
        rtc_valid_ = false;
        rtc_time_unset_ = rtcTimeLooksUnset(osc_stop);
        if (rtc_time_unset_) {
            LOGI("RTC", "%s time not set; waiting for NTP or manual time", rtcLabel());
        }
        return false;
    }
    time_t epoch = makeUtcEpoch(utc_tm);
    rtc_time_unset_ = rtcTimeLooksUnset(osc_stop);
    if (rtc_time_unset_) {
        rtc_valid_ = false;
        LOGI("RTC", "%s time not set; waiting for NTP or manual time", rtcLabel());
        return false;
    }
    if (epoch > Config::TIME_VALID_EPOCH) {
        if (osc_stop) {
            if (rtcClearLostPower(access)) {
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

bool TimeManager::readRtcInitState(tm &utc_tm,
                                   bool &osc_stop,
                                   bool &time_valid,
                                   const RtcAccess &access) {
    bool read_ok = false;
    for (uint8_t attempt = 0; attempt < Config::RTC_INIT_ATTEMPTS; ++attempt) {
        if (attempt > 0) {
            delay(Config::RTC_INIT_RETRY_MS);
            LOGD("RTC", "retry %u", attempt);
        }
        if (!rtcReadTime(utc_tm, osc_stop, time_valid, access)) {
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

bool TimeManager::retryWeakDs3231AsPcf8523(tm &utc_tm,
                                           bool &osc_stop,
                                           bool &time_valid,
                                           const RtcAccess &access) {
    if (!access) return false;
    if (!pcf8523_.probeFallback()) {
        return false;
    }

    LOGW("RTC", "weak DS3231 probe did not validate, retrying as %s", Pcf8523::label());
    rtc_type_ = RtcType::Pcf8523;
    rtc_probe_needs_pcf_verification_ = false;

    if (!rtcBegin(access)) {
        rtc_initialized_ = false;
        LOGW("RTC", "%s init failed after weak DS3231 retry", rtcLabel());
        return false;
    }
    rtc_initialized_ = true;
    delay(500);
    return readRtcInitState(utc_tm, osc_stop, time_valid, access);
}

void TimeManager::resetDeferredRtcState() {
    rtc_deferred_init_phase_ = RtcDeferredInitPhase::None;
    rtc_deferred_begin_ok_ = false;
    rtc_deferred_verifying_weak_ds_ = false;
    rtc_deferred_require_read_success_ = false;
    rtc_deferred_any_read_ok_ = false;
    rtc_deferred_last_osc_stop_ = false;
    rtc_deferred_last_time_valid_ = false;
    rtc_deferred_ds_calendar_ok_ = false;
    rtc_deferred_ds_calendar_valid_ = false;
    rtc_deferred_read_due_ms_ = 0;
    rtc_deferred_read_attempts_ = 0;
    rtc_deferred_ds_status_ = 0;
    rtc_deferred_ds_probe_ = Ds3231::ProbeStrength::None;
    rtc_deferred_tm_ = {};
    rtc_deferred_ds_calendar_tm_ = {};
    memset(rtc_deferred_ds_probe_meta_, 0, sizeof(rtc_deferred_ds_probe_meta_));
    memset(rtc_deferred_ds_probe_wrap_, 0, sizeof(rtc_deferred_ds_probe_wrap_));
    memset(rtc_deferred_ds_probe_head_, 0, sizeof(rtc_deferred_ds_probe_head_));
    memset(rtc_deferred_pcf_control_, 0, sizeof(rtc_deferred_pcf_control_));
    memset(rtc_deferred_pcf_time_, 0, sizeof(rtc_deferred_pcf_time_));
    memset(rtc_deferred_pcf_timers_, 0, sizeof(rtc_deferred_pcf_timers_));
}

void TimeManager::startDeferredRtcAttempt() {
    if (!isSharedI2cRuntimeAvailable()) {
        resetDeferredRtcState();
        return;
    }
    resetDeferredRtcState();
    rtc_type_ = RtcType::None;
    rtc_present_ = false;
    rtc_initialized_ = false;
    rtc_valid_ = false;
    rtc_lost_power_ = false;
    rtc_time_unset_ = false;
    rtc_battery_low_ = false;
    rtc_probe_needs_pcf_verification_ = false;
    last_rtc_status_poll_ms_ = 0;
    rtc_read_fail_count_ = 0;

    rtc_deferred_init_phase_ =
        rtc_mode_ == Config::RtcMode::Ds3231
            ? RtcDeferredInitPhase::DetectDsMeta
            : RtcDeferredInitPhase::DetectPcfSignature;
}

void TimeManager::selectDeferredRtc(RtcType type,
                                    bool needs_pcf_verification,
                                    bool require_read_success) {
    rtc_type_ = type;
    rtc_present_ = type != RtcType::None;
    rtc_initialized_ = false;
    rtc_probe_needs_pcf_verification_ = needs_pcf_verification;
    rtc_deferred_require_read_success_ = require_read_success;
    rtc_deferred_init_phase_ = RtcDeferredInitPhase::BeginSelected;
}

void TimeManager::resolveDeferredPcfFallback(bool matched,
                                             PollResult &result) {
    if (rtc_deferred_verifying_weak_ds_) {
        if (!matched) {
            finishDeferredRtcAttempt(false, result);
            return;
        }
        LOGW("RTC", "weak DS3231 probe did not validate, retrying as %s", Pcf8523::label());
        rtc_deferred_verifying_weak_ds_ = false;
        selectDeferredRtc(RtcType::Pcf8523, false, true);
        return;
    }

    if (rtc_mode_ == Config::RtcMode::Pcf8523) {
        if (!matched) {
            finishDeferredRtcAttempt(false, result);
            return;
        }
        selectDeferredRtc(RtcType::Pcf8523, false, false);
        LOGI("RTC", "%s selected manually at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
        return;
    }

    if (rtc_deferred_ds_probe_ == Ds3231::ProbeStrength::Weak) {
        selectDeferredRtc(RtcType::Ds3231, matched, false);
        LOGI("RTC", "%s weak signature at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
        return;
    }
    if (matched) {
        selectDeferredRtc(RtcType::Pcf8523, false, false);
        LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
        return;
    }
    finishDeferredRtcAttempt(false, result);
}

bool TimeManager::applyDeferredRtcInitState() {
    rtc_lost_power_ = rtc_deferred_last_osc_stop_;
    last_rtc_status_poll_ms_ = millis();
    if (!rtc_deferred_last_time_valid_) {
        rtc_valid_ = false;
        rtc_time_unset_ = rtcTimeLooksUnset(rtc_deferred_last_osc_stop_);
        if (rtc_time_unset_) {
            LOGI("RTC", "%s time not set; waiting for NTP or manual time", rtcLabel());
        }
        return false;
    }

    const time_t epoch = makeUtcEpoch(rtc_deferred_tm_);
    rtc_time_unset_ = rtcTimeLooksUnset(rtc_deferred_last_osc_stop_);
    if (rtc_time_unset_) {
        rtc_valid_ = false;
        LOGI("RTC", "%s time not set; waiting for NTP or manual time", rtcLabel());
        return false;
    }
    if (epoch > Config::TIME_VALID_EPOCH) {
        rtc_valid_ = true;
        setSystemTime(epoch);
        return true;
    }
    rtc_valid_ = false;
    return false;
}

void TimeManager::prepareDeferredRtcFinish(PollResult &result) {
    // NTP or manual time can become valid while detection is in flight. Once
    // the RTC identity is confirmed, seed the RTC without replacing the newer
    // system clock. DS3231 writes are split into one transaction per poll.
    if (rtc_deferred_begin_ok_ && isSystemTimeValid()) {
        const time_t epoch = nowEpoch();
        if (gmtimeInto(epoch, rtc_deferred_tm_)) {
            rtc_pending_write_ = true;
            rtc_pending_write_epoch_ = epoch;
            rtc_pending_write_due_ms_ = 0;
            rtc_deferred_init_phase_ =
                rtc_type_ == RtcType::Pcf8523
                    ? RtcDeferredInitPhase::WriteTrustedPcfTime
                    : RtcDeferredInitPhase::WriteTrustedDsCalendar;
            return;
        }
    }

    if (rtc_deferred_any_read_ok_) {
        result.time_updated = applyDeferredRtcInitState();
    }
    finishDeferredRtcAttempt(rtc_deferred_begin_ok_, result);
}

void TimeManager::completeDeferredRtcRead(bool read_ok,
                                          PollResult &result) {
    if (rtc_deferred_read_attempts_ < UINT8_MAX) {
        ++rtc_deferred_read_attempts_;
    }
    if (read_ok) {
        rtc_deferred_any_read_ok_ = true;
        noteRtcReadSuccess(false);
    }

    const bool read_verified =
        read_ok && !rtc_deferred_last_osc_stop_ && rtc_deferred_last_time_valid_;
    if (read_verified) {
        prepareDeferredRtcFinish(result);
        return;
    }
    if (rtc_deferred_read_attempts_ < Config::RTC_INIT_ATTEMPTS) {
        // The I2C call above may consume most of its timeout. Anchor the retry
        // to a fresh clock sample instead of the caller's stale now_ms value.
        rtc_deferred_read_due_ms_ = millis() + Config::RTC_INIT_RETRY_MS;
        rtc_deferred_init_phase_ = RtcDeferredInitPhase::ReadWait;
        return;
    }

    const bool should_try_pcf =
        rtc_mode_ == Config::RtcMode::Auto &&
        rtc_type_ == RtcType::Ds3231 &&
        rtc_probe_needs_pcf_verification_ &&
        (!rtc_deferred_any_read_ok_ ||
         (!rtc_deferred_last_osc_stop_ && !rtc_deferred_last_time_valid_));
    if (should_try_pcf) {
        rtc_deferred_verifying_weak_ds_ = true;
        rtc_deferred_require_read_success_ = true;
        rtc_deferred_any_read_ok_ = false;
        rtc_deferred_read_attempts_ = 0;
        rtc_deferred_last_osc_stop_ = false;
        rtc_deferred_last_time_valid_ = false;
        rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackControl;
        return;
    }

    // A PCF8523 reached through an ambiguous weak-DS fallback is confirmed
    // only by an actual successful calendar read. A successful begin/write
    // alone is not enough to stop the bounded startup detector.
    if (rtc_deferred_require_read_success_ && !rtc_deferred_any_read_ok_) {
        finishDeferredRtcAttempt(false, result);
        return;
    }
    prepareDeferredRtcFinish(result);
}

void TimeManager::markDeferredRtcWriteSuccess() {
    rtc_valid_ = true;
    rtc_lost_power_ = false;
    rtc_time_unset_ = false;
    last_rtc_status_poll_ms_ = millis();
    rtc_pending_write_ = false;
    rtc_pending_write_epoch_ = 0;
    rtc_pending_write_due_ms_ = 0;
}

void TimeManager::finishDeferredRtcAttempt(bool initialized,
                                           PollResult &result) {
    rtc_initialized_ = initialized;
    rtc_deferred_init_phase_ = RtcDeferredInitPhase::None;
    rtc_deferred_begin_ok_ = false;
    rtc_deferred_verifying_weak_ds_ = false;
    rtc_deferred_require_read_success_ = false;
    rtc_deferred_any_read_ok_ = false;
    rtc_deferred_read_due_ms_ = 0;
    rtc_deferred_read_attempts_ = 0;
    rtc_probe_.recordAttempt(initialized);
    result.state_changed = true;
    if (!initialized && rtc_probe_.exhausted()) {
        LOGI("RTC", "not detected after startup probes; stop probing until reboot");
    }
}

TimeManager::PollResult TimeManager::pollDeferredRtcInit(
    uint32_t now_ms,
    const RtcAccess &access) {
    PollResult result;
    if (!access) return result;
    if (rtc_deferred_init_phase_ == RtcDeferredInitPhase::None) {
        return result;
    }

    const bool was_present = rtc_present_;
    const bool was_initialized = rtc_initialized_;

    switch (rtc_deferred_init_phase_) {
        case RtcDeferredInitPhase::DetectPcfSignature: {
            bool matched = false;
            const bool read_ok = pcf8523_.readProbeSignature(matched);
            if (read_ok && matched) {
                selectDeferredRtc(RtcType::Pcf8523, false, false);
                if (rtc_mode_ == Config::RtcMode::Pcf8523) {
                    LOGI("RTC", "%s selected manually at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
                } else {
                    LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::PCF8523_ADDR);
                }
            } else {
                rtc_deferred_init_phase_ =
                    rtc_mode_ == Config::RtcMode::Pcf8523
                        ? RtcDeferredInitPhase::DetectPcfFallbackControl
                        : RtcDeferredInitPhase::DetectDsMeta;
            }
            break;
        }

        case RtcDeferredInitPhase::DetectDsMeta:
            if (ds3231_.readProbeMeta(rtc_deferred_ds_probe_meta_)) {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectDsWrap;
            } else if (rtc_mode_ == Config::RtcMode::Ds3231) {
                finishDeferredRtcAttempt(false, result);
            } else {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackControl;
            }
            break;

        case RtcDeferredInitPhase::DetectDsWrap:
            if (ds3231_.readProbeWrap(rtc_deferred_ds_probe_wrap_)) {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectDsHead;
            } else if (rtc_mode_ == Config::RtcMode::Ds3231) {
                finishDeferredRtcAttempt(false, result);
            } else {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackControl;
            }
            break;

        case RtcDeferredInitPhase::DetectDsHead:
            if (ds3231_.readProbeHead(rtc_deferred_ds_probe_head_)) {
                rtc_deferred_ds_probe_ = Ds3231::classifyProbe(
                    rtc_deferred_ds_probe_meta_,
                    rtc_deferred_ds_probe_wrap_,
                    rtc_deferred_ds_probe_head_);
            } else {
                rtc_deferred_ds_probe_ = Ds3231::ProbeStrength::None;
            }
            if (rtc_mode_ == Config::RtcMode::Ds3231) {
                if (rtc_deferred_ds_probe_ == Ds3231::ProbeStrength::None) {
                    finishDeferredRtcAttempt(false, result);
                } else {
                    selectDeferredRtc(RtcType::Ds3231, false, false);
                    if (rtc_deferred_ds_probe_ == Ds3231::ProbeStrength::Weak) {
                        LOGI("RTC", "%s weak signature at 0x%02X (manual)", rtcLabel(), Config::DS3231_ADDR);
                    } else {
                        LOGI("RTC", "%s selected manually at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
                    }
                }
            } else if (rtc_deferred_ds_probe_ == Ds3231::ProbeStrength::Strong) {
                selectDeferredRtc(RtcType::Ds3231, false, false);
                LOGI("RTC", "%s found at 0x%02X", rtcLabel(), Config::DS3231_ADDR);
            } else {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackControl;
            }
            break;

        case RtcDeferredInitPhase::DetectPcfFallbackControl:
            if (pcf8523_.readProbeFallbackControl(rtc_deferred_pcf_control_)) {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackTime;
            } else {
                resolveDeferredPcfFallback(false, result);
            }
            break;

        case RtcDeferredInitPhase::DetectPcfFallbackTime:
            if (pcf8523_.readProbeFallbackTime(rtc_deferred_pcf_time_)) {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::DetectPcfFallbackTimers;
            } else {
                resolveDeferredPcfFallback(false, result);
            }
            break;

        case RtcDeferredInitPhase::DetectPcfFallbackTimers: {
            const bool read_ok =
                pcf8523_.readProbeFallbackTimers(rtc_deferred_pcf_timers_);
            const bool matched =
                read_ok && Pcf8523::probeFallbackMatches(
                               rtc_deferred_pcf_control_,
                               rtc_deferred_pcf_time_,
                               rtc_deferred_pcf_timers_);
            resolveDeferredPcfFallback(matched, result);
            break;
        }

        case RtcDeferredInitPhase::BeginSelected:
            rtc_deferred_begin_ok_ = rtcBegin(access);
            if (!rtc_deferred_begin_ok_) {
                LOGW("RTC", "%s init failed", rtcLabel());
                finishDeferredRtcAttempt(false, result);
                break;
            }
            rtc_deferred_any_read_ok_ = false;
            rtc_deferred_read_attempts_ = 0;
            rtc_deferred_last_osc_stop_ = false;
            rtc_deferred_last_time_valid_ = false;
            rtc_deferred_ds_calendar_ok_ = false;
            rtc_deferred_ds_calendar_valid_ = false;
            // rtcBegin() can block until the I2C timeout. Use a fresh sample.
            rtc_deferred_read_due_ms_ = millis() + 500U;
            rtc_deferred_init_phase_ = RtcDeferredInitPhase::ReadWait;
            break;

        case RtcDeferredInitPhase::ReadWait:
            if (!StartupProbePolicy::deadlineReached(now_ms, rtc_deferred_read_due_ms_)) {
                break;
            }
            if (rtc_type_ == RtcType::Pcf8523) {
                tm candidate = {};
                bool osc_stop = false;
                bool time_valid = false;
                const bool read_ok = pcf8523_.readTime(candidate, osc_stop, time_valid);
                if (read_ok) {
                    rtc_deferred_tm_ = candidate;
                    rtc_deferred_last_osc_stop_ = osc_stop;
                    rtc_deferred_last_time_valid_ = time_valid;
                }
                completeDeferredRtcRead(read_ok, result);
            } else if (rtc_type_ == RtcType::Ds3231) {
                rtc_deferred_ds_calendar_ok_ =
                    ds3231_.readCalendar(rtc_deferred_ds_calendar_tm_,
                                         rtc_deferred_ds_calendar_valid_);
                if (!rtc_deferred_ds_calendar_ok_) {
                    completeDeferredRtcRead(false, result);
                } else {
                    rtc_deferred_init_phase_ = RtcDeferredInitPhase::ReadDsStatus;
                }
            } else {
                finishDeferredRtcAttempt(false, result);
            }
            break;

        case RtcDeferredInitPhase::ReadDsStatus: {
            uint8_t status = 0;
            const bool read_ok = rtc_deferred_ds_calendar_ok_ && ds3231_.readStatus(status);
            if (read_ok) {
                rtc_deferred_tm_ = rtc_deferred_ds_calendar_tm_;
                rtc_deferred_last_time_valid_ = rtc_deferred_ds_calendar_valid_;
                rtc_deferred_ds_status_ = status;
                rtc_deferred_last_osc_stop_ =
                    (status & Config::DS3231_STATUS_OSF) != 0;
            }
            rtc_deferred_ds_calendar_ok_ = false;
            rtc_deferred_ds_calendar_valid_ = false;
            completeDeferredRtcRead(read_ok, result);
            break;
        }

        case RtcDeferredInitPhase::WriteTrustedPcfTime: {
            const bool write_ok = pcf8523_.writeTime(rtc_deferred_tm_);
            if (write_ok) {
                markDeferredRtcWriteSuccess();
            } else {
                LOGW("RTC", "%s write from current system time failed", rtcLabel());
            }
            finishDeferredRtcAttempt(true, result);
            break;
        }

        case RtcDeferredInitPhase::WriteTrustedDsCalendar:
            if (!ds3231_.writeCalendar(rtc_deferred_tm_)) {
                LOGW("RTC", "%s write from current system time failed", rtcLabel());
                finishDeferredRtcAttempt(true, result);
            } else {
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::ReadTrustedDsStatus;
            }
            break;

        case RtcDeferredInitPhase::ReadTrustedDsStatus:
            if (!ds3231_.readStatus(rtc_deferred_ds_status_)) {
                LOGW("RTC", "%s write from current system time failed", rtcLabel());
                finishDeferredRtcAttempt(true, result);
            } else {
                rtc_deferred_ds_status_ &=
                    static_cast<uint8_t>(~Config::DS3231_STATUS_OSF);
                rtc_deferred_init_phase_ = RtcDeferredInitPhase::WriteTrustedDsStatus;
            }
            break;

        case RtcDeferredInitPhase::WriteTrustedDsStatus:
            if (ds3231_.writeStatus(rtc_deferred_ds_status_)) {
                markDeferredRtcWriteSuccess();
            } else {
                LOGW("RTC", "%s write from current system time failed", rtcLabel());
            }
            finishDeferredRtcAttempt(true, result);
            break;

        case RtcDeferredInitPhase::None:
            break;
    }

    if (rtc_present_ != was_present || rtc_initialized_ != was_initialized) {
        result.state_changed = true;
    }
    return result;
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
    const bool previous = ntp_enabled_pref_;
    ntp_enabled_pref_ = enabled;
    if (storage_) {
        storage_->config().ntp_enabled = ntp_enabled_pref_;
        if (!storage_->saveConfig(true)) {
            LOGE("Time", "failed to persist NTP preference");
            storage_->config().ntp_enabled = previous;
            ntp_enabled_pref_ = previous;
            return false;
        }
    }
    return syncNtpWithWifi();
}

bool TimeManager::setNtpServerPref(const String &server) {
    String next = trim_copy(server);
    if (next == ntp_server_pref_) {
        return false;
    }

    const String previous = ntp_server_pref_;
    ntp_server_pref_ = next;
    if (storage_) {
        storage_->config().ntp_server = ntp_server_pref_;
        if (!storage_->saveConfig(true)) {
            LOGE("Time", "failed to persist NTP server");
            storage_->config().ntp_server = previous;
            ntp_server_pref_ = previous;
            return false;
        }
    }

    bool state_changed = false;
    if (ntp_syncing_) {
        LOGI("Time", "restarting NTP sync after server change");
        state_changed = true;
    }
    stopNtpService();
    ntp_syncing_ = false;
    ntp_err_ = false;
    ntp_sync_start_ms_ = 0;
    ntp_last_attempt_ms_ = 0;

    if (ntp_enabled_ && wifi_connected_ && requestNtpSync()) {
        state_changed = true;
    }
    return state_changed;
}

TimeManager::PollResult TimeManager::poll(uint32_t now_ms,
                                         bool rtc_i2c_available) {
    PollResult result = ntpPoll(now_ms, rtc_i2c_available);
    if (!rtc_i2c_available) {
        return result;
    }
    RtcAccess access = shared_i2c_runtime_gate_.acquire();
    if (!access) {
        return result;
    }
    if (rtc_deferred_init_phase_ != RtcDeferredInitPhase::None) {
        const PollResult deferred = pollDeferredRtcInit(now_ms, access);
        result.state_changed = result.state_changed || deferred.state_changed;
        result.time_updated = result.time_updated || deferred.time_updated;
        return result;
    }

    if (rtc_pending_write_ && rtc_present_ && rtc_initialized_ &&
        StartupProbePolicy::deadlineReached(now_ms, rtc_pending_write_due_ms_)) {
        const bool state_was_valid = rtc_valid_ && !rtc_lost_power_ && !rtc_time_unset_;
        if (rtcWriteFromEpoch(rtc_pending_write_epoch_, access)) {
            result.state_changed = result.state_changed || !state_was_valid;
        } else {
            // Anchor the retry after the completed I2C call, not to now_ms
            // sampled by the caller before that transaction started.
            rtc_pending_write_due_ms_ = millis() + Config::RTC_INIT_RETRY_MS;
        }
        return result;
    }

    bool rtc_probe_attempted = false;
    if (!rtc_initialized_ && rtc_probe_.shouldAttempt(now_ms)) {
        rtc_probe_attempted = true;
        const bool was_present = rtc_present_;
        const bool valid_time_loaded = initRtcAttempt(access, true);
        const bool deferred_read_started =
            rtc_deferred_init_phase_ != RtcDeferredInitPhase::None;
        if (!deferred_read_started) {
            rtc_probe_.recordAttempt(rtc_initialized_);
        }
        if (rtc_present_ != was_present || rtc_initialized_) {
            result.state_changed = true;
        }
        if (valid_time_loaded) {
            result.time_updated = true;
        }
        if (!deferred_read_started &&
            !rtc_initialized_ &&
            rtc_probe_.exhausted()) {
            LOGI("RTC", "not detected after startup probes; stop probing until reboot");
        }
    }
    if (!rtc_probe_attempted) {
        PollResult rtc_result = pollRtcStatus(now_ms, access);
        result.state_changed = result.state_changed || rtc_result.state_changed;
        result.time_updated = result.time_updated || rtc_result.time_updated;
    }
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
    {
        RtcAccess access = shared_i2c_runtime_gate_.acquire();
        if (access) {
            (void)rtcWriteFromEpoch(epoch, access);
        }
    }
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
    persistTimezoneSelection();
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

void TimeManager::persistTimezoneSelection() {
    if (!storage_) {
        return;
    }
    if (tz_index_ < 0 || tz_index_ >= static_cast<int>(TIME_ZONE_COUNT)) {
        return;
    }

    Config::StoredConfig &cfg = storage_->config();
    const char *tz_name = kTimeZones[tz_index_].name ? kTimeZones[tz_index_].name : "";
    if (cfg.tz_index == tz_index_ && cfg.tz_name == tz_name) {
        return;
    }

    cfg.tz_index = tz_index_;
    cfg.tz_name = tz_name;
    if (!storage_->saveConfig(true)) {
        storage_->requestSave();
        LOGE("Time", "failed to persist timezone selection");
    }
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
    RtcAccess access;
    if (now <= Config::TIME_VALID_EPOCH && rtc_present_ && rtc_initialized_) {
        access = shared_i2c_runtime_gate_.acquire();
    }
    if (access) {
        uint32_t now_ms = millis();
        if (now_ms - last_rtc_restore_ms_ >= Config::RTC_RESTORE_INTERVAL_MS) {
            last_rtc_restore_ms_ = now_ms;
            tm utc_tm = {};
            bool osc_stop = false;
            bool time_valid = false;
            if (rtcReadTime(utc_tm, osc_stop, time_valid, access)) {
                noteRtcReadSuccess(false);
                rtc_lost_power_ = osc_stop;
                const time_t epoch = time_valid ? makeUtcEpoch(utc_tm) : static_cast<time_t>(-1);
                rtc_time_unset_ = rtcTimeLooksUnset(osc_stop);
                rtc_valid_ = time_valid && !osc_stop && !rtc_time_unset_;
                if (rtc_valid_) {
                    if (setSystemTime(epoch)) {
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

bool TimeManager::rtcWriteFromEpoch(time_t epoch,
                                    const RtcAccess &access) {
    if (!access || !rtc_present_ || !rtc_initialized_) {
        return false;
    }
    tm utc_tm = {};
    if (!gmtimeInto(epoch, utc_tm)) {
        return false;
    }
    if (!rtcWriteTime(utc_tm, access)) {
        return false;
    }
    rtc_valid_ = true;
    rtc_lost_power_ = false;
    rtc_time_unset_ = false;
    rtc_pending_write_ = false;
    rtc_pending_write_epoch_ = 0;
    rtc_pending_write_due_ms_ = 0;
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
    const bool use_custom_server = ntp_server_pref_.length() > 0;
    LOGI("Time",
         "NTP sync start (tz=%s, server=%s, wifi=ON)",
         tz.name ? tz.name : "unknown",
         use_custom_server ? ntp_server_pref_.c_str() : "default");
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    if (use_custom_server) {
        configTzTime(posix_tz, ntp_server_pref_.c_str(), nullptr, nullptr);
    } else {
        configTzTime(posix_tz, "pool.ntp.org", "time.nist.gov", "time.google.com");
    }
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

TimeManager::PollResult TimeManager::ntpPoll(uint32_t now_ms,
                                            bool rtc_i2c_available) {
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
                ntp_syncing_ = false;
                ntp_err_ = false;
                ntp_last_sync_ms_ = now_ms;
                bool write_attempted = false;
                bool write_ok = false;
                RtcAccess access = shared_i2c_runtime_gate_.acquire();
                if (access) {
                    if (rtc_i2c_available &&
                        rtc_deferred_init_phase_ == RtcDeferredInitPhase::None) {
                        write_attempted = true;
                        write_ok = rtcWriteFromEpoch(epoch, access);
                    }
                    if (!write_ok) {
                        rtc_pending_write_ = true;
                        rtc_pending_write_epoch_ = epoch;
                        rtc_pending_write_due_ms_ =
                            write_attempted
                                ? millis() + Config::RTC_INIT_RETRY_MS
                                : 0U;
                    }
                }
                result.state_changed = true;
                result.time_updated = true;
                return result;
            }
        }
        const uint32_t elapsed = now_ms - ntp_sync_start_ms_;
        // Guard against sampling-order inversion where now_ms is taken before ntp_sync_start_ms_ is updated.
        if (static_cast<int32_t>(elapsed) > static_cast<int32_t>(Config::NTP_SYNC_TIMEOUT_MS)) {
            LOGW("Time", "NTP sync timeout after %lu ms", static_cast<unsigned long>(elapsed));
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

TimeManager::PollResult TimeManager::pollRtcStatus(
    uint32_t now_ms,
    const RtcAccess &access) {
    PollResult result;
    if (!access) {
        return result;
    }
    if (!rtc_present_ || !rtc_initialized_) {
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
    if (rtcReadTime(utc_tm, osc_stop, time_valid, access)) {
        noteRtcReadSuccess(true);
        const bool rtc_time_unset = rtcTimeLooksUnset(osc_stop);
        const bool rtc_valid = time_valid && !osc_stop && !rtc_time_unset;
        if (rtc_lost_power_ != osc_stop ||
            rtc_valid_ != rtc_valid ||
            rtc_time_unset_ != rtc_time_unset) {
            result.state_changed = true;
        }
        rtc_lost_power_ = osc_stop;
        rtc_valid_ = rtc_valid;
        rtc_time_unset_ = rtc_time_unset;
        rtc_present_ = true;
    } else if (noteRtcReadFailure(true)) {
        result.state_changed = true;
    }

    bool battery_low = false;
    if (rtcReadBatteryLow(battery_low, access)) {
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
    }

    const bool state_changed = rtc_valid_ || rtc_time_unset_;
    rtc_valid_ = false;
    rtc_time_unset_ = false;
    rtc_present_ = true;
    return state_changed;
}

bool TimeManager::detectRtc(const RtcAccess &access) {
    if (!access) {
        return false;
    }
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

    // Prefer the explicit PCF8523 signature first on the shared 0x68 address.
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

bool TimeManager::rtcBegin(const RtcAccess &access) {
    if (!access) {
        return false;
    }
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.begin();
        case RtcType::Ds3231:
            return ds3231_.begin();
        default:
            return false;
    }
}

bool TimeManager::rtcReadTime(tm &out,
                              bool &osc_stop,
                              bool &valid,
                              const RtcAccess &access) {
    if (!access) {
        return false;
    }
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.readTime(out, osc_stop, valid);
        case RtcType::Ds3231:
            return ds3231_.readTime(out, osc_stop, valid);
        default:
            return false;
    }
}

bool TimeManager::rtcWriteTime(const tm &utc_tm,
                               const RtcAccess &access) {
    if (!access) {
        return false;
    }
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.writeTime(utc_tm);
        case RtcType::Ds3231:
            return ds3231_.writeTime(utc_tm);
        default:
            return false;
    }
}

bool TimeManager::rtcClearLostPower(const RtcAccess &access) {
    if (!access) {
        return false;
    }
    switch (rtc_type_) {
        case RtcType::Pcf8523:
            return pcf8523_.clearOscillatorStop();
        case RtcType::Ds3231:
            return ds3231_.clearOscillatorStop();
        default:
            return false;
    }
}

bool TimeManager::rtcReadBatteryLow(bool &low,
                                    const RtcAccess &access) {
    if (!access) {
        low = false;
        return false;
    }
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
        } else if (had_low) {
            LOGI("RTC", "battery status OK");
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
