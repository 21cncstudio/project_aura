// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiBootFlow.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#include "core/BootState.h"
#include "core/AppVersion.h"
#include "core/Logger.h"
#include "modules/FanControl.h"
#include "modules/StorageManager.h"
#include "ui/UiController.h"
#include "ui/UiText.h"
#include "ui/ui.h"

namespace {

const char *reset_reason_to_string(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

bool is_crash_reset(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

bool is_brownout_reset(esp_reset_reason_t reason) {
    return reason == ESP_RST_BROWNOUT;
}

void append_error_line(char *dst, size_t dst_size, size_t &offset, const char *line) {
    if (!dst || !line || dst_size == 0 || offset >= dst_size - 1) {
        return;
    }
    if (offset != 0) {
        int n = snprintf(dst + offset, dst_size - offset, "\n");
        if (n > 0) {
            offset += static_cast<size_t>(n);
            if (offset >= dst_size) {
                offset = dst_size - 1;
            }
        }
    }
    int n = snprintf(dst + offset, dst_size - offset, "%s", line);
    if (n > 0) {
        offset += static_cast<size_t>(n);
        if (offset >= dst_size) {
            offset = dst_size - 1;
        }
    }
}

} // namespace

void UiBootFlow::clearBootObjectRefs(UiController &owner) {
    (void)owner;
    objects.page_boot_logo = nullptr;
    objects.page_boot_diag = nullptr;
    objects.label_boot_ver = nullptr;
    objects.background_boot_diag = nullptr;
    objects.btn_diag_continue = nullptr;
    objects.label_btn_diag_continue = nullptr;
    objects.lbl_diag_title = nullptr;
    objects.lbl_diag_system_title = nullptr;
    objects.lbl_diag_app_label = nullptr;
    objects.lbl_diag_mac_label = nullptr;
    objects.lbl_diag_reason_label = nullptr;
    objects.lbl_diag_heap_label = nullptr;
    objects.lbl_diag_storage_label = nullptr;
    objects.lbl_diag_app_ver = nullptr;
    objects.lbl_diag_mac = nullptr;
    objects.lbl_diag_reason = nullptr;
    objects.lbl_diag_heap = nullptr;
    objects.lbl_diag_storage = nullptr;
    objects.lbl_diag_sensors_title = nullptr;
    objects.lbl_diag_i2c_label = nullptr;
    objects.lbl_diag_touch_label = nullptr;
    objects.lbl_diag_sen_label = nullptr;
    objects.lbl_diag_dps_label = nullptr;
    objects.lbl_diag_sfa_label = nullptr;
    objects.lbl_diag_i2c = nullptr;
    objects.lbl_diag_touch = nullptr;
    objects.lbl_diag_sen = nullptr;
    objects.lbl_diag_dps = nullptr;
    objects.lbl_diag_sfa = nullptr;
    objects.lbl_diag_rtc_label = nullptr;
    objects.lbl_diag_rtc = nullptr;
    objects.lbl_diag_co_label = nullptr;
    objects.lbl_diag_co = nullptr;
    objects.lbl_diag_dac_label = nullptr;
    objects.lbl_diag_dac = nullptr;
    objects.lbl_diag_error = nullptr;
    objects.btn_diag_errors = nullptr;
    objects.label_btn_diag_errors = nullptr;
    objects.container_diag_errors = nullptr;
    objects.label_diag_errors_text = nullptr;
}

void UiBootFlow::releaseBootScreens(UiController &owner) {
    if (owner.boot_ui_released) {
        return;
    }

    lv_obj_t *boot_logo = objects.page_boot_logo;
    lv_obj_t *boot_diag = objects.page_boot_diag;
    if (boot_logo && lv_obj_is_valid(boot_logo)) {
        lv_obj_del_async(boot_logo);
    }
    if (boot_diag && lv_obj_is_valid(boot_diag)) {
        lv_obj_del_async(boot_diag);
    }

    clearBootObjectRefs(owner);
    owner.screen_events_bound_[SCREEN_ID_PAGE_BOOT_LOGO] = false;
    owner.screen_events_bound_[SCREEN_ID_PAGE_BOOT_DIAG] = false;
    owner.boot_logo_active = false;
    owner.boot_diag_active = false;
    owner.boot_diag_has_error = false;
    owner.boot_release_at_ms = 0;
    owner.boot_ui_released = true;

    LOGI("UI", "boot screens released");
}

bool UiBootFlow::bootDiagHasErrors(UiController &owner, uint32_t now_ms) {
    bool has_error = false;
    if (boot_ui_auto_recovery_reboot) {
        has_error = true;
    }
    if (!owner.storage.isMounted()) {
        has_error = true;
    }
    if (!boot_i2c_recovered) {
        has_error = true;
    }
    if (!boot_touch_detected) {
        has_error = true;
    }
    if (is_crash_reset(boot_reset_reason) || is_brownout_reset(boot_reset_reason)) {
        has_error = true;
    }
    if (!owner.sensorManager.isOk()) {
        uint32_t retry_at = owner.sensorManager.retryAtMs();
        if (retry_at == 0 || now_ms >= retry_at) {
            has_error = true;
        }
    }
    if (!owner.sensorManager.isDpsOk()) {
        has_error = true;
    }
    if (owner.sensorManager.hasSfaFault()) {
        has_error = true;
    }
    if (owner.timeManager.isRtcPresent()) {
        if (owner.timeManager.isRtcLostPower() || !owner.timeManager.isRtcValid()) {
            has_error = true;
        }
    }
    return has_error;
}

void UiBootFlow::updateBootDiag(UiController &owner, uint32_t now_ms) {
    owner.update_boot_diag_texts();
    char buf[64];
    char error_lines[512] = {0};
    size_t error_len = 0;

    if (objects.lbl_diag_app_ver) {
        snprintf(buf, sizeof(buf), "v%s", AppVersion::fullVersion());
        owner.safe_label_set_text(objects.lbl_diag_app_ver, buf);
    }
    if (objects.lbl_diag_mac) {
        String mac = WiFi.macAddress();
        owner.safe_label_set_text(objects.lbl_diag_mac, mac.c_str());
    }
    if (objects.lbl_diag_reason) {
        const char *reason = reset_reason_to_string(boot_reset_reason);
        if (safe_boot_stage > 0) {
            snprintf(buf, sizeof(buf), "%s / boot=%lu safe=%lu",
                     reason,
                     static_cast<unsigned long>(boot_count),
                     static_cast<unsigned long>(safe_boot_stage));
        } else {
            snprintf(buf, sizeof(buf), "%s / boot=%lu",
                     reason,
                     static_cast<unsigned long>(boot_count));
        }
        owner.safe_label_set_text(objects.lbl_diag_reason, buf);
        if (boot_ui_auto_recovery_reboot) {
            append_error_line(error_lines, sizeof(error_lines), error_len,
                              "UI auto-recovery reboot (display task stalled)");
        }
        if (is_crash_reset(boot_reset_reason)) {
            char reason_line[96];
            snprintf(reason_line, sizeof(reason_line), "Crash reset: %s", reason);
            append_error_line(error_lines, sizeof(error_lines), error_len, reason_line);
        } else if (is_brownout_reset(boot_reset_reason)) {
            append_error_line(error_lines, sizeof(error_lines), error_len, "Brownout reset detected");
        }
    }
    if (objects.lbl_diag_heap) {
        size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t min_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        size_t max_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        snprintf(buf, sizeof(buf), "free %uk / min %uk / max %uk",
                 static_cast<unsigned>(free_bytes / 1024),
                 static_cast<unsigned>(min_bytes / 1024),
                 static_cast<unsigned>(max_bytes / 1024));
        owner.safe_label_set_text(objects.lbl_diag_heap, buf);
    }
    if (objects.lbl_diag_storage) {
        const char *status = UiText::StatusErr();
        if (owner.storage.isMounted()) {
            status = owner.storage.isConfigLoaded() ? UiText::BootDiagStorageOkConfig()
                                                    : UiText::BootDiagStorageOkDefaults();
        }
        owner.safe_label_set_text(objects.lbl_diag_storage, status);
    }
    if (!owner.storage.isMounted()) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "Storage not mounted");
    }
    if (objects.lbl_diag_i2c) {
        owner.safe_label_set_text(objects.lbl_diag_i2c,
                                  boot_i2c_recovered ? UiText::BootDiagRecovered() : UiText::BootDiagFail());
    }
    if (!boot_i2c_recovered) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "I2C bus recovery failed");
    }
    if (objects.lbl_diag_touch) {
        owner.safe_label_set_text(objects.lbl_diag_touch,
                                  boot_touch_detected ? UiText::BootDiagDetected() : UiText::BootDiagFail());
    }
    if (!boot_touch_detected) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "Touch probe failed at boot");
    }
    if (objects.lbl_diag_sen) {
        const char *status = UiText::StatusErr();
        if (owner.sensorManager.isOk()) {
            status = UiText::StatusOk();
        } else {
            uint32_t retry_at = owner.sensorManager.retryAtMs();
            if (retry_at != 0 && now_ms < retry_at) {
                status = UiText::BootDiagStarting();
            }
        }
        owner.safe_label_set_text(objects.lbl_diag_sen, status);
    }
    if (!owner.sensorManager.isOk()) {
        uint32_t retry_at = owner.sensorManager.retryAtMs();
        if (retry_at != 0 && now_ms < retry_at) {
            append_error_line(error_lines, sizeof(error_lines), error_len, "SEN66 starting...");
        } else {
            append_error_line(error_lines, sizeof(error_lines), error_len, "SEN66 not found/read failed");
        }
    }
    if (objects.lbl_diag_dps_label) {
        owner.safe_label_set_text(objects.lbl_diag_dps_label, owner.sensorManager.pressureSensorLabel());
    }
    if (objects.lbl_diag_dps) {
        owner.safe_label_set_text(objects.lbl_diag_dps,
                                  owner.sensorManager.isDpsOk() ? UiText::StatusOk() : UiText::StatusErr());
    }
    if (!owner.sensorManager.isDpsOk()) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "Pressure sensor read failed");
    }
    if (objects.lbl_diag_sfa) {
        const char *status = UiText::BootDiagNotFound();
        switch (owner.sensorManager.sfaStatus()) {
            case SensorManager::SfaStatus::Ok:
                status = UiText::StatusOk();
                break;
            case SensorManager::SfaStatus::Fault:
                status = UiText::StatusErr();
                break;
            case SensorManager::SfaStatus::Absent:
                break;
        }
        owner.safe_label_set_text(objects.lbl_diag_sfa, status);
    }
    if (owner.sensorManager.hasSfaFault()) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "SFA30 communication failed");
    }
    if (objects.lbl_diag_co) {
        const char *status = UiText::BootDiagNotFound();
        if (owner.sensorManager.isCoPresent()) {
            if (owner.sensorManager.isCoWarmupActive()) {
                status = UiText::BootDiagStarting();
            } else if (owner.sensorManager.isCoValid()) {
                status = UiText::StatusOk();
            } else {
                status = UiText::StatusErr();
            }
        }
        owner.safe_label_set_text(objects.lbl_diag_co, status);
    }
    if (owner.sensorManager.isCoPresent() &&
        !owner.sensorManager.isCoWarmupActive() &&
        !owner.sensorManager.isCoValid()) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "SEN0466 detected but read failed");
    }
    if (objects.lbl_diag_dac) {
        const char *status = UiText::BootDiagNotFound();
        if (owner.fanControl.isAvailable()) {
            status = UiText::StatusOk();
        } else if (owner.fanControl.isFaulted()) {
            status = UiText::StatusErr();
        }
        owner.safe_label_set_text(objects.lbl_diag_dac, status);
    }
    if (owner.fanControl.isFaulted()) {
        append_error_line(error_lines, sizeof(error_lines), error_len, "DFR0971 detected but probe/write failed");
    }
    if (objects.lbl_diag_rtc) {
        const char *status = UiText::BootDiagNotFound();
        if (owner.timeManager.isRtcPresent()) {
            if (owner.timeManager.isRtcLostPower()) {
                status = UiText::BootDiagLost();
            } else if (owner.timeManager.isRtcValid()) {
                status = UiText::StatusOk();
            } else {
                status = UiText::StatusErr();
            }
        }
        owner.safe_label_set_text(objects.lbl_diag_rtc, status);
    }
    if (owner.timeManager.isRtcPresent()) {
        if (owner.timeManager.isRtcLostPower()) {
            append_error_line(error_lines, sizeof(error_lines), error_len, "RTC lost power");
        } else if (!owner.timeManager.isRtcValid()) {
            append_error_line(error_lines, sizeof(error_lines), error_len, "RTC invalid time");
        }
    }

    bool has_errors = bootDiagHasErrors(owner, now_ms);
    // Block auto-advance while SEN66 is still in its startup grace period.
    // This is not treated as a real error — no error button is shown — but the
    // screen stays visible until the final sensor state is known.
    bool sen66_pending = !owner.sensorManager.isOk() &&
                         owner.sensorManager.retryAtMs() != 0 &&
                         now_ms < owner.sensorManager.retryAtMs();
    owner.boot_diag_has_error = has_errors || sen66_pending;

    // Show error UI only for confirmed errors, not for pending startup state.
    owner.set_visible(objects.lbl_diag_error, has_errors);
    owner.set_visible(objects.btn_diag_continue, has_errors);
    owner.set_visible(objects.btn_diag_errors, has_errors);
    if (objects.container_diag_errors) {
        if (!has_errors) {
            lv_obj_add_flag(objects.container_diag_errors, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (objects.label_diag_errors_text) {
        if (has_errors) {
            if (error_len == 0) {
                owner.safe_label_set_text(objects.label_diag_errors_text, "No details");
            } else {
                owner.safe_label_set_text(objects.label_diag_errors_text, error_lines);
            }
        } else {
            owner.safe_label_set_text(objects.label_diag_errors_text, "");
        }
    }
}
