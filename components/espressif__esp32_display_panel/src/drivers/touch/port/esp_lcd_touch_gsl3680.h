/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch: GSL3680
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LCD_TOUCH_GSL3680_VER_MAJOR    (1)
#define ESP_LCD_TOUCH_GSL3680_VER_MINOR    (0)
#define ESP_LCD_TOUCH_GSL3680_VER_PATCH    (0)

/**
 * @brief Create a new GSL3680 touch driver
 *
 * @note The I2C communication should be initialized before using this function.
 *
 * @param io LCD/Touch panel IO handle
 * @param config Touch configuration
 * @param out_touch Touch instance handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t esp_lcd_touch_new_i2c_gsl3680(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch);

/**
 * @brief I2C address of the GSL3680 controller
 */
#define ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS          (0x40)

/**
 * @brief GSL3680 Configuration Type
 */
typedef struct {
    uint8_t dev_addr;  /*!< I2C device address */
} esp_lcd_touch_io_gsl3680_config_t;

/**
 * @brief Touch IO configuration structure
 */
#define ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG()           \
    {                                       \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS, \
        .control_phase_bytes = 1,           \
        .dc_bit_offset = 0,                 \
        .lcd_cmd_bits = 8,                  \
        .flags =                            \
        {                                   \
            .disable_control_phase = 1,     \
        }                                   \
    }

/**
 * @brief Touch IO configuration structure with input address
 *
 * @param[in] addr I2C address of the touch panel
 */
#define ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG_WITH_ADDR(addr) \
    {                                       \
        .dev_addr = addr,                   \
        .control_phase_bytes = 1,           \
        .dc_bit_offset = 0,                 \
        .lcd_cmd_bits = 8,                  \
        .flags =                            \
        {                                   \
            .disable_control_phase = 1,     \
        }                                   \
    }

#ifdef __cplusplus
}
#endif
