"""Build an IDF 6.1 adaptation without modifying vendored Display Panel sources.

The seven changed upstream files are SHA-256 pinned after newline normalization.
Any dependency update requires reviewing these adaptations, not silently applying
them to a different library. Only generated build-directory copies are changed.
"""

import argparse
import hashlib
from pathlib import Path
from idf_i2c_adapt import adapt_panel_i2c


HASHES = {
    "drivers/bus/esp_panel_bus_i2c.hpp": "4f41eef5b1f9b7bec079cda78c55c182ad562d3e59f97a28e05313ca33c5774d",
    "drivers/host/esp_panel_host_i2c.hpp": "0301b81b9d8a906e0cf39e8c4e3b43786f1e9e6dbc19d0bda82a411cfd30dbcf",
    "drivers/host/esp_panel_host_i2c.cpp": "b0419f7b1a479bfa4dc5e2f7827bb5c3b90865ee1ef955a1bf6bab18d3a23dbd",
    "drivers/bus/esp_panel_bus_rgb.hpp": "d048d65af1b15cf741590ef029bfea55d4119ecc8b28e98c75dd6ae4187a236c",
    "drivers/bus/esp_panel_bus_rgb.cpp": "8518dba1a02d1ede33425f11f1be123e9e92ad57196c03112b7e79b9540b036b",
    "drivers/bus/esp_panel_bus_i2c.cpp": "65bcc7640810106b065f3194ab29cefd7109503ca40793609d9aad4608361b72",
    "drivers/lcd/esp_panel_lcd.cpp": "49714132a0641b0d9d5689115c3e0956dec5b802c5feb10b7a26f4e112629cda",
}


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected exactly one Display Panel patch location: {old!r}")
    return text.replace(old, new, 1)


def adapt(name, text):
    digest = hashlib.sha256(text.encode("utf-8")).hexdigest()
    if digest != HASHES[name]:
        raise ValueError(f"Upstream Display Panel changed: {name}, SHA256={digest}")
    if "_i2c." in name:
        text = adapt_panel_i2c(name, text)
    elif name.endswith("esp_panel_bus_rgb.hpp"):
        text = replace_once(text, "#ifdef SOC_LCDCAM_RGB_DATA_WIDTH",
                            "// Aura's ESP32-S3 uses a 16-line RGB bus. IDF 6 removed the old SoC macro.\n"
                            "#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)\n"
                            "    #define ESP_PANEL_BUS_RGB_DATA_BITS 16\n"
                            "#elif defined(SOC_LCDCAM_RGB_DATA_WIDTH)")
    elif name.endswith("esp_panel_bus_rgb.cpp"):
        text = replace_once(text, '#include "esp_panel_bus_rgb.hpp"',
                            '#include "esp_panel_bus_rgb.hpp"\n#include "hal/color_hal.h"\n#include <cassert>')
        text = replace_once(text, "        refresh_panel = RefreshPanelFullConfig{",
                            "        // This native adaptation is deliberately limited to Aura RGB565.\n"
                            "        assert(config.bits_per_pixel == 16 && config.data_width == 16);\n"
                            "        refresh_panel = RefreshPanelFullConfig{")
        text = replace_once(text, ".bits_per_pixel = static_cast<size_t>(config.bits_per_pixel),",
                            ".in_color_format = LCD_COLOR_FMT_RGB565,\n"
                            "            .out_color_format = LCD_COLOR_FMT_RGB565,")
        text = replace_once(text,
                            "static_cast<int>(config.bits_per_pixel)\n            , static_cast<int>(config.num_fbs)",
                            "static_cast<int>(color_hal_pixel_format_fourcc_get_bit_depth(config.in_color_format))\n"
                            "            , static_cast<int>(config.num_fbs)")
        for pin in ("hsync", "vsync", "de", "pclk", "disp"):
            text = replace_once(text, f".{pin}_gpio_num = config.{pin}_gpio_num,",
                                f".{pin}_gpio_num = static_cast<gpio_num_t>(config.{pin}_gpio_num),")
        start = text.index("            .data_gpio_nums = {")
        end = text.index("            .flags = {", start)
        pins = text[start:end]
        for index in range(16):
            pins = replace_once(pins, f"config.data_gpio_nums[{index}],",
                                f"static_cast<gpio_num_t>(config.data_gpio_nums[{index}]),")
        text = text[:start] + pins + text[end:]
    elif name.endswith("esp_panel_lcd.cpp"):
        text = replace_once(text, '#include "esp_panel_lcd.hpp"',
                            '#include "esp_panel_lcd.hpp"\n#include "hal/color_hal.h"')
        text = replace_once(text, "            .reset_gpio_num = config.reset_gpio_num,\n", "")
        text = replace_once(text, "            .vendor_config = nullptr,\n", "")
        text = replace_once(text, "            .bits_per_pixel = static_cast<uint32_t>(config.bits_per_pixel),",
                            "            .bits_per_pixel = static_cast<uint32_t>(config.bits_per_pixel),\n"
                            "            .reset_gpio_num = static_cast<gpio_num_t>(config.reset_gpio_num),\n"
                            "            .vendor_config = nullptr,")
        text = replace_once(text, "bits_per_pixel = rgb_config->bits_per_pixel;",
                            "bits_per_pixel = color_hal_pixel_format_fourcc_get_bit_depth(rgb_config->in_color_format);")
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    destination = args.destination.resolve()
    if destination == source or source in destination.parents or destination in source.parents:
        raise ValueError("The overlay must be separate from the vendored component")
    # Validate all patch inputs before writing any output.
    patches = {name: adapt(name, (source / "src" / name).read_text(encoding="utf-8"))
               for name in HASHES}
    for path in (source / "src").rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(source / "src")
        data = (patches[relative.as_posix()].encode("utf-8")
                if relative.as_posix() in patches else path.read_bytes())
        target = destination / "src" / relative
        if not target.exists() or target.read_bytes() != data:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    print("[idf-panel] verified upstream files and prepared the native-IDF overlay")


if __name__ == "__main__":
    main()
