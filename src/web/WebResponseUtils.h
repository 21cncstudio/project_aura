// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "web/WebStreamPolicy.h"
#include "web/WebStreamState.h"
#include "web/WebStreamWriter.h"

namespace WebResponseUtils {

enum class AssetCacheMode : uint8_t {
    NoStore = 0,
    Immutable,
};

struct StreamContext {
    void *context = nullptr;
    WebStreamState *stream_state = nullptr;
    const WebStreamRuntime *stream_runtime = nullptr;
    uint32_t slow_write_warn_ms = 0;
    uint32_t (*nowMs)(void *context) = nullptr;
    uint32_t (*wifiStaConnectedElapsedMs)(void *context) = nullptr;
};

void sendNoStoreHeaders(WebRequest &server);
void sendNoStoreText(WebRequest &server, int status_code, const char *message);
void noteShellPriority(const StreamContext &context);
bool shouldPauseMqttForTransfer(const StreamContext &context);
uint32_t transferPauseRemainingMs(const StreamContext &context);

bool sendHtmlStream(WebRequest &server, const String &html, const StreamContext &context);
bool sendHtmlStreamResilient(WebRequest &server, const String &html, const StreamContext &context);
bool sendProgmemAsset(WebRequest &server,
                      const char *content_type,
                      const uint8_t *content,
                      size_t content_size,
                      bool gzip_encoded,
                      AssetCacheMode cache_mode,
                      const StreamContext &context,
                      const StreamProfile *profile_override = nullptr);
bool sendHtmlStreamProgmem(WebRequest &server,
                           const uint8_t *content,
                           size_t content_size,
                           bool gzip_encoded,
                           const StreamContext &context);

}  // namespace WebResponseUtils
