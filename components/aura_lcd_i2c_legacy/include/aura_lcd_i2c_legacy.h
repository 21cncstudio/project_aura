#pragma once
#include "esp_lcd_panel_io.h"
#ifdef __cplusplus
extern "C" {
#endif
// bus is an already-installed legacy I2C port, not an i2c_master_bus_handle_t.
// The adapter does not own/delete the shared bus. Transactions keep the
// upstream v5.5.5 synchronous command-link behavior and timeout.
esp_err_t aura_lcd_new_panel_io_i2c_legacy(uint32_t bus,
    const esp_lcd_panel_io_i2c_config_t *config, esp_lcd_panel_io_handle_t *out);
#ifdef __cplusplus
}
#endif
