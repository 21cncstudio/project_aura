// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "drivers/Gp8403.h"

#include <driver/i2c.h>

#include "config/AppConfig.h"
#include "core/I2CHelper.h"

bool Gp8403::begin(uint8_t address) {
    address_ = address;
    return probe();
}

bool Gp8403::probe() {
    if (address_ == 0) {
        return false;
    }
    // GP8403 documents write transactions but no register-read protocol.
    // Match DFRobot's current GP8XXX begin(): SLA+W followed by STOP only.
    return I2C::probe(address_) == ESP_OK;
}

bool Gp8403::setOutputRange10V() {
    const uint8_t range = Config::DAC_RANGE_10V;
    return writeRegister(Config::DAC_REG_OUTPUT_RANGE, &range, 1);
}

bool Gp8403::writeChannelRaw12(uint8_t channel, uint16_t raw12) {
    const uint8_t reg = channelRegister(channel);
    if (reg == 0) {
        return false;
    }
    if (raw12 > 0x0FFF) {
        raw12 = 0x0FFF;
    }

    const uint16_t packed = static_cast<uint16_t>(raw12 << 4);
    const uint8_t payload[2] = {
        static_cast<uint8_t>(packed & 0xFF),
        static_cast<uint8_t>((packed >> 8) & 0xFF),
    };
    return writeRegister(reg, payload, sizeof(payload));
}

bool Gp8403::writeChannelMillivolts(uint8_t channel, uint16_t millivolts) {
    if (millivolts < Config::DAC_VOUT_MIN_MV) {
        millivolts = Config::DAC_VOUT_MIN_MV;
    } else if (millivolts > Config::DAC_VOUT_FULL_SCALE_MV) {
        millivolts = Config::DAC_VOUT_FULL_SCALE_MV;
    }

    if (Config::DAC_VOUT_FULL_SCALE_MV == 0) {
        return false;
    }

    const uint32_t numerator = static_cast<uint32_t>(millivolts) * 4095u +
                               (Config::DAC_VOUT_FULL_SCALE_MV / 2u);
    const uint16_t raw12 = static_cast<uint16_t>(numerator / Config::DAC_VOUT_FULL_SCALE_MV);
    return writeChannelRaw12(channel, raw12);
}

bool Gp8403::writeRegister(uint8_t reg, const uint8_t *data, size_t len) {
    if (address_ == 0) {
        return false;
    }
    if (len > 2) {
        return false;
    }
    if (len > 0 && data == nullptr) {
        return false;
    }

    uint8_t tx[3] = {reg, 0, 0};
    for (size_t i = 0; i < len; ++i) {
        tx[1 + i] = data[i];
    }

    const esp_err_t err = i2c_master_write_to_device(
        Config::I2C_PORT,
        address_,
        tx,
        len + 1,
        pdMS_TO_TICKS(Config::I2C_TIMEOUT_MS)
    );
    return err == ESP_OK;
}

uint8_t Gp8403::channelRegister(uint8_t channel) const {
    if (channel == Config::DAC_CHANNEL_VOUT0) {
        return Config::DAC_REG_CHANNEL_0;
    }
    if (channel == Config::DAC_CHANNEL_VOUT1) {
        return Config::DAC_REG_CHANNEL_1;
    }
    return 0;
}

