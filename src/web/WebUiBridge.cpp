// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebUiBridge.h"

WebUiBridge::WebUiBridge() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
    settings_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&settings_reply_semaphore_buffer_);
    theme_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&theme_reply_semaphore_buffer_);
    dac_action_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&dac_action_reply_semaphore_buffer_);
    dac_auto_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&dac_auto_reply_semaphore_buffer_);
    wifi_save_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&wifi_save_reply_semaphore_buffer_);
    mqtt_save_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&mqtt_save_reply_semaphore_buffer_);
}

void WebUiBridge::bindSettingsApplier(void *ctx, SettingsApplyFn fn) {
    lock();
    settings_ctx_ = ctx;
    settings_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::bindThemeApplier(void *ctx, ThemeApplyFn fn) {
    lock();
    theme_ctx_ = ctx;
    theme_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::bindDacActionApplier(void *ctx, DacActionApplyFn fn) {
    lock();
    dac_action_ctx_ = ctx;
    dac_action_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::bindDacAutoApplier(void *ctx, DacAutoApplyFn fn) {
    lock();
    dac_auto_ctx_ = ctx;
    dac_auto_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::bindWifiSaveApplier(void *ctx, WifiSaveApplyFn fn) {
    lock();
    wifi_save_ctx_ = ctx;
    wifi_save_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::bindMqttSaveApplier(void *ctx, MqttSaveApplyFn fn) {
    lock();
    mqtt_save_ctx_ = ctx;
    mqtt_save_apply_fn_ = fn;
    unlock();
}

void WebUiBridge::setDispatchMode(DispatchMode mode) {
    lock();
    dispatch_mode_ = mode;
    unlock();
}

void WebUiBridge::publishSnapshot(const Snapshot &snapshot) {
    lock();
    snapshot_ = snapshot;
    snapshot_.available = true;
    unlock();
}

WebUiBridge::Snapshot WebUiBridge::snapshot() const {
    lock();
    Snapshot copy = snapshot_;
    unlock();
    return copy;
}

bool WebUiBridge::isAvailable() const {
    lock();
    const bool available = snapshot_.available;
    unlock();
    return available;
}

WebUiBridge::ApplyResult WebUiBridge::applySettings(const SettingsUpdate &update) {
    SettingsApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = settings_apply_fn_;
    ctx = settings_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_settings_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "UI bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_settings_update_ = update;
        pending_settings_request_ = true;
        request_id = ++pending_settings_request_id_;
        pending_settings_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "UI bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (settings_reply_semaphore_) {
            xSemaphoreTake(settings_reply_semaphore_, 0);
            if (xSemaphoreTake(settings_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "UI bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_settings_result_id_ == request_id;
        ApplyResult result = pending_settings_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "UI bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

WebUiBridge::ApplyResult WebUiBridge::applyTheme(const ThemeUpdate &update) {
    ThemeApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = theme_apply_fn_;
    ctx = theme_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_theme_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "Theme bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_theme_update_ = update;
        pending_theme_request_ = true;
        request_id = ++pending_theme_request_id_;
        pending_theme_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "Theme bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (theme_reply_semaphore_) {
            xSemaphoreTake(theme_reply_semaphore_, 0);
            if (xSemaphoreTake(theme_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "Theme bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_theme_result_id_ == request_id;
        ApplyResult result = pending_theme_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "Theme bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

WebUiBridge::ApplyResult WebUiBridge::applyDacAction(const DacActionUpdate &update) {
    DacActionApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = dac_action_apply_fn_;
    ctx = dac_action_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_dac_action_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "DAC bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_dac_action_update_ = update;
        pending_dac_action_request_ = true;
        request_id = ++pending_dac_action_request_id_;
        pending_dac_action_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "DAC bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (dac_action_reply_semaphore_) {
            xSemaphoreTake(dac_action_reply_semaphore_, 0);
            if (xSemaphoreTake(dac_action_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "DAC bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_dac_action_result_id_ == request_id;
        ApplyResult result = pending_dac_action_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "DAC bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

WebUiBridge::ApplyResult WebUiBridge::applyDacAuto(const DacAutoUpdate &update) {
    DacAutoApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = dac_auto_apply_fn_;
    ctx = dac_auto_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_dac_auto_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "DAC auto bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_dac_auto_update_ = update;
        pending_dac_auto_request_ = true;
        request_id = ++pending_dac_auto_request_id_;
        pending_dac_auto_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "DAC auto bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (dac_auto_reply_semaphore_) {
            xSemaphoreTake(dac_auto_reply_semaphore_, 0);
            if (xSemaphoreTake(dac_auto_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "DAC auto bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_dac_auto_result_id_ == request_id;
        ApplyResult result = pending_dac_auto_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "DAC auto bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

WebUiBridge::ApplyResult WebUiBridge::applyWifiSave(const WifiSaveUpdate &update) {
    WifiSaveApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = wifi_save_apply_fn_;
    ctx = wifi_save_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_wifi_save_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "WiFi bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_wifi_save_update_ = update;
        pending_wifi_save_request_ = true;
        request_id = ++pending_wifi_save_request_id_;
        pending_wifi_save_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "WiFi bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (wifi_save_reply_semaphore_) {
            xSemaphoreTake(wifi_save_reply_semaphore_, 0);
            if (xSemaphoreTake(wifi_save_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "WiFi bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_wifi_save_result_id_ == request_id;
        ApplyResult result = pending_wifi_save_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "WiFi bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

WebUiBridge::ApplyResult WebUiBridge::applyMqttSave(const MqttSaveUpdate &update) {
    MqttSaveApplyFn fn = nullptr;
    void *ctx = nullptr;
    DispatchMode mode = DispatchMode::DirectCallback;
    uint32_t request_id = 0;
    lock();
    mode = dispatch_mode_;
    fn = mqtt_save_apply_fn_;
    ctx = mqtt_save_ctx_;
    if (mode == DispatchMode::DeferredReply) {
        if (pending_mqtt_save_request_) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "MQTT bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        pending_mqtt_save_update_ = update;
        pending_mqtt_save_request_ = true;
        request_id = ++pending_mqtt_save_request_id_;
        pending_mqtt_save_result_id_ = 0;
    }
    unlock();

    if (!fn) {
        ApplyResult unavailable{};
        unavailable.success = false;
        unavailable.status_code = 503;
        unavailable.error_message = "MQTT bridge unavailable";
        unavailable.snapshot = snapshot();
        return unavailable;
    }

    if (mode == DispatchMode::DeferredReply) {
        if (mqtt_save_reply_semaphore_) {
            xSemaphoreTake(mqtt_save_reply_semaphore_, 0);
            if (xSemaphoreTake(mqtt_save_reply_semaphore_, pdMS_TO_TICKS(5000)) != pdTRUE) {
                ApplyResult timeout{};
                timeout.success = false;
                timeout.status_code = 504;
                timeout.error_message = "MQTT bridge timeout";
                timeout.snapshot = snapshot();
                return timeout;
            }
        }
        lock();
        const bool result_matches = pending_mqtt_save_result_id_ == request_id;
        ApplyResult result = pending_mqtt_save_result_;
        unlock();
        if (!result_matches) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = "MQTT bridge response mismatch";
            invalid.snapshot = snapshot();
            return invalid;
        }
        return result;
    }

    return fn(update, ctx);
}

bool WebUiBridge::consumePendingSettingsRequest(SettingsUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_settings_request_) {
        unlock();
        return false;
    }
    update = pending_settings_update_;
    request_id = pending_settings_request_id_;
    pending_settings_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingSettingsRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_settings_result_ = result;
    pending_settings_result_id_ = request_id;
    unlock();
    if (settings_reply_semaphore_) {
        xSemaphoreGive(settings_reply_semaphore_);
    }
}

bool WebUiBridge::consumePendingThemeRequest(ThemeUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_theme_request_) {
        unlock();
        return false;
    }
    update = pending_theme_update_;
    request_id = pending_theme_request_id_;
    pending_theme_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingThemeRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_theme_result_ = result;
    pending_theme_result_id_ = request_id;
    unlock();
    if (theme_reply_semaphore_) {
        xSemaphoreGive(theme_reply_semaphore_);
    }
}

bool WebUiBridge::consumePendingDacActionRequest(DacActionUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_dac_action_request_) {
        unlock();
        return false;
    }
    update = pending_dac_action_update_;
    request_id = pending_dac_action_request_id_;
    pending_dac_action_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingDacActionRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_dac_action_result_ = result;
    pending_dac_action_result_id_ = request_id;
    unlock();
    if (dac_action_reply_semaphore_) {
        xSemaphoreGive(dac_action_reply_semaphore_);
    }
}

bool WebUiBridge::consumePendingDacAutoRequest(DacAutoUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_dac_auto_request_) {
        unlock();
        return false;
    }
    update = pending_dac_auto_update_;
    request_id = pending_dac_auto_request_id_;
    pending_dac_auto_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingDacAutoRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_dac_auto_result_ = result;
    pending_dac_auto_result_id_ = request_id;
    unlock();
    if (dac_auto_reply_semaphore_) {
        xSemaphoreGive(dac_auto_reply_semaphore_);
    }
}

bool WebUiBridge::consumePendingWifiSaveRequest(WifiSaveUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_wifi_save_request_) {
        unlock();
        return false;
    }
    update = pending_wifi_save_update_;
    request_id = pending_wifi_save_request_id_;
    pending_wifi_save_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingWifiSaveRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_wifi_save_result_ = result;
    pending_wifi_save_result_id_ = request_id;
    unlock();
    if (wifi_save_reply_semaphore_) {
        xSemaphoreGive(wifi_save_reply_semaphore_);
    }
}

bool WebUiBridge::consumePendingMqttSaveRequest(MqttSaveUpdate &update, uint32_t &request_id) {
    lock();
    if (!pending_mqtt_save_request_) {
        unlock();
        return false;
    }
    update = pending_mqtt_save_update_;
    request_id = pending_mqtt_save_request_id_;
    pending_mqtt_save_request_ = false;
    unlock();
    return true;
}

void WebUiBridge::completePendingMqttSaveRequest(uint32_t request_id, const ApplyResult &result) {
    lock();
    pending_mqtt_save_result_ = result;
    pending_mqtt_save_result_id_ = request_id;
    unlock();
    if (mqtt_save_reply_semaphore_) {
        xSemaphoreGive(mqtt_save_reply_semaphore_);
    }
}

void WebUiBridge::requestFirmwareUpdateScreen(bool active) {
    lock();
    firmware_update_screen_active_ = active;
    firmware_update_screen_pending_ = true;
    unlock();
}

bool WebUiBridge::consumePendingFirmwareUpdateScreen(bool &active) {
    lock();
    if (!firmware_update_screen_pending_) {
        unlock();
        return false;
    }
    active = firmware_update_screen_active_;
    firmware_update_screen_pending_ = false;
    unlock();
    return true;
}

void WebUiBridge::setMqttScreenOpen(bool open) {
    lock();
    snapshot_.mqtt_screen_open = open;
    unlock();
}

void WebUiBridge::setThemeScreenOpen(bool open, bool custom_open) {
    lock();
    snapshot_.theme_screen_open = open;
    snapshot_.theme_custom_screen_open = open && custom_open;
    unlock();
}

void WebUiBridge::lock() const {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void WebUiBridge::unlock() const {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}
