"""Require the new I2C master/LCD API and reject every legacy driver entry."""

REQUIRED = {"i2c_new_master_bus", "i2c_master_bus_add_device", "i2c_master_transmit",
            "i2c_master_transmit_receive", "i2c_master_probe", "esp_lcd_new_panel_io_i2c"}
FORBIDDEN = {"i2c_driver_install", "i2c_driver_delete", "i2c_param_config", "i2c_master_cmd_begin",
             "aura_lcd_new_panel_io_i2c_legacy", "esp_lcd_new_panel_io_i2c_v1"}


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
                         f"legacy-driver symbols={sorted(conflicts)}")
