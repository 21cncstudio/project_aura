// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/InitConfig.h"

#include <math.h>

#include "config/AppConfig.h"

void InitConfig::normalizeOffsets(float &temp_offset, float &hum_offset) {
    temp_offset = lroundf(temp_offset * 10.0f) / 10.0f;
    hum_offset = lroundf(hum_offset);

    if (hum_offset < Config::HUM_OFFSET_MIN) {
        hum_offset = Config::HUM_OFFSET_MIN;
    } else if (hum_offset > Config::HUM_OFFSET_MAX) {
        hum_offset = Config::HUM_OFFSET_MAX;
    }
}
