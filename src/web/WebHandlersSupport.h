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
#include "web/WebDeferredActionsState.h"
#include "web/WebOtaHandlers.h"
#include "web/WebResponseUtils.h"
#include "web/WebStreamState.h"

namespace WebHandlersSupport {

constexpr uint32_t deferredActionDelayMs() {
    return 200;
}

constexpr size_t webDisplayNameMaxLen() {
    return 32;
}

const char *otaBusyJson();

void init(WebHandlerContext *context);
WebHandlerContext *context();

bool isOtaBusy();
bool consumeRestartRequest();
void requestRestart(uint32_t delay_ms);
void beginRestartShutdown();
bool shouldPauseMqttForTransfer();
void noteMqttConnectDeferred();
void noteMqttPublishDeferred();
void pollDeferred();

WebTransferSnapshot streamSnapshot(uint32_t now_ms);
WebResponseUtils::StreamContext responseContext();
WebDeferredActionsState &deferredActions();
OtaDeferredRestart::Controller &restartController();
WebOtaHandlers::Runtime otaRuntime(WebHandlerContext &context);

}  // namespace WebHandlersSupport
