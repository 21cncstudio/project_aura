// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#if AURA_NATIVE_IDF
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#else
#include "driver/i2c.h"
#endif

typedef struct {
    int sda_io_num;
    int scl_io_num;
    bool sda_pullup_en;
    bool scl_pullup_en;
    uint32_t clk_speed;
} aura_i2c_host_config_t;

#ifdef __cplusplus
extern "C" {
#endif

#if AURA_NATIVE_IDF
// The host owner starts/stops a port. Devices borrow it; transfers never
// install, delete or recover a bus. Timeouts are milliseconds, not RTOS ticks.
esp_err_t aura_i2c_start(i2c_port_t port, const aura_i2c_host_config_t *config);
esp_err_t aura_i2c_stop(i2c_port_t port);
i2c_master_bus_handle_t aura_i2c_bus(i2c_port_t port);
uint32_t aura_i2c_frequency(i2c_port_t port);
esp_err_t aura_i2c_probe(i2c_port_t port, uint8_t address, uint32_t timeout_ms);
esp_err_t aura_i2c_write(i2c_port_t port, uint8_t address, const uint8_t *data, size_t length, uint32_t timeout_ms);
esp_err_t aura_i2c_read(i2c_port_t port, uint8_t address, uint8_t *data, size_t length, uint32_t timeout_ms);
esp_err_t aura_i2c_write_read(i2c_port_t port, uint8_t address, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length, uint32_t timeout_ms);
esp_err_t aura_i2c_write_pair(i2c_port_t port, uint8_t address, const uint8_t *first, size_t first_length, const uint8_t *second, size_t second_length, uint32_t timeout_ms);
#else
// Only the existing host driver tests use this backend in the migration branch.
static inline esp_err_t aura_i2c_write(i2c_port_t port, uint8_t address, const uint8_t *data, size_t length, uint32_t timeout_ms) {
    return i2c_master_write_to_device(port, address, data, length, pdMS_TO_TICKS(timeout_ms));
}
static inline esp_err_t aura_i2c_read(i2c_port_t port, uint8_t address, uint8_t *data, size_t length, uint32_t timeout_ms) {
    return i2c_master_read_from_device(port, address, data, length, pdMS_TO_TICKS(timeout_ms));
}
static inline esp_err_t aura_i2c_write_read(i2c_port_t port, uint8_t address, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length, uint32_t timeout_ms) {
    return i2c_master_write_read_device(port, address, tx, tx_length, rx, rx_length, pdMS_TO_TICKS(timeout_ms));
}
static inline esp_err_t aura_i2c_write_pair(i2c_port_t port, uint8_t address, const uint8_t *first, size_t first_length, const uint8_t *second, size_t second_length, uint32_t timeout_ms) {
    if ((first_length && !first) || (second_length && !second)) return ESP_ERR_INVALID_ARG;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;
    esp_err_t err = i2c_master_start(cmd);
    if (err == ESP_OK) err = i2c_master_write_byte(cmd, (address << 1U) | I2C_MASTER_WRITE, true);
    if (err == ESP_OK && first_length) err = i2c_master_write(cmd, first, first_length, true);
    if (err == ESP_OK && second_length) err = i2c_master_write(cmd, second, second_length, true);
    if (err == ESP_OK) err = i2c_master_stop(cmd);
    if (err == ESP_OK) err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(timeout_ms));
    i2c_cmd_link_delete(cmd);
    return err;
}
static inline esp_err_t aura_i2c_probe(i2c_port_t port, uint8_t address, uint32_t timeout_ms) {
    return aura_i2c_write_pair(port, address, NULL, 0, NULL, 0, timeout_ms);
}
#endif

#ifdef __cplusplus
}
#endif
