// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Sen0466.h"

#include "config/AppConfig.h"

Sen0466::Sen0466()
    : DfrMultiGasSensor({
          "SEN0466",
          "SEN0466 CO",
          Config::SEN0466_ADDR,
          Config::SEN0466_GAS_TYPE_CO,
          Config::SEN0466_CO_MIN_PPM,
          Config::SEN0466_CO_MAX_PPM,
          nullptr,
          0,
          true,
          Config::SEN0466_RUNTIME_TRANSPORT_RETRY_DELAY_MS,
          true,
      }) {}
