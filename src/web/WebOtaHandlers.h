// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "web/OtaDeferredRestart.h"
#include "web/WebContext.h"
#include "web/WebOtaState.h"

namespace WebOtaHandlers {

struct Runtime {
    WebHandlerContext &context;
    WebOtaState &ota_state;
    OtaDeferredRestart::Controller &restart_controller;
    uint32_t deferred_restart_delay_ms = 0;
    uint32_t (*upload_timeout_ms)(size_t image_size_bytes) = nullptr;
    void (*disable_wifi_power_save_for_upload)() = nullptr;
    void (*restore_wifi_power_save)() = nullptr;
    void (*arm_preflight_ui)() = nullptr;
    void (*cancel_preflight_ui)() = nullptr;
    void (*set_ui_screen)(bool active) = nullptr;
    void (*set_error)(const String &error) = nullptr;
};

void handlePrepare(Runtime &runtime, bool ota_busy);
void handleUpload(Runtime &runtime, bool ota_busy);
void handleUpdate(Runtime &runtime, bool ota_busy);

}  // namespace WebOtaHandlers
