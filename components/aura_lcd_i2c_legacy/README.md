# Temporary LCD IO adapter for the IDF 6.1 migration

`esp_lcd_panel_io_i2c_v1.c` is an unmodified copy from Espressif's ESP-IDF
v5.5.5 tag:

https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_lcd/i2c/esp_lcd_panel_io_i2c_v1.c

SHA-256 (original bytes):
`fb009474a40c26d8fd7a47bc089d0b46dec31dd8459bc415adbaaee6ed66476a`.
The Espressif copyright/SPDX header and Apache-2.0 license are retained.
CMake renames only the exported factory to `aura_lcd_new_panel_io_i2c_legacy`.

The adapter uses IDF 6.1's public LCD IO interface and its existing legacy I2C
driver. It creates a panel IO object over an already-installed numeric I2C port;
it does not install, remove, reset or recover the shared bus. The original
synchronous transactions, repeated START handling, error returns and
`portMAX_DELAY` behavior are preserved. The new IDF 6.1
`transaction_timeout_ms` field is not used by this legacy adapter.

This lets GT911 share the same driver with the CH422G expander and Aura sensor
code during the first native-IDF build stage. The final ELF check requires the
legacy driver, its startup conflict check and this adapter, and rejects a
linked new I2C driver. Do not disable the SDK conflict check.

Remove this component together with the Display Panel factory adaptation when
panel, sensors, IO expander, probes and recovery are migrated to the new master
bus API as one coordinated change. This component is not a claim that legacy
I2C has been migrated to the new API or that hardware has been validated.
