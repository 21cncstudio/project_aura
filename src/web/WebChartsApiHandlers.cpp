// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebChartsApiHandlers.h"

#include <ArduinoJson.h>

#include "core/ChartsRuntimeState.h"
#include "modules/ChartsHistory.h"
#include "web/WebChartsApiUtils.h"
#include "web/WebResponseUtils.h"

namespace {

constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";

class ChartsRuntimeHistoryView final : public WebChartsApiUtils::HistoryView {
public:
    explicit ChartsRuntimeHistoryView(const ChartsRuntimeState &history) : history_(history) {}

    uint16_t count() const override { return history_.count(); }

    uint32_t latestEpoch() const override { return history_.latestEpoch(); }

    bool latestMetric(ChartsHistory::Metric metric, float &out_value) const override {
        return history_.latestMetric(metric, out_value);
    }

    bool metricValueFromOldest(uint16_t offset,
                               ChartsHistory::Metric metric,
                               float &value,
                               bool &valid) const override {
        return history_.metricValueFromOldest(offset, metric, value, valid);
    }

private:
    const ChartsRuntimeState &history_;
};

void send_ota_busy_json(WebRequest &server) {
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(503, "application/json", kApiErrorOtaBusyJson);
}

}  // namespace

namespace WebChartsApiHandlers {

void handleData(WebHandlerContext &context, bool ota_busy) {
    if (!context.server || !context.charts_runtime) {
        return;
    }
    if (ota_busy) {
        send_ota_busy_json(*context.server);
        return;
    }

    WebRequest &server = *context.server;
    const ChartsRuntimeState &history = *context.charts_runtime;
    const ChartsRuntimeHistoryView history_view(history);
    ArduinoJson::JsonDocument doc;
    WebChartsApiUtils::fillJson(
        doc.to<ArduinoJson::JsonObject>(), history_view, server.arg("window"), server.arg("group"));

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

}  // namespace WebChartsApiHandlers
