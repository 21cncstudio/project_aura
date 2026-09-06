// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <driver/i2c.h>

namespace SensorI2cBus {

struct Result {
    bool separate = false;
    bool configuration_attempted = false;
    bool installation_attempted = false;
    esp_err_t configuration_error = ESP_OK;
    esp_err_t installation_error = ESP_OK;

    bool ready() const {
        return (!configuration_attempted || configuration_error == ESP_OK) &&
               (!installation_attempted || installation_error == ESP_OK);
    }
};

// The panel library already owns I2C0. On the 4.3-inch profile this is a
// no-op; on the 7-inch profile it installs the externally pulled-up I2C1 host.
Result begin();

} // namespace SensorI2cBus
