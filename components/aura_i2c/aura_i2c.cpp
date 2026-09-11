// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AuraI2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <limits.h>

namespace {
struct Bus {
    StaticSemaphore_t storage{};
    SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&storage);
    i2c_master_bus_handle_t handle = nullptr;
    i2c_master_dev_handle_t devices[128]{};
    uint32_t frequency = 0;
};

Bus *bus_state(i2c_port_t port) {
    static Bus buses[I2C_NUM_MAX];
    const int index = static_cast<int>(port);
    return index >= 0 && index < I2C_NUM_MAX ? &buses[index] : nullptr;
}

struct Lock {
    Bus *bus;
    bool held;
    explicit Lock(Bus *state, uint32_t timeout_ms) : bus(state),
        held(state && xSemaphoreTake(state->mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {}
    ~Lock() { if (held) xSemaphoreGive(bus->mutex); }
};

// Existing sensor policies distinguish an address NACK from a bus timeout.
esp_err_t transfer_result(esp_err_t result) {
    return result == ESP_ERR_INVALID_RESPONSE || result == ESP_ERR_NOT_FOUND ? ESP_FAIL : result;
}

esp_err_t device(Bus &bus, uint8_t address, i2c_master_dev_handle_t *out) {
    if (!bus.handle) return ESP_ERR_INVALID_STATE;
    if (!bus.devices[address]) {
        i2c_device_config_t config{};
        config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        config.device_address = address;
        config.scl_speed_hz = bus.frequency;
        const esp_err_t err = i2c_master_bus_add_device(bus.handle, &config, &bus.devices[address]);
        if (err != ESP_OK) return err;
    }
    *out = bus.devices[address];
    return ESP_OK;
}

template <typename Transfer>
esp_err_t transact(i2c_port_t port, uint8_t address, uint32_t timeout_ms, Transfer transfer) {
    if (address >= 128 || timeout_ms == 0 || timeout_ms > INT_MAX) return ESP_ERR_INVALID_ARG;
    Bus *bus = bus_state(port);
    if (!bus) return ESP_ERR_INVALID_ARG;
    Lock lock(bus, timeout_ms);
    if (!lock.held) return ESP_ERR_TIMEOUT;
    i2c_master_dev_handle_t handle = nullptr;
    const esp_err_t err = device(*bus, address, &handle);
    return err == ESP_OK ? transfer_result(transfer(handle)) : err;
}
}

esp_err_t aura_i2c_start(i2c_port_t port, const aura_i2c_host_config_t *config) {
    Bus *bus = bus_state(port);
    if (!bus || !config || !config->clk_speed || config->clk_speed > 400000 ||
        config->sda_pullup_en != config->scl_pullup_en) return ESP_ERR_INVALID_ARG;
    Lock lock(bus, 1000);
    if (!lock.held) return ESP_ERR_TIMEOUT;
    if (bus->handle) return ESP_ERR_INVALID_STATE;
    i2c_master_bus_config_t native{};
    native.i2c_port = port;
    native.sda_io_num = static_cast<gpio_num_t>(config->sda_io_num);
    native.scl_io_num = static_cast<gpio_num_t>(config->scl_io_num);
    native.clk_source = I2C_CLK_SRC_DEFAULT;
    native.glitch_ignore_cnt = 7;
    native.flags.enable_internal_pullup = config->sda_pullup_en;
    const esp_err_t err = i2c_new_master_bus(&native, &bus->handle);
    if (err == ESP_OK) bus->frequency = config->clk_speed;
    return err;
}

esp_err_t aura_i2c_stop(i2c_port_t port) {
    Bus *bus = bus_state(port);
    if (!bus) return ESP_ERR_INVALID_ARG;
    Lock lock(bus, 1000);
    if (!lock.held) return ESP_ERR_TIMEOUT;
    if (!bus->handle) return ESP_ERR_INVALID_STATE;
    // Runtime shutdown already drains the sensor/touch gates before the owner
    // releases its bus. Invalidate every cached handle before another start.
    for (auto &entry : bus->devices) {
        if (!entry) continue;
        const esp_err_t err = i2c_master_bus_rm_device(entry);
        if (err != ESP_OK) return err;
        entry = nullptr;
    }
    const esp_err_t err = i2c_del_master_bus(bus->handle);
    if (err == ESP_OK) { bus->handle = nullptr; bus->frequency = 0; }
    return err;
}

i2c_master_bus_handle_t aura_i2c_bus(i2c_port_t port) {
    Bus *bus = bus_state(port);
    return bus ? bus->handle : nullptr;
}
uint32_t aura_i2c_frequency(i2c_port_t port) {
    Bus *bus = bus_state(port);
    return bus ? bus->frequency : 0;
}

esp_err_t aura_i2c_probe(i2c_port_t port, uint8_t address, uint32_t timeout_ms) {
    Bus *bus = bus_state(port);
    if (!bus || address >= 128 || !timeout_ms || timeout_ms > INT_MAX) return ESP_ERR_INVALID_ARG;
    Lock lock(bus, timeout_ms);
    if (!lock.held) return ESP_ERR_TIMEOUT;
    if (!bus->handle) return ESP_ERR_INVALID_STATE;
    return transfer_result(i2c_master_probe(bus->handle, address, timeout_ms));
}
esp_err_t aura_i2c_write(i2c_port_t port, uint8_t address, const uint8_t *data, size_t length, uint32_t timeout_ms) {
    if (!data || !length) return ESP_ERR_INVALID_ARG;
    return transact(port, address, timeout_ms, [&](auto dev) { return i2c_master_transmit(dev, data, length, timeout_ms); });
}
esp_err_t aura_i2c_read(i2c_port_t port, uint8_t address, uint8_t *data, size_t length, uint32_t timeout_ms) {
    if (!data || !length) return ESP_ERR_INVALID_ARG;
    return transact(port, address, timeout_ms, [&](auto dev) { return i2c_master_receive(dev, data, length, timeout_ms); });
}
esp_err_t aura_i2c_write_read(i2c_port_t port, uint8_t address, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length, uint32_t timeout_ms) {
    if (!tx || !tx_length || !rx || !rx_length) return ESP_ERR_INVALID_ARG;
    return transact(port, address, timeout_ms, [&](auto dev) { return i2c_master_transmit_receive(dev, tx, tx_length, rx, rx_length, timeout_ms); });
}
esp_err_t aura_i2c_write_pair(i2c_port_t port, uint8_t address, const uint8_t *first, size_t first_length, const uint8_t *second, size_t second_length, uint32_t timeout_ms) {
    if (!first || !first_length || (second_length && !second)) return ESP_ERR_INVALID_ARG;
    if (!second_length) return aura_i2c_write(port, address, first, first_length, timeout_ms);
    i2c_master_transmit_multi_buffer_info_t buffers[] = {{first, first_length}, {second, second_length}};
    return transact(port, address, timeout_ms, [&](auto dev) { return i2c_master_multi_buffer_transmit(dev, buffers, 2, timeout_ms); });
}
