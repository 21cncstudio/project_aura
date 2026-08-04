#pragma once

#include <cstddef>
#include <cstdint>

typedef int i2c_port_t;
typedef int esp_err_t;
typedef uint32_t TickType_t;
typedef struct MockI2cCmd *i2c_cmd_handle_t;

#ifndef I2C_NUM_0
#define I2C_NUM_0 0
#endif

#ifndef ESP_OK
#define ESP_OK 0
#endif

#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif

#ifndef ESP_ERR_INVALID_ARG
#define ESP_ERR_INVALID_ARG -2
#endif

#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM -3
#endif

#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT -4
#endif

#ifndef I2C_MASTER_WRITE
#define I2C_MASTER_WRITE 0
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif

i2c_cmd_handle_t i2c_cmd_link_create();
void i2c_cmd_link_delete(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_start(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool ack_en);
esp_err_t i2c_master_write(i2c_cmd_handle_t cmd,
                           const uint8_t *data,
                           size_t data_len,
                           bool ack_en);
esp_err_t i2c_master_cmd_begin(i2c_port_t port,
                               i2c_cmd_handle_t cmd,
                               TickType_t ticks_to_wait);

esp_err_t i2c_master_write_read_device(i2c_port_t port,
                                       uint8_t addr,
                                       const uint8_t *write_buffer,
                                       size_t write_size,
                                       uint8_t *read_buffer,
                                       size_t read_size,
                                       TickType_t ticks_to_wait);

esp_err_t i2c_master_write_to_device(i2c_port_t port,
                                     uint8_t addr,
                                     const uint8_t *write_buffer,
                                     size_t write_size,
                                     TickType_t ticks_to_wait);

esp_err_t i2c_master_read_from_device(i2c_port_t port,
                                      uint8_t addr,
                                      uint8_t *read_buffer,
                                      size_t read_size,
                                      TickType_t ticks_to_wait);
