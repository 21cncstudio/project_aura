// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "modules/ChartsHistory.h"

namespace WebChartsApiUtils {

class HistoryView {
public:
    virtual ~HistoryView() = default;

    virtual uint16_t count() const = 0;
    virtual uint32_t latestEpoch() const = 0;
    virtual bool latestMetric(ChartsHistory::Metric metric, float &out_value) const = 0;
    virtual bool metricValueFromOldest(uint16_t offset,
                                       ChartsHistory::Metric metric,
                                       float &value,
                                       bool &valid) const = 0;
};

void fillJson(ArduinoJson::JsonObject root,
              const HistoryView &history,
              const String &window_arg,
              const String &group_arg);

} // namespace WebChartsApiUtils
