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

namespace WebSettingsApiHandlers {

void handleUpdate(WebHandlerContext &context,
                  bool ota_busy,
                  size_t display_name_max_len,
                  uint32_t deferred_restart_delay_ms,
                  OtaDeferredRestart::Controller &restart_controller);

}  // namespace WebSettingsApiHandlers
