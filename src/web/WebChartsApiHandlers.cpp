// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebChartsApiHandlers.h"

#include <ArduinoJson.h>
#include <stdlib.h>
#include <errno.h>

#include "core/ChartsRuntimeState.h"
#include "drivers/DfrOptionalGasSensor.h"
#include "modules/ChartsHistory.h"
#include "web/WebChartsApiUtils.h"
#include "web/WebResponseUtils.h"

namespace {

constexpr const char kApiErrorOtaBusyJson[] =
    "{\"success\":false,\"error\":\"OTA upload in progress\","
    "\"error_code\":\"OTA_BUSY\",\"ota_busy\":true}";

class ChartsRuntimeHistoryView final : public WebChartsApiUtils::HistoryView {
public:
    explicit ChartsRuntimeHistoryView(const ChartsRuntimeState::Snapshot &history)
        : history_(history) {}

    bool entryFromOldest(uint16_t offset, ChartsHistory::Entry &out) const override {
        return history_.entryFromOldest(offset, out);
    }
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
    const ChartsRuntimeState::Snapshot &history_;
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
    const std::unique_ptr<const ChartsRuntimeState::Snapshot> snapshot =
        history.copySnapshot();
    if (!snapshot) {
        WebResponseUtils::sendNoStoreHeaders(server);
        server.send(503,
                    "application/json",
                    "{\"success\":false,\"error\":\"Charts snapshot unavailable\","
                    "\"error_code\":\"CHARTS_SNAPSHOT_UNAVAILABLE\"}");
        return;
    }

    const ChartsRuntimeHistoryView history_view(*snapshot);
    const DfrOptionalGasSensor::OptionalGasType optional_gas_type =
        static_cast<DfrOptionalGasSensor::OptionalGasType>(
            snapshot->optionalGasType());
    ArduinoJson::JsonDocument doc;
    if (server.arg("format") == "history") {
        const String after_arg = server.arg("after");
        uint32_t after = 0;
        if (after_arg.length()) {
            bool digits = after_arg.length() <= 10;
            for (size_t i = 0; i < after_arg.length(); ++i)
                digits = digits && after_arg[i] >= '0' && after_arg[i] <= '9';
            errno = 0;
            const unsigned long long parsed = strtoull(after_arg.c_str(), nullptr, 10);
            if (!digits || errno == ERANGE || parsed > UINT32_MAX) {
                WebResponseUtils::sendNoStoreHeaders(server);
                server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid history cursor\"}");
                return;
            }
            after = static_cast<uint32_t>(parsed);
        }
        WebChartsApiUtils::fillHistoryJson(doc.to<ArduinoJson::JsonObject>(), history_view, after);
    } else WebChartsApiUtils::fillJson(
        doc.to<ArduinoJson::JsonObject>(),
        history_view,
        server.arg("window"),
        server.arg("group"),
        DfrOptionalGasSensor::unitForType(optional_gas_type),
        server.arg("stats") == "1");

    String json;
    serializeJson(doc, json);
    WebResponseUtils::sendNoStoreHeaders(server);
    server.send(200, "application/json", json);
}

}  // namespace WebChartsApiHandlers
