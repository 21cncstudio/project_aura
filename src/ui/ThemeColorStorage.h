// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <lvgl.h>

// Theme preferences use RGB565, as on LVGL 8. The web API remains hex RGB.
// LVGL 9 represents lv_color_t in RGB888 regardless of the display format.
namespace ThemeColorStorage {
inline uint32_t encode(lv_color_t color) {
#if LVGL_VERSION_MAJOR >= 9
    return lv_color_to_u16(color);
#else
    return color.full;
#endif
}

inline lv_color_t decode(uint32_t stored) {
#if LVGL_VERSION_MAJOR >= 9
    const uint16_t rgb565 = static_cast<uint16_t>(stored);
    const uint8_t red = (rgb565 >> 11) & 31;
    const uint8_t green = (rgb565 >> 5) & 63;
    const uint8_t blue = rgb565 & 31;
    return lv_color_make((red << 3) | (red >> 2),
                         (green << 2) | (green >> 4),
                         (blue << 3) | (blue >> 2));
#else
    lv_color_t color;
    color.full = stored;
    return color;
#endif
}
} // namespace ThemeColorStorage
