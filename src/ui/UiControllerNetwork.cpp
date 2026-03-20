// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"
#include "ui/UiText.h"

#include <WiFi.h>
#include <string.h>

#include "config/AppConfig.h"
#include "modules/NetworkManager.h"
#include "modules/MqttManager.h"
#include "ui/ui.h"
#include "ui/images.h"
#include "web/WebWifiUtils.h"
using namespace Config;

namespace {

const char *rtc_runtime_status_text(const TimeManager &timeManager) {
    if (!timeManager.isRtcPresent()) {
        return UiText::RtcDetectionStatusNotFound();
    }
    if (timeManager.isRtcBatteryLow()) {
        return UiText::RtcDetectionStatusBatteryLow();
    }
    if (timeManager.isRtcLostPower()) {
        return UiText::RtcDetectionStatusLostPower();
    }
    if (!timeManager.isRtcValid()) {
        return UiText::RtcDetectionStatusTimeInvalid();
    }
    return UiText::RtcDetectionStatusOk();
}

lv_color_t rtc_runtime_status_color(const TimeManager &timeManager) {
    if (!timeManager.isRtcPresent()) {
        return lv_color_hex(0xffeb3b);
    }
    if (timeManager.isRtcBatteryLow() || timeManager.isRtcLostPower()) {
        return lv_color_hex(0xffeb3b);
    }
    if (!timeManager.isRtcValid()) {
        return lv_color_hex(0xff1100);
    }
    return lv_color_hex(0x00c853);
}

WebUiBridge::ApplyResult finalize_network_bridge_result(bool success,
                                                        uint16_t status_code,
                                                        const char *message) {
    WebUiBridge::ApplyResult result{};
    result.success = success;
    result.status_code = status_code;
    result.error_message = message ? message : "";
    return result;
}

} // namespace

void UiController::update_datetime_ui() {
    const bool controls_enabled = !timeManager.isManualLocked(millis());
    // Keep editable fields in sync with system time unless user is actively editing.
    if (!datetime_changed || !controls_enabled) {
        timeManager.syncInputsFromSystem(set_hour, set_minute, set_day, set_month, set_year);
    }

    if (objects.label_ntp_interval) {
        safe_label_set_text(objects.label_ntp_interval, UiText::NtpInterval());
    }

    const TimeZoneEntry &tz = timeManager.getTimezone();
    char offset_buf[12];
    TimeManager::formatTzOffset(timeManager.currentUtcOffsetMinutes(), offset_buf, sizeof(offset_buf));
    if (objects.label_tz_offset_value) {
        safe_label_set_text(objects.label_tz_offset_value, offset_buf);
    }
    if (objects.label_tz_name) {
        safe_label_set_text(objects.label_tz_name, tz.name);
    }

    const lv_color_t text_on = active_text_color();
    const lv_color_t text_off = color_inactive();
    set_button_enabled(objects.btn_set_time_hours_minus, controls_enabled);
    set_button_enabled(objects.btn_set_time_hours_plus, controls_enabled);
    set_button_enabled(objects.btn_set_time_minutes_minus, controls_enabled);
    set_button_enabled(objects.btn_set_time_minutes_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_day_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_day_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_month_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_month_plus, controls_enabled);
    set_button_enabled(objects.btn_set_date_year_minus, controls_enabled);
    set_button_enabled(objects.btn_set_date_year_plus, controls_enabled);
    set_button_enabled(objects.btn_datetime_apply, controls_enabled);

    if (objects.label_set_time_hours_value) {
        lv_obj_set_style_text_color(objects.label_set_time_hours_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_time_minutes_value) {
        lv_obj_set_style_text_color(objects.label_set_time_minutes_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_day_value) {
        lv_obj_set_style_text_color(objects.label_set_date_day_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_month_value) {
        lv_obj_set_style_text_color(objects.label_set_date_month_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.label_set_date_year_value) {
        lv_obj_set_style_text_color(objects.label_set_date_year_value, controls_enabled ? text_on : text_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", set_hour);
    if (objects.label_set_time_hours_value) safe_label_set_text(objects.label_set_time_hours_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_minute);
    if (objects.label_set_time_minutes_value) safe_label_set_text(objects.label_set_time_minutes_value, buf);

    int max_day = TimeManager::daysInMonth(set_year, set_month);
    if (set_day > max_day) set_day = max_day;
    if (set_day < 1) set_day = 1;
    snprintf(buf, sizeof(buf), "%02d", set_day);
    if (objects.label_set_date_day_value) safe_label_set_text(objects.label_set_date_day_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_month);
    if (objects.label_set_date_month_value) safe_label_set_text(objects.label_set_date_month_value, buf);
    snprintf(buf, sizeof(buf), "%02d", set_year % 100);
    if (objects.label_set_date_year_value) safe_label_set_text(objects.label_set_date_year_value, buf);

    if (objects.btn_ntp_toggle) {
        ntp_toggle_syncing = true;
        if (timeManager.isNtpEnabled()) lv_obj_add_state(objects.btn_ntp_toggle, LV_STATE_CHECKED);
        else lv_obj_clear_state(objects.btn_ntp_toggle, LV_STATE_CHECKED);
        ntp_toggle_syncing = false;
    }
    refreshConnectivitySnapshot();
    set_button_enabled(objects.btn_ntp_toggle, connectivity_.wifi_enabled);

    TimeManager::NtpUiState ntp_state = timeManager.getNtpUiState(millis());
    lv_color_t ntp_color = color_yellow();
    const char *ntp_label = UiText::StatusOff();
    if (ntp_state == TimeManager::NTP_UI_SYNCING) {
        ntp_color = color_blue();
        ntp_label = UiText::StatusUpd();
    } else if (ntp_state == TimeManager::NTP_UI_OK) {
        ntp_color = color_green();
        ntp_label = UiText::StatusOk();
    } else if (ntp_state == TimeManager::NTP_UI_ERR) {
        ntp_color = color_red();
        ntp_label = UiText::StatusErr();
    }
    if (objects.dot_ntp_status) {
        set_dot_color(objects.dot_ntp_status, ntp_color);
    }
    if (objects.label_ntp_status) {
        safe_label_set_text(objects.label_ntp_status, ntp_label);
    }
    if (objects.chip_ntp_status) {
        set_chip_color(objects.chip_ntp_status, ntp_color);
    }

    if (objects.label_rtc_status) {
        if (!timeManager.isRtcPresent()) {
            safe_label_set_text(objects.label_rtc_status, UiText::StatusOff());
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_yellow());
        } else if (!timeManager.isRtcValid()) {
            safe_label_set_text(objects.label_rtc_status, UiText::StatusErr());
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_red());
        } else if (timeManager.isRtcBatteryLow()) {
            safe_label_set_text(objects.label_rtc_status, UiText::StatusLow());
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_yellow());
        } else {
            safe_label_set_text(objects.label_rtc_status, UiText::StatusOk());
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_green());
        }
    }

    update_rtc_detection_ui();

    if (objects.label_wifi_status_1) {
        if (!connectivity_.wifi_enabled) {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusOff());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_yellow());
        } else if (connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED)) {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusOk());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_green());
        } else {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusConn());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_blue());
        }
    }
}

WebUiBridge::ApplyResult UiController::applyWifiSaveBridge(
    const WebUiBridge::WifiSaveUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    if (!controller) {
        return finalize_network_bridge_result(false, 503, "WiFi bridge unavailable");
    }
    if (!controller->storage.saveWiFiSettings(update.ssid, update.pass, update.enabled)) {
        WebUiBridge::ApplyResult result =
            finalize_network_bridge_result(false, 500, "Failed to persist WiFi settings");
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    }
    NetworkCommandQueue::WifiSettingsUpdate settings_update{};
    settings_update.ssid = update.ssid;
    settings_update.pass = update.pass;
    settings_update.enabled = update.enabled;
    if (!controller->networkCommandQueue.publishSavedWifiSettings(settings_update)) {
        WebUiBridge::ApplyResult result =
            finalize_network_bridge_result(false, 503, "Failed to apply WiFi settings");
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    }
    controller->markWebPagePanelDirty();
    controller->data_dirty = true;
    WebUiBridge::ApplyResult result = finalize_network_bridge_result(true, 200, nullptr);
    result.snapshot = controller->buildWebUiSnapshot();
    controller->publishWebUiSnapshot();
    return result;
}

WebUiBridge::ApplyResult UiController::applyMqttSaveBridge(
    const WebUiBridge::MqttSaveUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    if (!controller) {
        return finalize_network_bridge_result(false, 503, "MQTT bridge unavailable");
    }
    if (!controller->storage.saveMqttSettings(update.host,
                                              update.port,
                                              update.user,
                                              update.pass,
                                              update.base_topic,
                                              update.device_name,
                                              update.discovery,
                                              update.anonymous)) {
        WebUiBridge::ApplyResult result =
            finalize_network_bridge_result(false, 500, "Failed to persist MQTT settings");
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    }
    NetworkCommandQueue::MqttSettingsUpdate settings_update{};
    settings_update.host = update.host;
    settings_update.port = update.port;
    settings_update.user = update.user;
    settings_update.pass = update.pass;
    settings_update.base_topic = update.base_topic;
    settings_update.device_name = update.device_name;
    settings_update.discovery = update.discovery;
    settings_update.anonymous = update.anonymous;
    if (!controller->networkCommandQueue.publishSavedMqttSettings(settings_update)) {
        WebUiBridge::ApplyResult result =
            finalize_network_bridge_result(false, 503, "Failed to apply MQTT settings");
        result.snapshot = controller->buildWebUiSnapshot();
        controller->publishWebUiSnapshot();
        return result;
    }
    controller->markWebPagePanelDirty();
    controller->data_dirty = true;
    WebUiBridge::ApplyResult result = finalize_network_bridge_result(true, 200, nullptr);
    result.snapshot = controller->buildWebUiSnapshot();
    controller->publishWebUiSnapshot();
    return result;
}

bool UiController::rtc_detection_overlay_visible() const {
    return objects.container_rtc_detection &&
           !lv_obj_has_flag(objects.container_rtc_detection, LV_OBJ_FLAG_HIDDEN);
}

void UiController::open_rtc_detection_overlay() {
    if (!objects.container_rtc_detection) {
        return;
    }
    rtc_detection_saved_mode_ = storage.config().rtc_mode;
    rtc_detection_pending_mode_ = rtc_detection_saved_mode_;
    lv_obj_clear_flag(objects.container_rtc_detection, LV_OBJ_FLAG_HIDDEN);
    datetime_ui_dirty = true;
}

void UiController::close_rtc_detection_overlay() {
    if (!objects.container_rtc_detection) {
        return;
    }
    lv_obj_add_flag(objects.container_rtc_detection, LV_OBJ_FLAG_HIDDEN);
    datetime_ui_dirty = true;
}

void UiController::set_rtc_detection_pending_mode(Config::RtcMode mode) {
    rtc_detection_pending_mode_ = mode;
    datetime_ui_dirty = true;
}

void UiController::update_rtc_detection_ui() {
    if (!objects.container_rtc_detection) {
        return;
    }
    if (!rtc_detection_overlay_visible()) {
        return;
    }

    const auto apply_selection = [&](lv_obj_t *chip, bool selected) {
        if (!chip) {
            return;
        }
        if (selected) {
            lv_obj_add_state(chip, LV_STATE_CHECKED);
            set_chip_color(chip, color_green());
        } else {
            lv_obj_clear_state(chip, LV_STATE_CHECKED);
            set_chip_color(chip, color_inactive());
        }
    };

    apply_selection(objects.chip_rtc_detection_auto,
                    rtc_detection_pending_mode_ == Config::RtcMode::Auto);
    apply_selection(objects.chip_rtc_detection_pcf8523,
                    rtc_detection_pending_mode_ == Config::RtcMode::Pcf8523);
    apply_selection(objects.chip_rtc_detection_ds3231,
                    rtc_detection_pending_mode_ == Config::RtcMode::Ds3231);

    if (objects.label_rtc_detection_title_2) {
        char line[96];
        if (rtc_detection_pending_mode_ == Config::RtcMode::Auto) {
            const char *detected = timeManager.isRtcPresent() ? timeManager.rtcLabel() : UiText::RtcDetectionNone();
            snprintf(line, sizeof(line), UiText::RtcDetectionDetectedFmt(), detected);
        } else {
            snprintf(line,
                     sizeof(line),
                     UiText::RtcDetectionSelectedFmt(),
                     TimeManager::rtcModeLabel(rtc_detection_pending_mode_));
        }
        safe_label_set_text(objects.label_rtc_detection_title_2, line);
        lv_obj_set_style_text_color(objects.label_rtc_detection_title_2,
                                    active_text_color(),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (objects.label_rtc_detection_title_3) {
        const bool pending_changed = rtc_detection_pending_mode_ != rtc_detection_saved_mode_;
        const char *status_text = pending_changed
                                      ? UiText::RtcDetectionStatusRestartRequired()
                                      : rtc_runtime_status_text(timeManager);
        const lv_color_t status_color = pending_changed
                                            ? color_yellow()
                                            : rtc_runtime_status_color(timeManager);
        safe_label_set_text(objects.label_rtc_detection_title_3, status_text);
        lv_obj_set_style_text_color(objects.label_rtc_detection_title_3,
                                    status_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void UiController::update_wifi_texts() {
    refreshConnectivitySnapshot();
    if (objects.label_wifi_title) safe_label_set_text(objects.label_wifi_title, UiText::LabelWifiSettingsTitle());
    if (objects.label_wifi_status) safe_label_set_text(objects.label_wifi_status, UiText::LabelWifiStatus());
    if (objects.label_wifi_help) {
        String help_text = UiText::LabelWifiHelp();
        const String &ap_ssid = connectivity_.ap_ssid;
        help_text.replace(Config::WIFI_AP_SSID, ap_ssid);
        help_text.replace("ProjectAura-Setup", ap_ssid);
        safe_label_set_text(objects.label_wifi_help, help_text.c_str());
    }
    if (objects.label_wifi_ssid) safe_label_set_text(objects.label_wifi_ssid, UiText::LabelWifiSsid());
    if (objects.label_wifi_ip) safe_label_set_text(objects.label_wifi_ip, UiText::LabelWifiIp());
    if (objects.label_btn_wifi_toggle) safe_label_set_text(objects.label_btn_wifi_toggle, UiText::MqttToggleLabel());
    if (objects.label_btn_wifi_forget) safe_label_set_text(objects.label_btn_wifi_forget, UiText::LabelWifiForget());
    if (objects.label_btn_wifi_reconnect) safe_label_set_text(objects.label_btn_wifi_reconnect, UiText::LabelWifiReconnect());
    if (objects.label_btn_wifi_start_ap) safe_label_set_text(objects.label_btn_wifi_start_ap, UiText::LabelWifiStartAp());
    if (objects.label_btn_wifi_back) safe_label_set_text(objects.label_btn_wifi_back, UiText::LabelSettingsBack());
}

void UiController::update_wifi_ui() {
    refreshConnectivitySnapshot();
    const bool wifi_enabled = connectivity_.wifi_enabled;
    const int wifi_state = connectivity_.wifi_state;
    const String &wifi_ssid = connectivity_.wifi_ssid;
    const uint8_t wifi_retry_count = connectivity_.wifi_retry_count;

    // page_wifi is created lazily, so these runtime style/flag guards must be
    // applied here (not only in init_ui_defaults()) once objects actually exist.
    if (objects.btn_wifi_reconnect) {
        lv_obj_clear_flag(objects.btn_wifi_reconnect, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_bg_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.btn_wifi_reconnect, color_inactive(),
                                  LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(objects.btn_wifi_reconnect, color_inactive(),
                                      LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_reconnect, color_inactive(),
                                      LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.label_btn_wifi_reconnect) {
        lv_obj_set_style_text_color(objects.label_btn_wifi_reconnect, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.label_btn_wifi_reconnect, color_inactive(),
                                    LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.btn_wifi_start_ap) {
        lv_obj_clear_flag(objects.btn_wifi_start_ap, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_bg_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.btn_wifi_start_ap, color_inactive(),
                                  LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(objects.btn_wifi_start_ap, color_inactive(),
                                      LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_color(objects.btn_wifi_start_ap, color_inactive(),
                                      LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }
    if (objects.label_btn_wifi_start_ap) {
        lv_obj_set_style_text_color(objects.label_btn_wifi_start_ap, color_inactive(), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.label_btn_wifi_start_ap, color_inactive(),
                                    LV_PART_MAIN | LV_STATE_DISABLED | LV_STATE_CHECKED);
    }

    if (objects.label_wifi_status_value) {
        const char *status = UiText::StatusOff();
        if (wifi_enabled) {
            if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED)) status = UiText::WifiStatusConnected();
            else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) status = UiText::WifiStatusApMode();
            else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_OFF) &&
                     wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) status = UiText::WifiStatusError();
            else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTING) ||
                     wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_OFF)) status = UiText::WifiStatusConnecting();
        }
        safe_label_set_text(objects.label_wifi_status_value, status);
    }
    if (objects.container_wifi_status) {
        apply_toggle_style(objects.container_wifi_status);
        if (wifi_enabled && wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            lv_obj_add_state(objects.container_wifi_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_wifi_status, LV_STATE_CHECKED);
        }
    }

    if (objects.label_wifi_ssid_value) {
        String safe_ssid;
        const String &ap_ssid = connectivity_.ap_ssid;
        const char *ssid_text = UiText::ValueMissing();
        if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED) && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) {
            ssid_text = ap_ssid.c_str();
        } else if (wifi_enabled && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        }
        safe_label_set_text(objects.label_wifi_ssid_value, ssid_text);
    }

    if (objects.label_wifi_ip_value) {
        String ip = UiText::ValueMissing();
        if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED)) {
            ip = connectivity_.sta_ip;
        } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) {
            ip = connectivity_.ap_ip;
        }
        safe_label_set_text(objects.label_wifi_ip_value, ip.c_str());
    }
    if (objects.qrcode_wifi_portal) {
        if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) {
            lv_obj_clear_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
            update_qrcode_if_needed(objects.qrcode_wifi_portal,
                                    UiText::WifiPortalUrl(),
                                    wifi_portal_qr_cache_,
                                    sizeof(wifi_portal_qr_cache_));
        } else {
            lv_obj_add_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (objects.btn_wifi_reconnect) {
        bool can_reconnect = wifi_enabled && !wifi_ssid.isEmpty();
        if (!can_reconnect) {
            lv_obj_clear_state(objects.btn_wifi_reconnect, LV_STATE_CHECKED);
        }
        set_button_enabled(objects.btn_wifi_reconnect, can_reconnect);
        set_button_enabled(objects.label_btn_wifi_reconnect, can_reconnect);
    }
    if (objects.btn_wifi_start_ap) {
        if (wifi_enabled && wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) {
            lv_obj_add_state(objects.btn_wifi_start_ap, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.btn_wifi_start_ap, LV_STATE_CHECKED);
        }
        set_button_enabled(objects.btn_wifi_start_ap, wifi_enabled);
        set_button_enabled(objects.label_btn_wifi_start_ap, wifi_enabled);
    }
    sync_wifi_toggle_state();
}

void UiController::update_status_icons() {
    refreshConnectivitySnapshot();
    // WiFi icon states: 0=hidden, 1=green, 2=blue, 3=yellow, 4=red
    int new_wifi_state = 0;
    const bool wifi_enabled = connectivity_.wifi_enabled;
    const int wifi_state = connectivity_.wifi_state;
    const uint8_t wifi_retry_count = connectivity_.wifi_retry_count;

    if (!wifi_enabled) {
        new_wifi_state = 0; // hidden
    } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED)) {
        new_wifi_state = 1; // green
    } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTING)) {
        new_wifi_state = 2; // blue
    } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG)) {
        new_wifi_state = 3; // yellow
    } else if (wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_OFF) &&
               wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) {
        new_wifi_state = 4; // red
    }

    int main_wifi_state = new_wifi_state;
    if (night_mode && main_wifi_state != 4) {
        main_wifi_state = 0;
    }
    if (main_wifi_state != wifi_icon_state_main) {
        wifi_icon_state_main = main_wifi_state;
        lv_obj_t *main_wifi_icons[] = {
            objects.wifi_status_icon_4
        };
        const size_t main_wifi_icon_count = sizeof(main_wifi_icons) / sizeof(main_wifi_icons[0]);
        for (size_t i = 0; i < main_wifi_icon_count; ++i) {
            lv_obj_t *icon = main_wifi_icons[i];
            if (!icon) {
                continue;
            }
            if (wifi_icon_state_main == 0) {
                lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
                if (wifi_icon_state_main == 1) {
                    lv_img_set_src(icon, &img_wifi_green);
                } else if (wifi_icon_state_main == 2) {
                    lv_img_set_src(icon, &img_wifi_blue);
                } else if (wifi_icon_state_main == 3) {
                    lv_img_set_src(icon, &img_wifi_yellow);
                } else if (wifi_icon_state_main == 4) {
                    lv_img_set_src(icon, &img_wifi_red);
                }
            }
        }
    }

    if (new_wifi_state != wifi_icon_state) {
        wifi_icon_state = new_wifi_state;

        lv_obj_t *wifi_icons[] = {
            objects.wifi_status_icon_1,
            objects.wifi_status_icon_2,
            objects.wifi_status_icon_3
        };
        const size_t wifi_icon_count = sizeof(wifi_icons) / sizeof(wifi_icons[0]);
        for (size_t i = 0; i < wifi_icon_count; i++) {
            if (wifi_icons[i]) {
                if (wifi_icon_state == 0) {
                    lv_obj_add_flag(wifi_icons[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_clear_flag(wifi_icons[i], LV_OBJ_FLAG_HIDDEN);
                    if (wifi_icon_state == 1) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_green);
                    } else if (wifi_icon_state == 2) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_blue);
                    } else if (wifi_icon_state == 3) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_yellow);
                    } else if (wifi_icon_state == 4) {
                        lv_img_set_src(wifi_icons[i], &img_wifi_red);
                    }
                }
            }
        }
    }

    // MQTT icon states: 0=hidden, 1=green, 2=blue, 3=red, 4=yellow
    int new_mqtt_state = 0;

    if (!connectivity_.mqtt_enabled || !wifi_enabled ||
        wifi_state != static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED)) {
        new_mqtt_state = 0; // hidden - MQTT disabled or WiFi not ready
    } else if (connectivity_.mqtt_connected) {
        new_mqtt_state = 1; // green
    } else {
        const uint8_t retry_stage = connectivity_.mqtt_retry_stage;
        if (retry_stage >= 2) {
            new_mqtt_state = 3; // red
        } else if (retry_stage >= 1) {
            new_mqtt_state = 4; // yellow
        } else {
            new_mqtt_state = 2; // blue
        }
    }

    int main_mqtt_state = new_mqtt_state;
    if (night_mode && main_mqtt_state != 3) {
        main_mqtt_state = 0;
    }
    if (main_mqtt_state != mqtt_icon_state_main) {
        mqtt_icon_state_main = main_mqtt_state;
        lv_obj_t *main_mqtt_icons[] = {
            objects.mqtt_status_icon_4
        };
        const size_t main_mqtt_icon_count = sizeof(main_mqtt_icons) / sizeof(main_mqtt_icons[0]);
        for (size_t i = 0; i < main_mqtt_icon_count; ++i) {
            lv_obj_t *icon = main_mqtt_icons[i];
            if (!icon) {
                continue;
            }
            if (mqtt_icon_state_main == 0) {
                lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
                if (mqtt_icon_state_main == 1) {
                    lv_img_set_src(icon, &img_home_green);
                } else if (mqtt_icon_state_main == 2) {
                    lv_img_set_src(icon, &img_home_blue);
                } else if (mqtt_icon_state_main == 3) {
                    lv_img_set_src(icon, &img_home_red);
                } else if (mqtt_icon_state_main == 4) {
                    lv_img_set_src(icon, &img_home_yellow);
                }
            }
        }
    }

    if (new_mqtt_state != mqtt_icon_state) {
        mqtt_icon_state = new_mqtt_state;

        lv_obj_t *mqtt_icons[] = {
            objects.mqtt_status_icon_1,
            objects.mqtt_status_icon_2,
            objects.mqtt_status_icon_3
        };
        const size_t mqtt_icon_count = sizeof(mqtt_icons) / sizeof(mqtt_icons[0]);
        for (size_t i = 0; i < mqtt_icon_count; i++) {
            if (mqtt_icons[i]) {
                if (mqtt_icon_state == 0) {
                    lv_obj_add_flag(mqtt_icons[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_clear_flag(mqtt_icons[i], LV_OBJ_FLAG_HIDDEN);
                    if (mqtt_icon_state == 1) {
                        lv_img_set_src(mqtt_icons[i], &img_home_green);
                    } else if (mqtt_icon_state == 2) {
                        lv_img_set_src(mqtt_icons[i], &img_home_blue);
                    } else if (mqtt_icon_state == 3) {
                        lv_img_set_src(mqtt_icons[i], &img_home_red);
                    } else if (mqtt_icon_state == 4) {
                        lv_img_set_src(mqtt_icons[i], &img_home_yellow);
                    }
                }
            }
        }
    }
}

void UiController::update_mqtt_ui() {
    refreshConnectivitySnapshot();
    const bool wifi_ready = connectivity_.wifi_enabled && connectivity_.wifi_connected;
    const String &local_mqtt_url = connectivity_.mqtt_local_url;
    String ip_mqtt_url = "http://<device-ip>/mqtt";
    if (wifi_ready) {
        ip_mqtt_url = connectivity_.mqtt_sta_url;
    }

    // Update MQTT status label
    if (objects.label_mqtt_status_value) {
        const char *status = UiText::MqttStatusDisabled();
        if (connectivity_.mqtt_user_enabled) {
            if (!wifi_ready) {
                status = UiText::MqttStatusNoWifi();
            } else if (connectivity_.mqtt_connected) {
                status = UiText::MqttStatusConnected();
            } else {
                const uint8_t retry_stage = connectivity_.mqtt_retry_stage;
                if (retry_stage >= 2) {
                    status = UiText::MqttStatusRetry1h();
                } else if (retry_stage >= 1) {
                    status = UiText::MqttStatusRetry10m();
                } else {
                    status = UiText::MqttStatusConnecting();
                }
            }
        }
        safe_label_set_text(objects.label_mqtt_status_value, status);
    }

    // Update MQTT status container style
    if (objects.container_mqtt_status) {
        apply_toggle_style(objects.container_mqtt_status);
        if (connectivity_.mqtt_enabled && connectivity_.mqtt_connected) {
            lv_obj_add_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        }
    }

    // Update Broker IP
    if (objects.label_mqtt_broker_value) {
        String broker_addr = UiText::ValueMissing();
        if (connectivity_.mqtt_user_enabled && !connectivity_.mqtt_host.isEmpty()) {
            broker_addr = connectivity_.mqtt_host + ":" + String(connectivity_.mqtt_port);
        }
        safe_label_set_text(objects.label_mqtt_broker_value, broker_addr.c_str());
    }

    // Update Device IP
    if (objects.label_mqtt_device_ip_value) {
        String device_ip = UiText::ValueMissing();
        if (connectivity_.wifi_connected && !connectivity_.sta_ip.isEmpty()) {
            device_ip = connectivity_.sta_ip;
        }
        safe_label_set_text(objects.label_mqtt_device_ip_value, device_ip.c_str());
    }

    // Update Topic
    if (objects.label_mqtt_topic_value) {
        String topic = UiText::ValueMissing();
        if (connectivity_.mqtt_user_enabled && !connectivity_.mqtt_base_topic.isEmpty()) {
            topic = connectivity_.mqtt_base_topic;
        }
        safe_label_set_text(objects.label_mqtt_topic_value, topic.c_str());
    }

    // Update QR code - show only when WiFi is connected.
    if (objects.qrcode_mqtt_portal) {
        if (wifi_ready) {
            lv_obj_clear_flag(objects.qrcode_mqtt_portal, LV_OBJ_FLAG_HIDDEN);
            update_qrcode_if_needed(objects.qrcode_mqtt_portal,
                                    ip_mqtt_url.c_str(),
                                    mqtt_portal_qr_cache_,
                                    sizeof(mqtt_portal_qr_cache_));
        } else {
            lv_obj_add_flag(objects.qrcode_mqtt_portal, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update toggle button text and state
    if (objects.label_btn_mqtt_toggle) {
        safe_label_set_text(objects.label_btn_mqtt_toggle, UiText::MqttToggleLabel());
    }
    sync_mqtt_toggle_state();
    set_button_enabled(objects.btn_mqtt_toggle, wifi_ready);
    set_button_enabled(objects.btn_mqtt, wifi_ready);

    // Update reconnect button state
    if (objects.btn_mqtt_reconnect) {
        bool can_reconnect = connectivity_.mqtt_enabled && wifi_ready;
        if (can_reconnect) {
            lv_obj_clear_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        }
    }
}

void UiController::update_mqtt_texts() {
    refreshConnectivitySnapshot();
    if (objects.label_mqtt_title) safe_label_set_text(objects.label_mqtt_title, UiText::LabelMqttSettingsTitle());
    if (objects.label_mqtt_status) safe_label_set_text(objects.label_mqtt_status, UiText::LabelMqttStatus());
    if (objects.label_mqtt_help) {
        String help_text = UiText::LabelMqttHelp();
        const String &local_url = connectivity_.mqtt_local_url;
        String ip_url = "http://<device-ip>/mqtt";
        const bool sta_mode =
            connectivity_.wifi_enabled &&
            connectivity_.wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED);
        if (sta_mode) {
            ip_url = connectivity_.mqtt_sta_url;
        }
        help_text.replace("{{IP_URL}}", ip_url);
        help_text.replace("{{LOCAL_URL}}", local_url);
        help_text.replace(UiText::MqttPortalUrl(), local_url);
        help_text.replace("http://aura.local/mqtt", local_url);
        help_text.replace("http://<device-ip>/mqtt", ip_url);
        help_text.replace("http://<device ip>/mqtt", ip_url);
        safe_label_set_text(objects.label_mqtt_help, help_text.c_str());
    }
    if (objects.label_mqtt_device_ip) safe_label_set_text(objects.label_mqtt_device_ip, UiText::LabelMqttDeviceIp());
    if (objects.label_mqtt_broker) safe_label_set_text(objects.label_mqtt_broker, UiText::LabelMqttBroker());
    if (objects.label_mqtt_topic) safe_label_set_text(objects.label_mqtt_topic, UiText::LabelMqttTopic());
    if (objects.label_btn_mqtt_toggle) safe_label_set_text(objects.label_btn_mqtt_toggle, UiText::MqttToggleLabel());
    if (objects.label_btn_mqtt_reconnect) safe_label_set_text(objects.label_btn_mqtt_reconnect, UiText::LabelMqttReconnect());
    if (objects.label_btn_mqtt_back) safe_label_set_text(objects.label_btn_mqtt_back, UiText::LabelSettingsBack());
}

void UiController::update_datetime_texts() {
    if (objects.label_datetime_title) safe_label_set_text(objects.label_datetime_title, UiText::LabelDateTimeTitle());
    if (objects.label_timezone_title) safe_label_set_text(objects.label_timezone_title, UiText::LabelTimeZone());
    if (objects.label_ntp_title) safe_label_set_text(objects.label_ntp_title, UiText::LabelNtpAutoSync());
    if (objects.label_btn_ntp_toggle) safe_label_set_text(objects.label_btn_ntp_toggle, UiText::MqttToggleLabel());
    if (objects.label_set_time_title) safe_label_set_text(objects.label_set_time_title, UiText::LabelSetTime());
    if (objects.label_set_time_hours_title) safe_label_set_text(objects.label_set_time_hours_title, UiText::LabelSetTimeHours());
    if (objects.label_set_time_minutes_title) safe_label_set_text(objects.label_set_time_minutes_title, UiText::LabelSetTimeMinutes());
    if (objects.label_set_date_title) safe_label_set_text(objects.label_set_date_title, UiText::LabelSetDate());
    if (objects.label_set_date_day_title) safe_label_set_text(objects.label_set_date_day_title, UiText::LabelSetDateDay());
    if (objects.label_set_date_month_title) safe_label_set_text(objects.label_set_date_month_title, UiText::LabelSetDateMonth());
    if (objects.label_set_date_year_title) safe_label_set_text(objects.label_set_date_year_title, UiText::LabelSetDateYear());
    if (objects.label_time_title_1) safe_label_set_text(objects.label_time_title_1, UiText::LabelTimeCard());
    if (objects.label_btn_datetime_apply) safe_label_set_text(objects.label_btn_datetime_apply, UiText::LabelApplyNow());
    if (objects.label_rtc_title) safe_label_set_text(objects.label_rtc_title, UiText::LabelRtc());
    if (objects.label_wifi_title_1) safe_label_set_text(objects.label_wifi_title_1, UiText::LabelWifiChip());
    if (objects.label_chip_ntp_title) safe_label_set_text(objects.label_chip_ntp_title, UiText::LabelNtpChip());
    if (objects.label_rtc_detection_title) safe_label_set_text(objects.label_rtc_detection_title, UiText::LabelRtcDetectionHelp());
    if (objects.label_rtc_detection_auto) safe_label_set_text(objects.label_rtc_detection_auto, UiText::LabelRtcDetectionAuto());
    if (objects.label_rtc_detection_pcf8523) safe_label_set_text(objects.label_rtc_detection_pcf8523, "PCF8523");
    if (objects.label_rtc_detection_ds3231) safe_label_set_text(objects.label_rtc_detection_ds3231, "DS3231");
    if (objects.label_rtc_detection_title_1) safe_label_set_text(objects.label_rtc_detection_title_1, UiText::LabelRtcDetectionHint());
}
