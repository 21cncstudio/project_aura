"""Native I2C adaptation of the pinned Display Panel configuration interface."""
import re


def replace_body(text, signature, body):
    start = text.index("{", text.index(signature))
    end = start + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[:start] + "{\n" + body + "\n}" + text[end:]


def flat_config(text):
    # Both vendors accept the same configuration when sharing HostI2C.
    text = re.sub(r"\bi2c_config_t\b", "aura_i2c_host_config_t", text)
    text = text.replace("            .mode = I2C_MODE_MASTER,\n", "")
    text = text.replace("            .master = {\n                .clk_speed = static_cast<uint32_t>(config.clk_speed),\n            },",
                        "            .clk_speed = static_cast<uint32_t>(config.clk_speed),")
    text = text.replace("            .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,\n", "")
    text = text.replace(".master.clk_speed", ".clk_speed")
    text = text.replace('            "\\n\\t\\t-> [mode]: %d"\n', "")
    text = text.replace('            "\\n\\t\\t-> [clk_flags]: %d"\n', "")
    text = text.replace('            "\\t\\t-> [mode]: %d\\n"\n', "")
    text = text.replace('            "\\t\\t-> [clk_flags]: %d"\n', "")
    text = text.replace("            , static_cast<int>(config.mode)\n", "")
    text = text.replace("            , static_cast<int>(config.clk_flags)\n", "")
    return text


def adapt_panel_i2c(name, text):
    text = text.replace('#include "driver/i2c.h"', '#include "AuraI2c.h"')
    text = flat_config(text)
    if name.endswith("esp_panel_host_i2c.cpp"):
        text = text.replace("i2c_driver_delete(static_cast<i2c_port_t>(id))", "aura_i2c_stop(static_cast<i2c_port_t>(id))")
        old = '''        ESP_UTILS_CHECK_ERROR_RETURN(
            i2c_param_config(static_cast<i2c_port_t>(id), &config), false, "I2C param config failed"
        );
        ESP_UTILS_CHECK_ERROR_RETURN(
            i2c_driver_install(static_cast<i2c_port_t>(id), config.mode, 0, 0, 0), false, "I2C driver install failed"
        );'''
        assert text.count(old) == 1
        text = text.replace(old, '''        ESP_UTILS_CHECK_ERROR_RETURN(
            aura_i2c_start(static_cast<i2c_port_t>(id), &config), false, "I2C master bus creation failed"
        );''')
        text = replace_body(text, "bool HostI2C::calibrateConfig", '''    const auto &old = this->config;
    return config.sda_io_num == old.sda_io_num && config.scl_io_num == old.scl_io_num &&
           config.sda_pullup_en == old.sda_pullup_en && config.scl_pullup_en == old.scl_pullup_en &&
           config.clk_speed == old.clk_speed;''')
    elif name.endswith("esp_panel_bus_i2c.cpp"):
        start = text.index("#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)")
        end = text.index("#endif // ESP_IDF_VERSION", start) + len("#endif // ESP_IDF_VERSION")
        text = text[:start] + '''    auto native_port = static_cast<i2c_port_t>(host_id);
    auto bus = aura_i2c_bus(native_port);
    ESP_UTILS_CHECK_NULL_RETURN(bus, false, "I2C master bus is not initialized");
    auto panel_config = getControlPanelFullConfig();
    panel_config.scl_speed_hz = aura_i2c_frequency(native_port);
    // A stalled touch device must release the GUI task within a finite interval.
    panel_config.transaction_timeout_ms = 50;
    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_lcd_new_panel_io_i2c(bus, &panel_config, &control_panel), false, "create control panel failed"
    );''' + text[end:]
    return text
