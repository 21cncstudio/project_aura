// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "WebUiBridge.h"

WebUiBridge::WebUiBridge() {
    mutex_ = xSemaphoreCreateMutexStatic(&mutex_buffer_);
    settings_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&settings_reply_semaphore_buffer_);
    theme_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&theme_reply_semaphore_buffer_);
    dac_action_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&dac_action_reply_semaphore_buffer_);
    dac_auto_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&dac_auto_reply_semaphore_buffer_);
    wifi_save_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&wifi_save_reply_semaphore_buffer_);
    mqtt_save_reply_semaphore_ = xSemaphoreCreateBinaryStatic(&mqtt_save_reply_semaphore_buffer_);
}

WebUiBridge::ApplyResult WebUiBridge::waitForDeferredReply(
    DeferredRequestState &state,
    uint32_t request_id,
    uint32_t &active_request_id,
    ApplyResult &stored_result,
    uint32_t &stored_result_id,
    SemaphoreHandle_t reply_semaphore,
    const char *timeout_message,
    const char *mismatch_message) {
    bool wait_without_timeout = false;

    for (;;) {
        const TickType_t wait_ticks = wait_without_timeout
                                          ? portMAX_DELAY
                                          : pdMS_TO_TICKS(5000);
        const bool signaled = xSemaphoreTake(reply_semaphore, wait_ticks) == pdTRUE;

        lock();
        if (active_request_id != request_id) {
            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = mismatch_message;
            invalid.snapshot = snapshot_;
            unlock();
            return invalid;
        }

        if (state == DeferredRequestState::Completed) {
            // completeDeferredReply publishes the token while holding mutex_, so a
            // timeout racing a completion can safely remove the already-published
            // token before making this channel idle again.
            if (!signaled) {
                xSemaphoreTake(reply_semaphore, 0);
            }
            if (stored_result_id == request_id) {
                ApplyResult result = stored_result;
                state = DeferredRequestState::Idle;
                stored_result_id = 0;
                unlock();
                return result;
            }

            ApplyResult invalid{};
            invalid.success = false;
            invalid.status_code = 500;
            invalid.error_message = mismatch_message;
            invalid.snapshot = snapshot_;
            unlock();
            return invalid;
        }

        if (signaled) {
            // A defensive stale-token drain must never release this request or be
            // mistaken for another generation's reply.
            wait_without_timeout = state == DeferredRequestState::InFlight;
            unlock();
            continue;
        }

        if (state == DeferredRequestState::Queued) {
            // Work that the UI has not accepted can be cancelled without a late
            // side effect. consumePending* will no longer see this generation.
            state = DeferredRequestState::Idle;
            stored_result_id = 0;
            ApplyResult timeout{};
            timeout.success = false;
            timeout.status_code = 504;
            timeout.error_message = timeout_message;
            timeout.snapshot = snapshot_;
            unlock();
            return timeout;
        }

        if (state == DeferredRequestState::InFlight) {
            // Once the UI has accepted the request it may already be applying
            // persistent changes. Returning 504 would be a false cancellation,
            // so retain ownership of this generation until its matching reply.
            wait_without_timeout = true;
            unlock();
            continue;
        }

        ApplyResult invalid{};
        invalid.success = false;
        invalid.status_code = 500;
        invalid.error_message = mismatch_message;
        invalid.snapshot = snapshot_;
        unlock();
        return invalid;
    }
}

void WebUiBridge::completeDeferredReply(DeferredRequestState &state,
                                        uint32_t request_id,
                                        const uint32_t &active_request_id,
                                        ApplyResult &stored_result,
                                        uint32_t &stored_result_id,
                                        SemaphoreHandle_t reply_semaphore,
                                        const ApplyResult &result) {
    lock();
    if (state == DeferredRequestState::InFlight &&
        active_request_id == request_id) {
        stored_result = result;
        stored_result_id = request_id;
        state = DeferredRequestState::Completed;
        // Publish the state and its binary token atomically with respect to the
        // timeout path. Stale/wrong generations are ignored and never signal.
        xSemaphoreGive(reply_semaphore);
    }
    unlock();
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
        if (!fn || !settings_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "UI bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_settings_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "UI bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (settings_reply_semaphore_) {
            xSemaphoreTake(settings_reply_semaphore_, 0);
        }
        pending_settings_update_ = update;
        pending_settings_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_settings_state_,
                                    request_id,
                                    pending_settings_request_id_,
                                    pending_settings_result_,
                                    pending_settings_result_id_,
                                    settings_reply_semaphore_,
                                    "UI bridge timeout",
                                    "UI bridge response mismatch");
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
        if (!fn || !theme_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "Theme bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_theme_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "Theme bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (theme_reply_semaphore_) {
            xSemaphoreTake(theme_reply_semaphore_, 0);
        }
        pending_theme_update_ = update;
        pending_theme_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_theme_state_,
                                    request_id,
                                    pending_theme_request_id_,
                                    pending_theme_result_,
                                    pending_theme_result_id_,
                                    theme_reply_semaphore_,
                                    "Theme bridge timeout",
                                    "Theme bridge response mismatch");
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
        if (!fn || !dac_action_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "DAC bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_dac_action_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "DAC bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (dac_action_reply_semaphore_) {
            xSemaphoreTake(dac_action_reply_semaphore_, 0);
        }
        pending_dac_action_update_ = update;
        pending_dac_action_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_dac_action_state_,
                                    request_id,
                                    pending_dac_action_request_id_,
                                    pending_dac_action_result_,
                                    pending_dac_action_result_id_,
                                    dac_action_reply_semaphore_,
                                    "DAC bridge timeout",
                                    "DAC bridge response mismatch");
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
        if (!fn || !dac_auto_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "DAC auto bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_dac_auto_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "DAC auto bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (dac_auto_reply_semaphore_) {
            xSemaphoreTake(dac_auto_reply_semaphore_, 0);
        }
        pending_dac_auto_update_ = update;
        pending_dac_auto_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_dac_auto_state_,
                                    request_id,
                                    pending_dac_auto_request_id_,
                                    pending_dac_auto_result_,
                                    pending_dac_auto_result_id_,
                                    dac_auto_reply_semaphore_,
                                    "DAC auto bridge timeout",
                                    "DAC auto bridge response mismatch");
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
        if (!fn || !wifi_save_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "WiFi bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_wifi_save_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "WiFi bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (wifi_save_reply_semaphore_) {
            xSemaphoreTake(wifi_save_reply_semaphore_, 0);
        }
        pending_wifi_save_update_ = update;
        pending_wifi_save_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_wifi_save_state_,
                                    request_id,
                                    pending_wifi_save_request_id_,
                                    pending_wifi_save_result_,
                                    pending_wifi_save_result_id_,
                                    wifi_save_reply_semaphore_,
                                    "WiFi bridge timeout",
                                    "WiFi bridge response mismatch");
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
        if (!fn || !mqtt_save_reply_semaphore_) {
            ApplyResult unavailable{};
            unavailable.success = false;
            unavailable.status_code = 503;
            unavailable.error_message = "MQTT bridge unavailable";
            unavailable.snapshot = snapshot_;
            unlock();
            return unavailable;
        }
        if (pending_mqtt_save_state_ != DeferredRequestState::Idle) {
            ApplyResult busy{};
            busy.success = false;
            busy.status_code = 503;
            busy.error_message = "MQTT bridge busy";
            busy.snapshot = snapshot_;
            unlock();
            return busy;
        }
        if (mqtt_save_reply_semaphore_) {
            xSemaphoreTake(mqtt_save_reply_semaphore_, 0);
        }
        pending_mqtt_save_update_ = update;
        pending_mqtt_save_state_ = DeferredRequestState::Queued;
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
        return waitForDeferredReply(pending_mqtt_save_state_,
                                    request_id,
                                    pending_mqtt_save_request_id_,
                                    pending_mqtt_save_result_,
                                    pending_mqtt_save_result_id_,
                                    mqtt_save_reply_semaphore_,
                                    "MQTT bridge timeout",
                                    "MQTT bridge response mismatch");
    }

    return fn(update, ctx);
}

bool WebUiBridge::consumePendingSettingsRequest(SettingsUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_settings_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_settings_update_;
    request_id = pending_settings_request_id_;
    pending_settings_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingSettingsRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_settings_state_,
                          request_id,
                          pending_settings_request_id_,
                          pending_settings_result_,
                          pending_settings_result_id_,
                          settings_reply_semaphore_,
                          result);
}

bool WebUiBridge::consumePendingThemeRequest(ThemeUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_theme_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_theme_update_;
    request_id = pending_theme_request_id_;
    pending_theme_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingThemeRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_theme_state_,
                          request_id,
                          pending_theme_request_id_,
                          pending_theme_result_,
                          pending_theme_result_id_,
                          theme_reply_semaphore_,
                          result);
}

bool WebUiBridge::consumePendingDacActionRequest(DacActionUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_dac_action_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_dac_action_update_;
    request_id = pending_dac_action_request_id_;
    pending_dac_action_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingDacActionRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_dac_action_state_,
                          request_id,
                          pending_dac_action_request_id_,
                          pending_dac_action_result_,
                          pending_dac_action_result_id_,
                          dac_action_reply_semaphore_,
                          result);
}

bool WebUiBridge::consumePendingDacAutoRequest(DacAutoUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_dac_auto_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_dac_auto_update_;
    request_id = pending_dac_auto_request_id_;
    pending_dac_auto_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingDacAutoRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_dac_auto_state_,
                          request_id,
                          pending_dac_auto_request_id_,
                          pending_dac_auto_result_,
                          pending_dac_auto_result_id_,
                          dac_auto_reply_semaphore_,
                          result);
}

bool WebUiBridge::consumePendingWifiSaveRequest(WifiSaveUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_wifi_save_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_wifi_save_update_;
    request_id = pending_wifi_save_request_id_;
    pending_wifi_save_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingWifiSaveRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_wifi_save_state_,
                          request_id,
                          pending_wifi_save_request_id_,
                          pending_wifi_save_result_,
                          pending_wifi_save_result_id_,
                          wifi_save_reply_semaphore_,
                          result);
}

bool WebUiBridge::consumePendingMqttSaveRequest(MqttSaveUpdate &update, uint32_t &request_id) {
    lock();
    if (pending_mqtt_save_state_ != DeferredRequestState::Queued) {
        unlock();
        return false;
    }
    update = pending_mqtt_save_update_;
    request_id = pending_mqtt_save_request_id_;
    pending_mqtt_save_state_ = DeferredRequestState::InFlight;
    unlock();
    return true;
}

void WebUiBridge::completePendingMqttSaveRequest(uint32_t request_id, const ApplyResult &result) {
    completeDeferredReply(pending_mqtt_save_state_,
                          request_id,
                          pending_mqtt_save_request_id_,
                          pending_mqtt_save_result_,
                          pending_mqtt_save_result_id_,
                          mqtt_save_reply_semaphore_,
                          result);
}

void WebUiBridge::requestFirmwareUpdateScreen(FirmwareUpdateScreenMode mode,
                                              uint32_t confirm_id) {
    lock();
    if (!firmware_update_screen_lineage_.accept(confirm_id)) {
        unlock();
        return;
    }
    firmware_update_screen_request_.mode = mode;
    firmware_update_screen_request_.confirm_id = confirm_id;
    firmware_update_screen_pending_ = true;
    unlock();
}

bool WebUiBridge::consumePendingFirmwareUpdateScreen(FirmwareUpdateScreenRequest &request) {
    lock();
    if (!firmware_update_screen_pending_) {
        unlock();
        return false;
    }
    request = firmware_update_screen_request_;
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
