// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

void WebHandlersPollDeferred();
bool WebHandlersIsOtaBusy();
bool WebHandlersIsOtaUploadActive();
bool WebHandlersConsumeRestartRequest();
void WebHandlersRequestRestart(uint32_t delay_ms = 0);
bool WebHandlersTryBeginRestartShutdown();
bool WebHandlersAllowOtaPhysicalConfirm(uint32_t expected_confirm_id);
bool WebHandlersDenyOtaPhysicalConfirm(uint32_t expected_confirm_id);
bool WebHandlersShouldPauseMqttConnect();
bool WebHandlersShouldPauseMqttPublish();
void WebHandlersNoteMqttConnectDeferred();
void WebHandlersNoteMqttPublishDeferred();
