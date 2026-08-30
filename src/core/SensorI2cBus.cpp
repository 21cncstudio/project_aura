// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/SensorI2cBus.h"

#include <esp_err.h>

#include "config/AppConfig.h"
#include "core/Logger.h"

namespace SensorI2cBus {

Result begin() {
    Result result{};
    result.separate = Config::SENSOR_I2C_SEPARATE;

    if (!result.separate) {
        LOGI("I2C",
             "sensor bus shares panel host: port=%d SDA=%u SCL=%u freq=%lu Hz",
             static_cast<int>(Config::SENSOR_I2C_PORT),
             static_cast<unsigned>(Config::SENSOR_I2C_SDA_PIN),
             static_cast<unsigned>(Config::SENSOR_I2C_SCL_PIN),
             static_cast<unsigned long>(Config::SENSOR_I2C_FREQ_HZ));
        return result;
    }

    LOGI("I2C",
         "sensor bus separate: port=%d SDA=%u SCL=%u freq=%lu Hz pullups=%s",
         static_cast<int>(Config::SENSOR_I2C_PORT),
         static_cast<unsigned>(Config::SENSOR_I2C_SDA_PIN),
         static_cast<unsigned>(Config::SENSOR_I2C_SCL_PIN),
         static_cast<unsigned long>(Config::SENSOR_I2C_FREQ_HZ),
         Config::SENSOR_I2C_INTERNAL_PULLUPS ? "internal" : "external");

    i2c_config_t config{};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num =
        static_cast<gpio_num_t>(Config::SENSOR_I2C_SDA_PIN);
    config.scl_io_num =
        static_cast<gpio_num_t>(Config::SENSOR_I2C_SCL_PIN);
    config.sda_pullup_en = Config::SENSOR_I2C_INTERNAL_PULLUPS
                               ? GPIO_PULLUP_ENABLE
                               : GPIO_PULLUP_DISABLE;
    config.scl_pullup_en = Config::SENSOR_I2C_INTERNAL_PULLUPS
                               ? GPIO_PULLUP_ENABLE
                               : GPIO_PULLUP_DISABLE;
    config.master.clk_speed = Config::SENSOR_I2C_FREQ_HZ;
    config.clk_flags = 0;

    result.configuration_attempted = true;
    result.configuration_error =
        i2c_param_config(Config::SENSOR_I2C_PORT, &config);
    if (result.configuration_error != ESP_OK) {
        LOGE("I2C",
             "sensor bus configuration failed: port=%d error=%s (%d)",
             static_cast<int>(Config::SENSOR_I2C_PORT),
             esp_err_to_name(result.configuration_error),
             static_cast<int>(result.configuration_error));
        return result;
    }

    result.installation_attempted = true;
    result.installation_error = i2c_driver_install(
        Config::SENSOR_I2C_PORT, config.mode, 0, 0, 0);
    if (result.installation_error != ESP_OK) {
        LOGE("I2C",
             "sensor bus driver install failed: port=%d error=%s (%d)",
             static_cast<int>(Config::SENSOR_I2C_PORT),
             esp_err_to_name(result.installation_error),
             static_cast<int>(result.installation_error));
        return result;
    }

    LOGI("I2C", "sensor bus ready: port=%d",
         static_cast<int>(Config::SENSOR_I2C_PORT));
    return result;
}

} // namespace SensorI2cBus
