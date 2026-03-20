// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "web/WebContext.h"
#include "web/WebRuntime.h"
#include "web/WebWifiUtils.h"

void WebHandlersInit(WebHandlerContext *context);
void wifi_build_scan_items(int count);

void wifi_handle_root();
void dashboard_handle_root();
void dashboard_handle_styles();
void dashboard_handle_app();
void wifi_handle_save();
void wifi_handle_not_found();
void diag_handle_root();
void mqtt_handle_root();
void mqtt_handle_save();
void theme_handle_root();
void theme_handle_styles();
void theme_handle_app();
void theme_handle_state();
void theme_handle_apply();
void dac_handle_root();
void dac_handle_styles();
void dac_handle_app();
void dac_handle_state();
void dac_handle_action();
void dac_handle_auto();
void charts_handle_data();
void state_handle_data();
void events_handle_data();
void diag_handle_data();
void settings_handle_update();
void ota_handle_update();
void ota_handle_upload();
