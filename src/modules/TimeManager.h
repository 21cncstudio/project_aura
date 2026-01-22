#pragma once

#include <Arduino.h>
#include <time.h>
#include "config/AppConfig.h"
#include "config/AppData.h"
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

    bool updateWifiState(bool wifi_enabled, bool wifi_connected);

    bool setNtpEnabledPref(bool enabled);
    bool isNtpEnabledPref() const { return ntp_enabled_pref_; }
    bool isNtpEnabled() const { return ntp_enabled_; }
    bool isNtpSyncing() const { return ntp_syncing_; }
    bool isNtpError() const { return ntp_err_; }
    uint32_t lastNtpSyncMs() const { return ntp_last_sync_ms_; }

    PollResult poll(uint32_t now_ms);

    NtpUiState getNtpUiState(uint32_t now_ms) const;
    bool isManualLocked(uint32_t now_ms) const;

    bool setLocalTime(int year, int month, int day, int hour, int minute);

    bool setTimezoneIndex(int index);
    bool adjustTimezone(int delta);
    int getTimezoneIndex() const { return tz_index_; }
    const TimeZoneEntry &getTimezone() const;

    bool isSystemTimeValid() const;
    bool getLocalTime(tm &out);
    bool syncInputsFromSystem(int &hour, int &minute, int &day, int &month, int &year);

    bool isRtcPresent() const { return rtc_present_; }
    bool isRtcValid() const { return rtc_valid_; }
    bool isRtcLostPower() const { return rtc_lost_power_; }

    static int findTimezoneIndex(const char *name);
    static void formatTzOffset(int offset_min, char *out, size_t len);
    static bool isLeapYear(int year);
    static int daysInMonth(int year, int month);

private:
    void applyTimezone();
    static void buildFixedTzString(int offset_min, char *out, size_t len);
    time_t makeUtcEpoch(const tm &utc_tm);
    bool setSystemTime(time_t epoch);
    bool rtcWriteFromEpoch(time_t epoch);
    bool requestNtpSync();
    bool syncNtpWithWifi();
    PollResult ntpPoll(uint32_t now_ms);

    StorageManager *storage_ = nullptr;
    Pcf8523 rtc_;

    bool rtc_present_ = false;
    bool rtc_valid_ = false;
    bool rtc_lost_power_ = false;

    bool ntp_enabled_pref_ = true;
    bool ntp_enabled_ = true;
    bool ntp_syncing_ = false;
    bool ntp_err_ = false;
    uint32_t ntp_last_sync_ms_ = 0;
    uint32_t ntp_last_attempt_ms_ = 0;
    uint32_t ntp_sync_start_ms_ = 0;
    uint32_t last_rtc_restore_ms_ = 0;

    bool wifi_enabled_ = false;
    bool wifi_connected_ = false;

    int tz_index_ = 0;
};
