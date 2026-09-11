"""Reject native-IDF images that mix Aura's legacy I2C with the new driver."""

REQUIRED = {"i2c_driver_install", "check_i2c_driver_conflict", "aura_lcd_new_panel_io_i2c_legacy"}
FORBIDDEN = {"i2c_acquire_bus_handle", "i2c_new_master_bus", "i2c_master_bus_add_device",
             "esp_lcd_new_panel_io_i2c"}


def validate_i2c_linkage(symbol_table):
    defined = set()
    for line in symbol_table.splitlines():
        fields = line.split(None, 5)
        if len(fields) == 6 and fields[2] == "F" and fields[3] != "*UND*":
            defined.add(fields[5])
    missing = REQUIRED - defined
    conflicts = FORBIDDEN & defined
    if missing or conflicts:
        raise ValueError(f"Invalid native I2C linkage: missing={sorted(missing)}, "
                         f"new-driver symbols={sorted(conflicts)}")
