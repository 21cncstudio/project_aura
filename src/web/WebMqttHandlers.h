// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

#include "web/WebContext.h"
#include "web/WebDeferredActionsState.h"
#include "web/WebResponseUtils.h"

namespace WebMqttHandlers {

void handleRoot(WebHandlerContext &context,
                const WebResponseUtils::StreamContext &stream_context);

void handleSave(WebHandlerContext &context,
                bool ota_busy,
                uint32_t deferred_action_delay_ms,
                WebDeferredActionsState &deferred_actions,
                const WebResponseUtils::StreamContext &stream_context);

}  // namespace WebMqttHandlers
