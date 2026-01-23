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
#include "web/WebHandlers.h"

using namespace Config;

void UiController::update_datetime_ui() {
    if (objects.label_ntp_interval) {
        safe_label_set_text(objects.label_ntp_interval, UiText::NtpInterval());
    }

    const TimeZoneEntry &tz = timeManager.getTimezone();
    char offset_buf[12];
    TimeManager::formatTzOffset(tz.offset_min, offset_buf, sizeof(offset_buf));
    if (objects.label_tz_offset_value) {
        safe_label_set_text(objects.label_tz_offset_value, offset_buf);
    }
    if (objects.label_tz_name) {
        safe_label_set_text(objects.label_tz_name, tz.name);
    }

    const lv_color_t text_on = active_text_color();
    const lv_color_t text_off = color_inactive();
    bool controls_enabled = !timeManager.isManualLocked(millis());

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
    set_button_enabled(objects.btn_ntp_toggle, networkManager.isEnabled());

    TimeManager::NtpUiState ntp_state = timeManager.getNtpUiState(millis());
    lv_color_t ntp_color = color_yellow();
    const char *ntp_label = UiText::StatusOff();
    if (ntp_state == TimeManager::NTP_UI_SYNCING) {
        ntp_color = color_blue();
        ntp_label = UiText::StatusSync();
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
        } else {
            safe_label_set_text(objects.label_rtc_status, UiText::StatusOk());
            if (objects.chip_rtc_status) set_chip_color(objects.chip_rtc_status, color_green());
        }
    }

    if (objects.label_wifi_status_1) {
        bool wifi_enabled = networkManager.isEnabled();
        AuraNetworkManager::WifiState wifi_state = networkManager.state();
        if (!wifi_enabled) {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusOff());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_yellow());
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusOn());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_green());
        } else {
            safe_label_set_text(objects.label_wifi_status_1, UiText::StatusOn());
            if (objects.chip_wifi_status) set_chip_color(objects.chip_wifi_status, color_blue());
        }
    }
}

void UiController::update_wifi_texts() {
    if (objects.label_wifi_title) safe_label_set_text(objects.label_wifi_title, UiText::LabelWifiSettingsTitle());
    if (objects.label_wifi_status) safe_label_set_text(objects.label_wifi_status, UiText::LabelWifiStatus());
    if (objects.label_wifi_help) safe_label_set_text(objects.label_wifi_help, UiText::LabelWifiHelp());
    if (objects.label_wifi_ssid) safe_label_set_text(objects.label_wifi_ssid, UiText::LabelWifiSsid());
    if (objects.label_wifi_ip) safe_label_set_text(objects.label_wifi_ip, UiText::LabelWifiIp());
    if (objects.label_btn_wifi_toggle) safe_label_set_text(objects.label_btn_wifi_toggle, UiText::MqttToggleLabel());
    if (objects.label_btn_wifi_forget) safe_label_set_text(objects.label_btn_wifi_forget, UiText::LabelWifiForget());
    if (objects.label_btn_wifi_reconnect) safe_label_set_text(objects.label_btn_wifi_reconnect, UiText::LabelWifiReconnect());
    if (objects.label_btn_wifi_start_ap) safe_label_set_text(objects.label_btn_wifi_start_ap, UiText::LabelWifiStartAp());
    if (objects.label_btn_wifi_back) safe_label_set_text(objects.label_btn_wifi_back, UiText::LabelSettingsBack());
}

void UiController::update_wifi_ui() {
    bool wifi_enabled = networkManager.isEnabled();
    AuraNetworkManager::WifiState wifi_state = networkManager.state();
    const String &wifi_ssid = networkManager.ssid();
    uint8_t wifi_retry_count = networkManager.retryCount();
    if (objects.label_wifi_status_value) {
        const char *status = UiText::StatusOff();
        if (wifi_enabled) {
            if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) status = UiText::WifiStatusConnected();
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) status = UiText::WifiStatusApMode();
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_OFF &&
                     wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) status = UiText::WifiStatusError();
            else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTING ||
                     wifi_state == AuraNetworkManager::WIFI_STATE_OFF) status = UiText::WifiStatusConnecting();
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
        const char *ssid_text = UiText::ValueMissing();
        if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            ssid_text = WIFI_AP_SSID;
        } else if (wifi_enabled && !wifi_ssid.isEmpty()) {
            safe_ssid = wifi_label_safe(wifi_ssid);
            ssid_text = safe_ssid.c_str();
        }
        safe_label_set_text(objects.label_wifi_ssid_value, ssid_text);
    }

    if (objects.label_wifi_ip_value) {
        String ip = UiText::ValueMissing();
        if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
            ip = WiFi.localIP().toString();
        } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            ip = WiFi.softAPIP().toString();
        }
        safe_label_set_text(objects.label_wifi_ip_value, ip.c_str());
    }
    if (objects.qrcode_wifi_portal) {
        if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
            lv_obj_clear_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
            lv_qrcode_update(objects.qrcode_wifi_portal, UiText::WifiPortalUrl(), strlen(UiText::WifiPortalUrl()));
        } else {
            lv_obj_add_flag(objects.qrcode_wifi_portal, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (objects.btn_wifi_reconnect) {
        bool can_reconnect = wifi_enabled && !wifi_ssid.isEmpty();
        set_button_enabled(objects.btn_wifi_reconnect, can_reconnect);
    }
    if (objects.btn_wifi_start_ap) {
        set_button_enabled(objects.btn_wifi_start_ap, wifi_enabled);
    }
    sync_wifi_toggle_state();
}

void UiController::update_status_icons() {
    // WiFi icon states: 0=hidden, 1=green, 2=blue, 3=yellow, 4=red
    int new_wifi_state = 0;
    bool wifi_enabled = networkManager.isEnabled();
    AuraNetworkManager::WifiState wifi_state = networkManager.state();
    uint8_t wifi_retry_count = networkManager.retryCount();

    if (!wifi_enabled) {
        new_wifi_state = 0; // hidden
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
        new_wifi_state = 1; // green
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_STA_CONNECTING) {
        new_wifi_state = 2; // blue
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_AP_CONFIG) {
        new_wifi_state = 3; // yellow
    } else if (wifi_state == AuraNetworkManager::WIFI_STATE_OFF &&
               wifi_retry_count >= WIFI_CONNECT_MAX_RETRIES) {
        new_wifi_state = 4; // red
    }

    int main_wifi_state = new_wifi_state;
    if (night_mode && main_wifi_state != 4) {
        main_wifi_state = 0;
    }
    static int wifi_icon_state_main = -1;
    if (main_wifi_state != wifi_icon_state_main) {
        wifi_icon_state_main = main_wifi_state;
        if (objects.wifi_status_icon) {
            if (wifi_icon_state_main == 0) {
                lv_obj_add_flag(objects.wifi_status_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(objects.wifi_status_icon, LV_OBJ_FLAG_HIDDEN);
                if (wifi_icon_state_main == 1) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_green);
                } else if (wifi_icon_state_main == 2) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_blue);
                } else if (wifi_icon_state_main == 3) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_yellow);
                } else if (wifi_icon_state_main == 4) {
                    lv_img_set_src(objects.wifi_status_icon, &img_wifi_red);
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

    if (!mqttManager.isEnabled() || !wifi_enabled ||
        wifi_state != AuraNetworkManager::WIFI_STATE_STA_CONNECTED) {
        new_mqtt_state = 0; // hidden - MQTT disabled or WiFi not ready
    } else if (mqttManager.isConnected()) {
        new_mqtt_state = 1; // green
    } else {
        uint32_t attempts = mqttManager.connectAttempts();
        const uint32_t stage_limit = static_cast<uint32_t>(MQTT_CONNECT_MAX_FAILS);
        if (mqttManager.retryExhausted() || attempts >= stage_limit * 2) {
            new_mqtt_state = 3; // red
        } else if (attempts >= stage_limit) {
            new_mqtt_state = 4; // yellow
        } else {
            new_mqtt_state = 2; // blue
        }
    }

    int main_mqtt_state = new_mqtt_state;
    if (night_mode && main_mqtt_state != 3) {
        main_mqtt_state = 0;
    }
    static int mqtt_icon_state_main = -1;
    if (main_mqtt_state != mqtt_icon_state_main) {
        mqtt_icon_state_main = main_mqtt_state;
        if (objects.mqtt_status_icon) {
            if (mqtt_icon_state_main == 0) {
                lv_obj_add_flag(objects.mqtt_status_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(objects.mqtt_status_icon, LV_OBJ_FLAG_HIDDEN);
                if (mqtt_icon_state_main == 1) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_green);
                } else if (mqtt_icon_state_main == 2) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_blue);
                } else if (mqtt_icon_state_main == 3) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_red);
                } else if (mqtt_icon_state_main == 4) {
                    lv_img_set_src(objects.mqtt_status_icon, &img_home_yellow);
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
    bool wifi_ready = networkManager.isEnabled() && networkManager.isConnected();

    // Update MQTT status label
    if (objects.label_mqtt_status_value) {
        const char *status = UiText::MqttStatusDisabled();
        if (mqttManager.isUserEnabled()) {
            if (!wifi_ready) {
                status = UiText::MqttStatusNoWifi();
            } else if (mqttManager.isConnected()) {
                status = UiText::MqttStatusConnected();
            } else {
                uint32_t attempts = mqttManager.connectAttempts();
                const uint32_t stage_limit = static_cast<uint32_t>(MQTT_CONNECT_MAX_FAILS);
                if (mqttManager.retryExhausted()) {
                    status = UiText::MqttStatusError();
                } else if (attempts >= stage_limit * 2) {
                    status = UiText::MqttStatusRetry1h();
                } else if (attempts >= stage_limit) {
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
        if (mqttManager.isEnabled() && mqttManager.isConnected()) {
            lv_obj_add_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_mqtt_status, LV_STATE_CHECKED);
        }
    }

    // Update Broker IP
    if (objects.label_mqtt_broker_value) {
        String broker_addr = UiText::ValueMissing();
        if (mqttManager.isUserEnabled() && !mqttManager.host().isEmpty()) {
            broker_addr = mqttManager.host() + ":" + String(mqttManager.port());
        }
        safe_label_set_text(objects.label_mqtt_broker_value, broker_addr.c_str());
    }

    // Update Device IP
    if (objects.label_mqtt_device_ip_value) {
        String device_ip = UiText::ValueMissing();
        if (networkManager.isConnected()) {
            device_ip = WiFi.localIP().toString();
        }
        safe_label_set_text(objects.label_mqtt_device_ip_value, device_ip.c_str());
    }

    // Update Topic
    if (objects.label_mqtt_topic_value) {
        String topic = UiText::ValueMissing();
        if (mqttManager.isUserEnabled() && !mqttManager.baseTopic().isEmpty()) {
            topic = mqttManager.baseTopic();
        }
        safe_label_set_text(objects.label_mqtt_topic_value, topic.c_str());
    }

    // Update QR code - show only when WiFi is connected.
    if (objects.qrcode_mqtt_portal) {
        if (wifi_ready) {
            String mqtt_url = UiText::MqttPortalUrl();
            lv_obj_clear_flag(objects.qrcode_mqtt_portal, LV_OBJ_FLAG_HIDDEN);
            lv_qrcode_update(objects.qrcode_mqtt_portal, mqtt_url.c_str(), mqtt_url.length());
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
        bool can_reconnect = mqttManager.isEnabled() && wifi_ready;
        if (can_reconnect) {
            lv_obj_clear_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(objects.btn_mqtt_reconnect, LV_STATE_DISABLED);
        }
    }
}

void UiController::update_mqtt_texts() {
    if (objects.label_mqtt_title) safe_label_set_text(objects.label_mqtt_title, UiText::LabelMqttSettingsTitle());
    if (objects.label_mqtt_status) safe_label_set_text(objects.label_mqtt_status, UiText::LabelMqttStatus());
    if (objects.label_mqtt_help) safe_label_set_text(objects.label_mqtt_help, UiText::LabelMqttHelp());
    if (objects.label_mqtt_device_ip) safe_label_set_text(objects.label_mqtt_device_ip, UiText::LabelMqttDeviceIp());
    if (objects.label_mqtt_broker) safe_label_set_text(objects.label_mqtt_broker, UiText::LabelMqttBroker());
    if (objects.label_mqtt_topic) safe_label_set_text(objects.label_mqtt_topic, UiText::LabelMqttTopic());
    if (objects.label_btn_mqtt_toggle) safe_label_set_text(objects.label_btn_mqtt_toggle, UiText::MqttToggleLabel());
    if (objects.label_btn_mqtt_reconnect) safe_label_set_text(objects.label_btn_mqtt_reconnect, UiText::LabelMqttReconnect());
    if (objects.label_btn_mqtt_back) safe_label_set_text(objects.label_btn_mqtt_back, UiText::LabelSettingsBack());
}

void UiController::update_datetime_texts() {
    if (objects.label_datetime_title) safe_label_set_text(objects.label_datetime_title, UiText::LabelDateTimeTitle());
    if (objects.label_btn_datetime_back) safe_label_set_text(objects.label_btn_datetime_back, UiText::LabelSettingsBack());
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
}
