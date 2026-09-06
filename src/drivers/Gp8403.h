// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <Arduino.h>

class Gp8403 {
public:
    bool begin(uint8_t address);
    bool probe();
    bool setOutputRange10V();
    bool writeChannelRaw12(uint8_t channel, uint16_t raw12);
    bool writeChannelMillivolts(uint8_t channel, uint16_t millivolts);

private:
    bool writeRegister(uint8_t reg, const uint8_t *data, size_t len);
    uint8_t channelRegister(uint8_t channel) const;

    uint8_t address_ = 0;
};

