// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebDailyHistoryHandlers.h"

#include <ArduinoJson.h>
#include <stdio.h>

#include "core/Logger.h"
#include "modules/DailyExtremaHistory.h"
#include "modules/SdCardManager.h"
#include "web/WebResponseUtils.h"

namespace {

constexpr size_t kCsvStreamBufferSize = 768;

void send_json(WebRequest &server, int status_code, ArduinoJson::JsonDocument &doc) {
    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(status_code, "application/json", json);
}

void send_error(WebRequest &server, int status_code, const char *message) {
    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();
    root["success"] = false;
    root["error"] = message ? message : "Unknown error";
    send_json(server, status_code, doc);
}

} // namespace

namespace WebDailyHistoryHandlers {

void handleStatus(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server) {
        return;
    }
    if (ota_busy) {
        WebResponseUtils::sendNoStoreHeaders(*context.server);
        context.server->send(503, "application/json", ota_busy_json);
        return;
    }

    ArduinoJson::JsonDocument doc;
    ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();
    root["success"] = true;

    ArduinoJson::JsonObject sd = root["sd"].to<ArduinoJson::JsonObject>();
    if (context.sd_card) {
        const SdCardManager::Status status = context.sd_card->status();
        sd["mounted"] = status.mounted;
        sd["attempted"] = status.attempted;
        sd["mount_point"] = status.mount_point;
        sd["card_size_bytes"] = status.card_size_bytes;
        sd["last_error"] = status.last_error;
    } else {
        sd["mounted"] = false;
        sd["attempted"] = false;
        sd["last_error"] = "SD manager unavailable";
    }

    ArduinoJson::JsonObject daily = root["daily"].to<ArduinoJson::JsonObject>();
    daily["csv_path"] = DailyExtremaHistory::kDailyCsvPath;
    if (context.daily_extrema) {
        daily["current_day"] = context.daily_extrema->currentDayKey();
        daily["current_sample_count"] = context.daily_extrema->currentSampleCount();
        daily["last_write_ok"] = context.daily_extrema->lastWriteOk();
    } else {
        daily["current_day"] = 0;
        daily["current_sample_count"] = 0;
        daily["last_write_ok"] = false;
    }

    send_json(*context.server, 200, doc);
}

void handleCsv(WebHandlerContext &context, bool ota_busy, const char *ota_busy_json) {
    if (!context.server) {
        return;
    }
    WebRequest &server = *context.server;
    if (ota_busy) {
        WebResponseUtils::sendNoStoreHeaders(server);
        server.send(503, "application/json", ota_busy_json);
        return;
    }
    if (!context.sd_card || !context.sd_card->isReady()) {
        send_error(server, 503, "SD card is not mounted");
        return;
    }

    size_t file_size = 0;
    if (!context.sd_card->fileSize(DailyExtremaHistory::kDailyCsvPath, file_size)) {
        send_error(server, 404, "Daily history CSV is not available yet");
        return;
    }

    FILE *file = context.sd_card->openRead(DailyExtremaHistory::kDailyCsvPath);
    if (!file) {
        send_error(server, 500, "Failed to open daily history CSV");
        return;
    }

    WebResponseUtils::sendNoStoreHeaders(server);
    server.sendHeader("Content-Disposition", "attachment; filename=\"aura-daily-history.csv\"");
    if (!server.beginStreamResponse(200, "text/csv; charset=utf-8", file_size)) {
        fclose(file);
        return;
    }

    uint8_t buffer[kCsvStreamBufferSize];
    size_t sent = 0;
    int last_error = 0;
    bool ok = true;
    while (sent < file_size) {
        const size_t remaining = file_size - sent;
        const size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const size_t read = fread(buffer, 1, to_read, file);
        if (read == 0) {
            ok = false;
            break;
        }
        size_t chunk_sent = 0;
        while (chunk_sent < read) {
            if (!server.waitUntilWritable(250, last_error)) {
                ok = false;
                break;
            }
            const int32_t written =
                server.writeStreamChunk(buffer + chunk_sent, read - chunk_sent, last_error);
            if (written <= 0) {
                ok = false;
                break;
            }
            chunk_sent += static_cast<size_t>(written);
            sent += static_cast<size_t>(written);
        }
        if (!ok || !server.clientConnected()) {
            ok = false;
            break;
        }
    }
    fclose(file);

    if (ok) {
        server.endStreamResponse();
    } else {
        Logger::log(Logger::Warn,
                    "Web",
                    "daily history CSV stream interrupted sent=%u/%u err=%d",
                    static_cast<unsigned>(sent),
                    static_cast<unsigned>(file_size),
                    last_error);
    }
}

} // namespace WebDailyHistoryHandlers
