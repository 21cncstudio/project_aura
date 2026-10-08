#pragma once
#include <stddef.h>
#include "esp_err.h"
#include <stdint.h>
typedef void *esp_lcd_panel_io_handle_t;
typedef struct {
    uint32_t dev_addr, scl_speed_hz;
    size_t control_phase_bytes;
    uint8_t dc_bit_offset;
    int lcd_cmd_bits;
    struct { unsigned disable_control_phase: 1; } flags;
} esp_lcd_panel_io_i2c_config_t;
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t, int, void *, size_t);
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int, const void *, size_t);
#ifdef __cplusplus
}
#endif
