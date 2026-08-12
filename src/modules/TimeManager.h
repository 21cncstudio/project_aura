// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <time.h>
#include "config/AppConfig.h"
#include "config/AppData.h"
#include "core/SharedI2cRuntimeGate.h"
#include "core/StartupProbePolicy.h"
#include "drivers/Ds3231.h"
#include "drivers/Pcf8523.h"
#include "modules/StorageManager.h"

class TimeManager {
public:
    enum NtpUiState { NTP_UI_OFF, NTP_UI_SYNCING, NTP_UI_OK, NTP_UI_ERR };

    struct PollResult {
        bool state_changed = false;
        bool time_updated = false;
    };

    void begin(StorageManager &storage);
    bool initRtc();
    void disableSharedI2cRuntime();
    bool finalizeSharedI2cRuntimeDisable(uint32_t timeout_ms);
    bool isSharedI2cRuntimeAvailable() const {
        return shared_i2c_runtime_gate_.available();
    }

    bool updateWifiState(bool wifi_enabled, bool wifi_connected);

    bool setNtpEnabledPref(bool enabled);
    bool isNtpEnabledPref() const { return ntp_enabled_pref_; }
    bool setNtpServerPref(const String &server);
    const String &ntpServerPref() const { return ntp_server_pref_; }
    bool isNtpEnabled() const { return ntp_enabled_; }
    bool isNtpSyncing() const { return ntp_syncing_; }
    bool isNtpError() const { return ntp_err_; }
    uint32_t lastNtpSyncMs() const { return ntp_last_sync_ms_; }

    PollResult poll(uint32_t now_ms, bool rtc_i2c_available = true);

    NtpUiState getNtpUiState(uint32_t now_ms) const;
    bool isManualLocked(uint32_t now_ms) const;

    bool setLocalTime(int year, int month, int day, int hour, int minute);

    bool setTimezoneIndex(int index);
    bool adjustTimezone(int delta);
    int getTimezoneIndex() const { return tz_index_; }
    const TimeZoneEntry &getTimezone() const;
    int currentUtcOffsetMinutes() const;

    bool isSystemTimeValid() const;
    bool getLocalTime(tm &out);
    bool syncInputsFromSystem(int &hour, int &minute, int &day, int &month, int &year);

    bool isRtcPresent() const { return rtc_present_; }
    bool isRtcInitialized() const { return rtc_initialized_; }
    bool isRtcDetecting() const {
        return isSharedI2cRuntimeAvailable() &&
               (rtc_deferred_init_phase_ != RtcDeferredInitPhase::None ||
                (!rtc_initialized_ && rtc_probe_.pending()));
    }
    bool isRtcValid() const { return rtc_valid_; }
    bool isRtcLostPower() const { return rtc_lost_power_; }
    bool isRtcTimeUnset() const { return rtc_time_unset_; }
    bool isRtcReadFault() const {
        return rtc_present_ && rtc_initialized_ &&
               rtc_read_fail_count_ >= Config::RTC_STATUS_READ_FAIL_LIMIT;
    }
    bool isRtcBatteryLow() const { return rtc_battery_low_; }
    Config::RtcMode configuredRtcMode() const { return rtc_mode_; }
    const char *rtcLabel() const;
    static const char *rtcModeLabel(Config::RtcMode mode);

    static int findTimezoneIndex(const char *name);
    static void formatTzOffset(int offset_min, char *out, size_t len);
    static bool isLeapYear(int year);
    static int daysInMonth(int year, int month);

private:
    using RtcAccess = SharedI2cRuntimeGate::Gate::Access;

    enum class RtcType : uint8_t {
        None = 0,
        Pcf8523,
        Ds3231
    };

    enum class RtcDeferredInitPhase : uint8_t {
        None = 0,
        DetectPcfSignature,
        DetectDsMeta,
        DetectDsWrap,
        DetectDsHead,
        DetectPcfFallbackControl,
        DetectPcfFallbackTime,
        DetectPcfFallbackTimers,
        BeginSelected,
        ReadWait,
        ReadDsStatus,
        WriteTrustedPcfTime,
        WriteTrustedDsCalendar,
        ReadTrustedDsStatus,
        WriteTrustedDsStatus,
    };

    void applyTimezone();
    void persistTimezoneSelection();
    static void buildFixedTzString(int offset_min, char *out, size_t len);
    time_t makeUtcEpoch(const tm &utc_tm);
    bool setSystemTime(time_t epoch);
    bool rtcWriteFromEpoch(time_t epoch, const RtcAccess &access);
    bool initRtcAttempt(const RtcAccess &access, bool defer_initial_read = false);
    bool applyRtcInitState(const tm &utc_tm,
                           bool osc_stop,
                           bool time_valid,
                           const RtcAccess &access);
    bool applyDeferredRtcInitState();
    void resetDeferredRtcState();
    void startDeferredRtcAttempt();
    void selectDeferredRtc(RtcType type, bool needs_pcf_verification, bool require_read_success);
    void resolveDeferredPcfFallback(bool matched, PollResult &result);
    void completeDeferredRtcRead(bool read_ok, PollResult &result);
    void prepareDeferredRtcFinish(PollResult &result);
    void markDeferredRtcWriteSuccess();
    PollResult pollDeferredRtcInit(uint32_t now_ms, const RtcAccess &access);
    void finishDeferredRtcAttempt(bool initialized, PollResult &result);
    bool detectRtc(const RtcAccess &access);
    bool readRtcInitState(tm &utc_tm,
                          bool &osc_stop,
                          bool &time_valid,
                          const RtcAccess &access);
    bool retryWeakDs3231AsPcf8523(tm &utc_tm,
                                  bool &osc_stop,
                                  bool &time_valid,
                                  const RtcAccess &access);
    bool rtcBegin(const RtcAccess &access);
    bool rtcReadTime(tm &out,
                     bool &osc_stop,
                     bool &valid,
                     const RtcAccess &access);
    bool rtcWriteTime(const tm &utc_tm, const RtcAccess &access);
    bool rtcClearLostPower(const RtcAccess &access);
    bool rtcReadBatteryLow(bool &low, const RtcAccess &access);
    bool requestNtpSync();
    bool syncNtpWithWifi();
    PollResult ntpPoll(uint32_t now_ms, bool rtc_i2c_available);
    PollResult pollRtcStatus(uint32_t now_ms, const RtcAccess &access);
    void noteRtcReadSuccess(bool log_transition);
    bool noteRtcReadFailure(bool log_transition);
    bool applyRtcBatteryLowState(bool battery_low, bool log_transition);
    void stopNtpService();
    static void buildTimezonePosix(const TimeZoneEntry &tz, char *out, size_t len);

    StorageManager *storage_ = nullptr;
    SharedI2cRuntimeGate::Gate shared_i2c_runtime_gate_;
    Pcf8523 pcf8523_;
    Ds3231 ds3231_;
    RtcType rtc_type_ = RtcType::None;
    Config::RtcMode rtc_mode_ = Config::RtcMode::Auto;

    bool rtc_present_ = false;
    bool rtc_initialized_ = false;
    bool rtc_valid_ = false;
    bool rtc_lost_power_ = false;
    bool rtc_time_unset_ = false;
    bool rtc_battery_low_ = false;
    bool rtc_probe_needs_pcf_verification_ = false;
    bool rtc_deferred_begin_ok_ = false;
    bool rtc_deferred_verifying_weak_ds_ = false;
    bool rtc_deferred_require_read_success_ = false;
    bool rtc_deferred_any_read_ok_ = false;
    bool rtc_deferred_last_osc_stop_ = false;
    bool rtc_deferred_last_time_valid_ = false;
    bool rtc_deferred_ds_calendar_ok_ = false;
    bool rtc_deferred_ds_calendar_valid_ = false;
    RtcDeferredInitPhase rtc_deferred_init_phase_ = RtcDeferredInitPhase::None;
    uint32_t rtc_deferred_read_due_ms_ = 0;
    uint8_t rtc_deferred_read_attempts_ = 0;
    uint8_t rtc_deferred_ds_status_ = 0;
    Ds3231::ProbeStrength rtc_deferred_ds_probe_ = Ds3231::ProbeStrength::None;
    tm rtc_deferred_tm_ = {};
    tm rtc_deferred_ds_calendar_tm_ = {};
    uint8_t rtc_deferred_ds_probe_meta_[4] = { 0 };
    uint8_t rtc_deferred_ds_probe_wrap_[4] = { 0 };
    uint8_t rtc_deferred_ds_probe_head_[2] = { 0 };
    uint8_t rtc_deferred_pcf_control_[3] = { 0 };
    uint8_t rtc_deferred_pcf_time_[7] = { 0 };
    uint8_t rtc_deferred_pcf_timers_[5] = { 0 };

    bool ntp_enabled_pref_ = true;
    String ntp_server_pref_;
    bool ntp_enabled_ = true;
    bool ntp_syncing_ = false;
    bool ntp_err_ = false;
    uint32_t ntp_last_sync_ms_ = 0;
    uint32_t ntp_last_attempt_ms_ = 0;
    uint32_t ntp_sync_start_ms_ = 0;
    uint32_t last_rtc_restore_ms_ = 0;
    uint32_t last_rtc_status_poll_ms_ = 0;
    uint8_t rtc_read_fail_count_ = 0;
    StartupProbePolicy::State rtc_probe_;
    bool rtc_pending_write_ = false;
    time_t rtc_pending_write_epoch_ = 0;
    uint32_t rtc_pending_write_due_ms_ = 0;

    bool wifi_enabled_ = false;
    bool wifi_connected_ = false;

    int tz_index_ = 0;
};
