"""Use official managed touch components without modifying Display Panel's vendor tree."""
from pathlib import Path
import hashlib

VENDOR_HASHES = {
    'drivers/touch/esp_panel_touch.cpp': '02e0aa883ac4e3505772f70ce2cd1d9c268512344fbe7b41bd00e3f245e61c9a',
    'drivers/touch/port/esp_lcd_touch.h': '592cb27591853597a43101e48c8e7cea333c80fec9c8f7f55d810f68b9118939',
    'drivers/touch/port/esp_lcd_touch_gt911.h': 'a9289b5e66bc64d58f300bf3e80121e22537827444e6a4fe47a2f9e77bbedb07',
}

def touch_patches(source: Path, core: Path, gt911: Path) -> dict[str, str]:
    for name, expected in VENDOR_HASHES.items():
        text = (source / "src" / name).read_text(encoding="utf-8")
        if hashlib.sha256(text.encode()).hexdigest() != expected:
            raise ValueError(f"Unreviewed Display Panel touch input: {name}")
    patches = {}
    for name, component in (("esp_lcd_touch", core), ("esp_lcd_touch_gt911", gt911)):
        header = (component / "include" / (name + ".h")).resolve()
        if not header.is_file():
            raise ValueError(f"Missing official touch header: {header}")
        # Absolute generated include prevents the identically named bundled header
        # from being selected again, independently of compiler include-path order.
        prefix = '#pragma once\n#include "../esp_panel_touch_conf_internal.h"\n'
        patches[f"drivers/touch/port/{name}.h"] = prefix + f'#include "{header.as_posix()}"\n'
    # Display Panel accepts an explicit profile address; upstream provides only
    # the default-address convenience macro. Derive the missing initializer from
    # that official macro, retaining its bus format and 100 kHz setting.
    text = (gt911 / "include/esp_lcd_touch_gt911.h").read_text(encoding="utf-8")
    start = text.index("#define ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG()")
    end = text.index("\n#ifdef __cplusplus", start)
    initializer = text[start:end]
    if initializer.count(".dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,") != 1:
        raise ValueError("Unreviewed official GT911 I2C initializer")
    initializer = initializer.replace("ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG()",
                                      "ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG_WITH_ADDR(addr)", 1)
    initializer = initializer.replace(".dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,",
                                      ".dev_addr = (addr),", 1)
    patches["drivers/touch/port/esp_lcd_touch_gt911.h"] += "\n" + initializer
    patches["drivers/touch/port/esp_lcd_touch.h"] += """
#if CONFIG_ESP_LCD_TOUCH_MAX_POINTS != ESP_PANEL_DRIVERS_TOUCH_MAX_POINTS
#error "Aura touch core and Display Panel point capacities must agree"
#endif
#if CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS != ESP_PANEL_DRIVERS_TOUCH_MAX_BUTTONS
#error "Aura touch core and Display Panel button capacities must agree"
#endif
"""
    name = "drivers/touch/esp_panel_touch.cpp"
    text = (source / "src" / name).read_text(encoding="utf-8")
    old = """    utils::vector<uint16_t> x(points_num);
    utils::vector<uint16_t> y(points_num);
    utils::vector<uint16_t> strength(points_num);
    auto x_buf = x.data();
    auto y_buf = y.data();
    auto strength_buf = strength.data();
    uint8_t ret_points_num = 0;

    // Get the point coordinates from the raw data
    esp_lcd_touch_get_coordinates(touch_panel, x_buf, y_buf, strength_buf, &ret_points_num, points_num);"""
    new = """    utils::vector<esp_lcd_touch_point_data_t> data(points_num);
    uint8_t ret_points_num = 0;

    // Official API keeps a sample available for repeated get_data() calls.
    // Display Panel's former GT911 get_xy() consumed it. Retain that wrapper
    // contract so a later non-ready/key frame cannot replay an old press.
    const esp_err_t result =
        esp_lcd_touch_get_data(touch_panel, data.data(), &ret_points_num, points_num);
    portENTER_CRITICAL(&touch_panel->data.lock);
    touch_panel->data.points = 0;
    portEXIT_CRITICAL(&touch_panel->data.lock);
    ESP_UTILS_CHECK_ERROR_RETURN(result, false, "Get touch data failed");"""
    if text.count(old) != 1:
        raise ValueError("Unexpected Display Panel point reader")
    text = text.replace(old, new, 1)
    old = "_points.emplace_back(static_cast<int>(x_buf[i]), static_cast<int>(y_buf[i]), static_cast<int>(strength_buf[i]));"
    new = "_points.emplace_back(static_cast<int>(data[i].x), static_cast<int>(data[i].y), static_cast<int>(data[i].strength));"
    if text.count(old) != 1:
        raise ValueError("Unexpected Display Panel point output")
    patches[name] = text.replace(old, new, 1)
    return patches
