// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <math.h>
#include <stdio.h>

#include "core/Logger.h"
#include "modules/FanControl.h"
#include "modules/NetworkManager.h"
#include "modules/StorageManager.h"
#include "ui/UiText.h"
#include "ui/ui.h"

namespace {

void set_checked_state(lv_obj_t *obj, bool checked) {
    if (!obj) {
        return;
    }
    if (checked) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(obj, LV_STATE_CHECKED);
    }
}

void set_button_accent(lv_obj_t *obj, lv_color_t color, lv_opa_t shadow_opa) {
    if (!obj) {
        return;
    }
    lv_obj_set_style_border_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(obj, shadow_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
}

uint32_t hash_text(uint32_t seed, const String &text) {
    uint32_t hash = seed;
    for (size_t i = 0; i < text.length(); ++i) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 16777619UL;
    }
    return hash;
}

uint8_t manual_level_from_target(lv_obj_t *target) {
    if (target == objects.btn_dak_manual_toggle_1) return 1;
    if (target == objects.btn_dak_manual_toggle_2) return 2;
    if (target == objects.btn_dak_manual_toggle_3) return 3;
    if (target == objects.btn_dak_manual_toggle_4) return 4;
    if (target == objects.btn_dak_manual_toggle_5) return 5;
    if (target == objects.btn_dak_manual_toggle_6) return 6;
    if (target == objects.btn_dak_manual_toggle_7) return 7;
    if (target == objects.btn_dak_manual_toggle_8) return 8;
    if (target == objects.btn_dak_manual_toggle_9) return 9;
    if (target == objects.btn_dak_manual_toggle_10) return 10;
    return 0;
}

uint32_t timer_seconds_from_target(lv_obj_t *target) {
    if (target == objects.btn_dak_manual_timer_toggle_10min) return Config::DAC_TIMER_PRESETS_S[0];
    if (target == objects.btn_dak_manual_timer_toggle_30min) return Config::DAC_TIMER_PRESETS_S[1];
    if (target == objects.btn_dak_manual_timer_toggle_1hour) return Config::DAC_TIMER_PRESETS_S[2];
    if (target == objects.btn_dak_manual_timer_toggle_2hours) return Config::DAC_TIMER_PRESETS_S[3];
    if (target == objects.btn_dak_manual_timer_toggle_4hours) return Config::DAC_TIMER_PRESETS_S[4];
    if (target == objects.btn_dak_manual_timer_toggle_8hours) return Config::DAC_TIMER_PRESETS_S[5];
    return Config::DAC_TIMER_NONE_S;
}

void format_mmss(uint32_t total_seconds, char *out, size_t out_len) {
    if (total_seconds >= 3600UL) {
        const uint32_t hours = total_seconds / 3600UL;
        const uint32_t minutes = (total_seconds % 3600UL) / 60UL;
        snprintf(out, out_len, "%02lu:%02lu",
                 static_cast<unsigned long>(hours),
                 static_cast<unsigned long>(minutes));
        return;
    }
    const uint32_t minutes = total_seconds / 60;
    const uint32_t seconds = total_seconds % 60;
    snprintf(out, out_len, "%02lu:%02lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
}

uint8_t co2_auto_percent(const DacAutoSensorConfig &cfg, int co2) {
    if (co2 < Config::AQ_CO2_GREEN_MAX_PPM) {
        return cfg.band.green_percent;
    }
    if (co2 < Config::AQ_CO2_YELLOW_MAX_PPM) {
        return cfg.band.yellow_percent;
    }
    if (co2 < Config::AQ_CO2_ORANGE_MAX_PPM) {
        return cfg.band.orange_percent;
    }
    return cfg.band.red_percent;
}

uint8_t co_auto_percent(const DacAutoSensorConfig &cfg, float co_ppm) {
    if (co_ppm < Config::AQ_CO_GREEN_MAX_PPM) {
        return cfg.band.green_percent;
    }
    if (co_ppm <= Config::AQ_CO_YELLOW_MAX_PPM) {
        return cfg.band.yellow_percent;
    }
    if (co_ppm <= Config::AQ_CO_ORANGE_MAX_PPM) {
        return cfg.band.orange_percent;
    }
    return cfg.band.red_percent;
}

uint8_t pm25_auto_percent(const DacAutoSensorConfig &cfg, float pm25) {
    if (pm25 <= Config::AQ_PM25_GREEN_MAX_UGM3) {
        return cfg.band.green_percent;
    }
    if (pm25 <= Config::AQ_PM25_YELLOW_MAX_UGM3) {
        return cfg.band.yellow_percent;
    }
    if (pm25 <= Config::AQ_PM25_ORANGE_MAX_UGM3) {
        return cfg.band.orange_percent;
    }
    return cfg.band.red_percent;
}

uint8_t voc_auto_percent(const DacAutoSensorConfig &cfg, int voc_index) {
    if (voc_index <= Config::AQ_VOC_GREEN_MAX_INDEX) {
        return cfg.band.green_percent;
    }
    if (voc_index <= Config::AQ_VOC_YELLOW_MAX_INDEX) {
        return cfg.band.yellow_percent;
    }
    if (voc_index <= Config::AQ_VOC_ORANGE_MAX_INDEX) {
        return cfg.band.orange_percent;
    }
    return cfg.band.red_percent;
}

uint8_t nox_auto_percent(const DacAutoSensorConfig &cfg, int nox_index) {
    if (nox_index <= Config::AQ_NOX_GREEN_MAX_INDEX) {
        return cfg.band.green_percent;
    }
    if (nox_index <= Config::AQ_NOX_YELLOW_MAX_INDEX) {
        return cfg.band.yellow_percent;
    }
    if (nox_index <= Config::AQ_NOX_ORANGE_MAX_INDEX) {
        return cfg.band.orange_percent;
    }
    return cfg.band.red_percent;
}

void update_reason_candidate(uint8_t percent,
                             const char *sensor,
                             const char *value,
                             uint8_t &best_percent,
                             char *best_sensor,
                             size_t sensor_len,
                             char *best_value,
                             size_t value_len) {
    if (percent <= best_percent) {
        return;
    }
    best_percent = percent;
    snprintf(best_sensor, sensor_len, "%s", sensor);
    snprintf(best_value, value_len, "%s", value);
}

} // namespace

void UiController::update_dac_ui(uint32_t now_ms) {
    refreshConnectivitySnapshot();

    const bool available = fanControl.isAvailable();
    const bool faulted = fanControl.isFaulted();
    set_button_enabled(objects.btn_dac_settings, available);

    const bool manual_mode = (fanControl.mode() == FanControl::Mode::Manual);
    const bool manual_tab_selected = !dac_auto_tab_selected_;
    set_checked_state(objects.btn_dac_manual_on, manual_tab_selected);
    set_checked_state(objects.btn_dac_auto_on, dac_auto_tab_selected_);
    set_visible(objects.dac_manual_container, manual_tab_selected);
    set_visible(objects.dac_auto_container, dac_auto_tab_selected_);

    const uint8_t manual_step = fanControl.manualStep();
    set_checked_state(objects.btn_dak_manual_toggle_1, manual_step == 1);
    set_checked_state(objects.btn_dak_manual_toggle_2, manual_step == 2);
    set_checked_state(objects.btn_dak_manual_toggle_3, manual_step == 3);
    set_checked_state(objects.btn_dak_manual_toggle_4, manual_step == 4);
    set_checked_state(objects.btn_dak_manual_toggle_5, manual_step == 5);
    set_checked_state(objects.btn_dak_manual_toggle_6, manual_step == 6);
    set_checked_state(objects.btn_dak_manual_toggle_7, manual_step == 7);
    set_checked_state(objects.btn_dak_manual_toggle_8, manual_step == 8);
    set_checked_state(objects.btn_dak_manual_toggle_9, manual_step == 9);
    set_checked_state(objects.btn_dak_manual_toggle_10, manual_step == 10);

    const uint32_t timer_s = fanControl.selectedTimerSeconds();
    set_checked_state(objects.btn_dak_manual_timer_toggle_10min, timer_s == Config::DAC_TIMER_PRESETS_S[0]);
    set_checked_state(objects.btn_dak_manual_timer_toggle_30min, timer_s == Config::DAC_TIMER_PRESETS_S[1]);
    set_checked_state(objects.btn_dak_manual_timer_toggle_1hour, timer_s == Config::DAC_TIMER_PRESETS_S[2]);
    set_checked_state(objects.btn_dak_manual_timer_toggle_2hours, timer_s == Config::DAC_TIMER_PRESETS_S[3]);
    set_checked_state(objects.btn_dak_manual_timer_toggle_4hours, timer_s == Config::DAC_TIMER_PRESETS_S[4]);
    set_checked_state(objects.btn_dak_manual_timer_toggle_8hours, timer_s == Config::DAC_TIMER_PRESETS_S[5]);

    const bool running = fanControl.isRunning();
    const bool auto_mode_active = !manual_mode &&
                                  !fanControl.isManualOverrideActive() &&
                                  !fanControl.isAutoResumeBlocked();
    const bool start_active = available && running && fanControl.isManualOverrideActive();
    const bool stop_active = available && !running;
    const bool auto_active = available && auto_mode_active;
    const lv_color_t neutral = color_card_border();
    set_button_accent(objects.btn_dak_manual_start,
                      start_active ? color_green() : neutral,
                      start_active ? LV_OPA_COVER : LV_OPA_TRANSP);
    set_button_accent(objects.btn_dak_manual_stop,
                      stop_active ? color_red() : neutral,
                      stop_active ? LV_OPA_COVER : LV_OPA_TRANSP);
    set_button_accent(objects.btn_dak_manual_auto,
                      auto_active ? color_green() : neutral,
                      auto_active ? LV_OPA_COVER : LV_OPA_TRANSP);
    set_button_accent(objects.btn_dak_auto_on_toggle,
                      auto_active ? color_green() : neutral,
                      auto_active ? LV_OPA_COVER : LV_OPA_TRANSP);
    set_button_accent(objects.btn_dak_manual_stop_1,
                      stop_active ? color_red() : neutral,
                      stop_active ? LV_OPA_COVER : LV_OPA_TRANSP);
    set_checked_state(objects.btn_dak_auto_on_toggle, auto_active);
    set_button_enabled(objects.btn_dak_auto_on_toggle, available);
    set_button_enabled(objects.btn_dak_manual_stop_1, available);

    if (objects.label_dac_status) {
        const char *status_text = "OFFLINE";
        if (faulted) {
            status_text = "FAULT";
        } else if (available) {
            status_text = running ? "RUNNING" : "STOPPED";
        }
        safe_label_set_text(objects.label_dac_status, status_text);
    }
    if (objects.chip_dac_status) {
        if (faulted) {
            set_chip_color(objects.chip_dac_status, color_red());
        } else if (!available) {
            set_chip_color(objects.chip_dac_status, color_inactive());
        } else if (running) {
            set_chip_color(objects.chip_dac_status, color_green());
        } else {
            set_chip_color(objects.chip_dac_status, color_yellow());
        }
    }

    if (objects.label_dac_output_value) {
        if (!fanControl.isOutputKnown()) {
            safe_label_set_text(objects.label_dac_output_value, "UNKNOWN");
        } else {
            const uint16_t output_mv = fanControl.outputMillivolts();
            const uint8_t output_pct = fanControl.outputPercent();
            char output_buf[24];
            snprintf(output_buf, sizeof(output_buf), "%u.%uV (%u%%)",
                     static_cast<unsigned>(output_mv / 1000),
                     static_cast<unsigned>((output_mv % 1000) / 100),
                     static_cast<unsigned>(output_pct));
            safe_label_set_text(objects.label_dac_output_value, output_buf);
        }
    }

    if (objects.label_dac_timer_value) {
        char timer_buf[16] = "--:--";
        const bool auto_mode_display = auto_active;
        if (auto_mode_display) {
            snprintf(timer_buf, sizeof(timer_buf), "%s", "AUTO");
        } else if (running) {
            if (fanControl.selectedTimerSeconds() == 0) {
                snprintf(timer_buf, sizeof(timer_buf), "%s", "\xE2\x88\x9E");
            } else {
                const uint32_t remaining_s = fanControl.remainingSeconds(now_ms);
                if (remaining_s > 0) {
                    format_mmss(remaining_s, timer_buf, sizeof(timer_buf));
                } else {
                    snprintf(timer_buf, sizeof(timer_buf), "%s", "00:00");
                }
            }
        }
        safe_label_set_text(objects.label_dac_timer_value, timer_buf);
    }

    if (objects.label_dac_rl_sensor || objects.label_dac_rl_value) {
        char reason_sensor[16] = "--";
        char reason_value[32] = "--";
        uint8_t best_percent = 0;
        const DacAutoConfig cfg = fanControl.autoConfig();
        const bool gas_warmup = sensorManager.isWarmupActive();

        if (cfg.enabled) {
            if (cfg.co2.enabled && currentData.co2_valid && currentData.co2 > 0) {
                char value_buf[32];
                snprintf(value_buf, sizeof(value_buf), "%d ppm", currentData.co2);
                update_reason_candidate(co2_auto_percent(cfg.co2, currentData.co2),
                                        "CO2:",
                                        value_buf,
                                        best_percent,
                                        reason_sensor, sizeof(reason_sensor),
                                        reason_value, sizeof(reason_value));
            }

            if (cfg.co.enabled &&
                currentData.co_sensor_present &&
                currentData.co_valid &&
                isfinite(currentData.co_ppm) &&
                currentData.co_ppm >= 0.0f) {
                char value_buf[32];
                snprintf(value_buf, sizeof(value_buf), "%.1f ppm", currentData.co_ppm);
                update_reason_candidate(co_auto_percent(cfg.co, currentData.co_ppm),
                                        "CO:",
                                        value_buf,
                                        best_percent,
                                        reason_sensor, sizeof(reason_sensor),
                                        reason_value, sizeof(reason_value));
            }

            if (cfg.pm25.enabled &&
                currentData.pm25_valid &&
                isfinite(currentData.pm25) &&
                currentData.pm25 >= 0.0f) {
                char value_buf[32];
                snprintf(value_buf, sizeof(value_buf), "%.1f ug/m3", currentData.pm25);
                update_reason_candidate(pm25_auto_percent(cfg.pm25, currentData.pm25),
                                        "PM2.5:",
                                        value_buf,
                                        best_percent,
                                        reason_sensor, sizeof(reason_sensor),
                                        reason_value, sizeof(reason_value));
            }

            if (cfg.voc.enabled &&
                !gas_warmup &&
                currentData.voc_valid &&
                currentData.voc_index >= 0) {
                char value_buf[32];
                snprintf(value_buf, sizeof(value_buf), "%d idx", currentData.voc_index);
                update_reason_candidate(voc_auto_percent(cfg.voc, currentData.voc_index),
                                        "VOC:",
                                        value_buf,
                                        best_percent,
                                        reason_sensor, sizeof(reason_sensor),
                                        reason_value, sizeof(reason_value));
            }

            if (cfg.nox.enabled &&
                !gas_warmup &&
                currentData.nox_valid &&
                currentData.nox_index >= 0) {
                char value_buf[32];
                snprintf(value_buf, sizeof(value_buf), "%d idx", currentData.nox_index);
                update_reason_candidate(nox_auto_percent(cfg.nox, currentData.nox_index),
                                        "NOx:",
                                        value_buf,
                                        best_percent,
                                        reason_sensor, sizeof(reason_sensor),
                                        reason_value, sizeof(reason_value));
            }
        }

        if (objects.label_dac_rl_sensor) {
            safe_label_set_text(objects.label_dac_rl_sensor, reason_sensor);
        }
        if (objects.label_dac_rl_value) {
            safe_label_set_text(objects.label_dac_rl_value, reason_value);
        }
    }

    const bool wifi_enabled = connectivity_.wifi_enabled;
    const int wifi_state = connectivity_.wifi_state;
    const bool ap_mode =
        wifi_enabled && wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_AP_CONFIG);
    const bool wifi_connected =
        wifi_enabled && wifi_state == static_cast<int>(AuraNetworkManager::WIFI_STATE_STA_CONNECTED);
    uint32_t network_sig =
        (wifi_enabled ? 0x80000000UL : 0UL) ^
        (static_cast<uint32_t>(wifi_state) << 24);
    network_sig = hash_text(network_sig, connectivity_.ap_ip);
    network_sig = hash_text(network_sig, connectivity_.sta_ip);
    if (network_sig != dac_network_ui_signature_) {
        dac_network_ui_signature_ = network_sig;

        const String local_url = connectivity_.dac_local_url;
        String ip_url = "http://<device-ip>/dac";
        if (wifi_connected) {
            ip_url = connectivity_.sta_ip.isEmpty() ? local_url : connectivity_.dac_sta_url;
        }
        String dac_url;
        if (ap_mode) {
            dac_url = connectivity_.ap_ip.isEmpty()
                          ? "http://192.168.4.1/dac"
                          : String("http://") + connectivity_.ap_ip + "/dac";
        } else if (wifi_connected) {
            dac_url = ip_url;
        }
        if (objects.label_dac_qr_link) {
            if (!dac_url.isEmpty()) {
                safe_label_set_text(objects.label_dac_qr_link, dac_url.c_str());
            } else {
                safe_label_set_text(objects.label_dac_qr_link, "Enable AP or connect to Wi-Fi");
            }
        }
        if (objects.label_dac_qr_text_ip_link) {
            set_visible(objects.label_dac_qr_text_ip_link, wifi_connected);
            String ip_link_text = UiText::LabelDacQrTextIpLink();
            ip_link_text.replace("{{IP_URL}}", ip_url);
            ip_link_text.replace("{{LOCAL_URL}}", local_url);
            safe_label_set_text(objects.label_dac_qr_text_ip_link, ip_link_text.c_str());
        }
        if (objects.qrcode_dac_portal) {
            if (!dac_url.isEmpty()) {
                lv_obj_clear_flag(objects.qrcode_dac_portal, LV_OBJ_FLAG_HIDDEN);
                update_qrcode_if_needed(objects.qrcode_dac_portal,
                                        dac_url.c_str(),
                                        dac_portal_qr_cache_,
                                        sizeof(dac_portal_qr_cache_));
            } else {
                lv_obj_add_flag(objects.qrcode_dac_portal, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

bool persist_dac_auto_state(StorageManager &storage, bool auto_mode, bool auto_armed) {
    if (storage.config().dac_auto_mode == auto_mode &&
        storage.config().dac_auto_armed == auto_armed) {
        return true;
    }
    if (storage.saveDacAutoState(auto_mode, auto_armed)) {
        return true;
    }
    LOGE("UI", "failed to persist DAC auto state");
    return false;
}

WebUiBridge::ApplyResult finalize_dac_bridge_result(bool success,
                                                    uint16_t status_code,
                                                    const char *message) {
    WebUiBridge::ApplyResult result{};
    result.success = success;
    result.status_code = status_code;
    result.error_message = message ? message : "";
    return result;
}

void UiController::on_dac_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!fanControl.isAvailable()) {
        return;
    }
    dac_auto_tab_selected_ = false;
    pending_screen_id = SCREEN_ID_PAGE_DAC_SETTINGS;
}

void UiController::on_dac_settings_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_dac_manual_on_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    dac_auto_tab_selected_ = false;
    update_dac_ui(millis());
}

void UiController::on_dac_auto_on_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    dac_auto_tab_selected_ = true;
    update_dac_ui(millis());
}

void UiController::on_dac_manual_level_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    const uint8_t level = manual_level_from_target(lv_event_get_target(e));
    if (level == 0) {
        return;
    }
    fanControl.setManualStep(level);
    update_dac_ui(millis());
}

void UiController::on_dac_manual_timer_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *target = lv_event_get_target(e);
    const uint32_t timer_s = timer_seconds_from_target(target);
    if (timer_s == 0) {
        return;
    }

    const bool is_checked = lv_obj_has_state(target, LV_STATE_CHECKED);
    if (is_checked) {
        fanControl.setTimerSeconds(timer_s);
    } else if (fanControl.selectedTimerSeconds() == timer_s) {
        fanControl.setTimerSeconds(0);
    }
    update_dac_ui(millis());
}

void UiController::on_dac_manual_start_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    dac_auto_tab_selected_ = false;
    if (!persist_dac_auto_state(storage, false, false)) {
        return;
    }
    if (fanControl.mode() != FanControl::Mode::Manual) {
        fanControl.setMode(FanControl::Mode::Manual);
    }
    fanControl.requestStart();
    update_dac_ui(millis());
}

void UiController::on_dac_manual_stop_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!persist_dac_auto_state(storage, fanControl.mode() == FanControl::Mode::Auto, false)) {
        return;
    }
    fanControl.requestStop();
    update_dac_ui(millis());
}

void UiController::on_dac_manual_auto_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!persist_dac_auto_state(storage, true, true)) {
        return;
    }
    fanControl.requestAutoStart();
    update_dac_ui(millis());
}

void UiController::on_dac_auto_start_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!persist_dac_auto_state(storage, true, true)) {
        return;
    }
    fanControl.requestAutoStart();
    update_dac_ui(millis());
}

void UiController::on_dac_auto_stop_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!persist_dac_auto_state(storage, true, false)) {
        return;
    }
    fanControl.requestStop();
    update_dac_ui(millis());
}

void UiController::on_dac_settings_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_settings_event(e); }
void UiController::on_dac_settings_back_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_settings_back_event(e); }
void UiController::on_dac_manual_on_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_on_event(e); }
void UiController::on_dac_auto_on_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_auto_on_event(e); }
void UiController::on_dac_manual_level_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_level_event(e); }
void UiController::on_dac_manual_timer_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_timer_event(e); }
void UiController::on_dac_manual_start_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_start_event(e); }
void UiController::on_dac_manual_stop_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_stop_event(e); }
void UiController::on_dac_manual_auto_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_manual_auto_event(e); }
void UiController::on_dac_auto_start_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_auto_start_event(e); }
void UiController::on_dac_auto_stop_event_cb(lv_event_t *e) { if (instance_) instance_->on_dac_auto_stop_event(e); }

WebUiBridge::ApplyResult UiController::applyDacActionBridge(
    const WebUiBridge::DacActionUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    if (!controller) {
        return finalize_dac_bridge_result(false, 503, "DAC bridge unavailable");
    }

    switch (update.type) {
        case WebUiBridge::DacActionUpdate::Type::SetMode: {
            const bool auto_mode = update.auto_mode;
            bool auto_armed = false;
            if (auto_mode && controller->fanControl.mode() == FanControl::Mode::Auto) {
                auto_armed = controller->storage.config().dac_auto_armed;
            }
            if (!persist_dac_auto_state(controller->storage, auto_mode, auto_armed)) {
                return finalize_dac_bridge_result(false, 500, "Failed to persist DAC mode");
            }
            controller->fanControl.setMode(auto_mode ? FanControl::Mode::Auto : FanControl::Mode::Manual);
            break;
        }
        case WebUiBridge::DacActionUpdate::Type::SetManualStep:
            controller->fanControl.setManualStep(update.manual_step);
            break;
        case WebUiBridge::DacActionUpdate::Type::SetTimerSeconds:
            controller->fanControl.setTimerSeconds(update.timer_seconds);
            break;
        case WebUiBridge::DacActionUpdate::Type::Start:
            if (!persist_dac_auto_state(controller->storage, false, false)) {
                return finalize_dac_bridge_result(false, 500, "Failed to persist DAC auto state");
            }
            if (controller->fanControl.mode() != FanControl::Mode::Manual) {
                controller->fanControl.setMode(FanControl::Mode::Manual);
            }
            controller->fanControl.requestStart();
            break;
        case WebUiBridge::DacActionUpdate::Type::Stop: {
            const bool auto_mode = (controller->fanControl.mode() == FanControl::Mode::Auto);
            if (!persist_dac_auto_state(controller->storage, auto_mode, false)) {
                return finalize_dac_bridge_result(false, 500, "Failed to persist DAC auto state");
            }
            controller->fanControl.requestStop();
            break;
        }
        case WebUiBridge::DacActionUpdate::Type::StartAuto:
            if (!persist_dac_auto_state(controller->storage, true, true)) {
                return finalize_dac_bridge_result(false, 500, "Failed to persist DAC auto mode");
            }
            controller->fanControl.requestAutoStart();
            break;
        default:
            return finalize_dac_bridge_result(false, 400, "Unsupported action");
    }

    controller->data_dirty = true;
    return finalize_dac_bridge_result(true, 200, nullptr);
}

WebUiBridge::ApplyResult UiController::applyDacAutoBridge(
    const WebUiBridge::DacAutoUpdate &update,
    void *ctx) {
    auto *controller = static_cast<UiController *>(ctx);
    if (!controller) {
        return finalize_dac_bridge_result(false, 503, "DAC auto bridge unavailable");
    }

    String serialized = DacAutoConfigJson::serialize(update.config);
    if (!controller->storage.saveTextAtomic(StorageManager::kDacAutoPath, serialized)) {
        return finalize_dac_bridge_result(false, 500, "Failed to persist auto config");
    }
    if (update.rearm && !persist_dac_auto_state(controller->storage, true, true)) {
        return finalize_dac_bridge_result(false, 500, "Failed to persist DAC auto mode");
    }

    controller->fanControl.setAutoConfig(update.config);
    if (update.rearm) {
        controller->fanControl.requestAutoStart();
    }
    controller->data_dirty = true;
    return finalize_dac_bridge_result(true, 200, nullptr);
}
