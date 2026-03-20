// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "web/WebStreamPolicy.h"
#include "web/WebTransport.h"

struct WebStreamRuntime {
    void *context = nullptr;
    uint32_t (*nowMs)(void *context) = nullptr;
    void (*delayMs)(void *context, uint16_t delay_ms) = nullptr;
    void (*kickWatchdog)(void *context) = nullptr;
    void (*serviceConnectedMqtt)(void *context, uint32_t now_ms) = nullptr;
    uint32_t mqttServiceIntervalMs = 0;
};

bool web_stream_client_bytes(WebRequest &server,
                             const uint8_t *data,
                             size_t size,
                             const StreamProfile &profile,
                             const WebStreamRuntime &runtime,
                             size_t &sent,
                             StreamAbortReason &abort_reason,
                             uint32_t &max_write_ms,
                             int &last_socket_errno);
