#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using i2c_port_t = int;
using gpio_num_t = int;
constexpr int I2C_NUM_MAX=2, I2C_ADDR_BIT_LEN_7=0, I2C_CLK_SRC_DEFAULT=0;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_ARG=2, ESP_ERR_NO_MEM=3;
constexpr int ESP_ERR_TIMEOUT=4, ESP_ERR_INVALID_STATE=5, ESP_ERR_NOT_FOUND=6, ESP_ERR_INVALID_RESPONSE=7;
struct FakeBus;
struct FakeDevice;
using i2c_master_bus_handle_t=FakeBus*;
using i2c_master_dev_handle_t=FakeDevice*;
struct i2c_master_bus_config_t { int i2c_port, sda_io_num, scl_io_num, clk_source, glitch_ignore_cnt; struct {bool enable_internal_pullup;} flags; };
struct i2c_device_config_t {int dev_addr_length;uint16_t device_address;uint32_t scl_speed_hz;};
struct i2c_master_transmit_multi_buffer_info_t {const uint8_t *write_buffer;size_t buffer_size;};
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t*,i2c_master_bus_handle_t*);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t*,i2c_master_dev_handle_t*);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t,uint16_t,int);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const uint8_t*,size_t,int);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t,uint8_t*,size_t,int);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const uint8_t*,size_t,uint8_t*,size_t,int);
esp_err_t i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t,i2c_master_transmit_multi_buffer_info_t*,size_t,int);
