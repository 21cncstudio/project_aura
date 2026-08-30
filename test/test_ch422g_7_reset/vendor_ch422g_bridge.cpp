#include "driver/i2c.h"

#ifndef I2C_NUM_MAX
#define I2C_NUM_MAX 2
#endif

inline void vTaskDelay(TickType_t) {}

esp_err_t aura_ch422g_test_write(i2c_port_t,
                                 uint8_t,
                                 const uint8_t *,
                                 size_t,
                                 TickType_t);
esp_err_t aura_ch422g_test_read(i2c_port_t,
                                uint8_t,
                                uint8_t *,
                                size_t,
                                TickType_t);

#define i2c_master_write_to_device aura_ch422g_test_write
#define i2c_master_read_from_device aura_ch422g_test_read
#include "../../third_party/ESP32_IO_Expander_7/src/port/esp_io_expander_ch422g.c"
#undef i2c_master_write_to_device
#undef i2c_master_read_from_device
