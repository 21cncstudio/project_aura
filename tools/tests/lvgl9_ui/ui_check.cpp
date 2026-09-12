// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <lvgl.h>
#include "ui/screens.h"
#include "ui/ThemeColorStorage.h"
#include "ui/UiCo2CardPresentation.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

static int flushed = 0;
static uint16_t *presented = nullptr;
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *pixels) {
    if (lv_display_flush_is_last(display)) {
        presented = reinterpret_cast<uint16_t *>(pixels);
        ++flushed;
    }
    lv_display_flush_ready(display);
}
static void log_message(lv_log_level_t level, const char *message) {
    std::fputs(message, stderr);
    if (level >= LV_LOG_LEVEL_ERROR) std::abort();
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::filesystem::create_directories(argv[1]);
    // Every old RGB565 preference must survive a LVGL 9 load/save round trip.
    for (uint32_t value = 0; value <= 0xffff; ++value)
        assert(ThemeColorStorage::encode(ThemeColorStorage::decode(value)) == value);
    assert(ThemeColorStorage::encode(lv_color_hex(0xff0000)) == 0xf800);
    assert(ThemeColorStorage::encode(lv_color_hex(0x00ff00)) == 0x07e0);
    assert(ThemeColorStorage::encode(lv_color_hex(0x0000ff)) == 0x001f);

    lv_init();
    lv_log_register_print_cb(log_message);
    std::vector<uint16_t> first(800 * 480), second(first.size());
    lv_display_t *display = lv_display_create(800, 480);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, first.data(), second.data(), first.size() * 2,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, flush);
    create_screens();
    assert(objects.label_co2_warmup);
    assert(lv_obj_has_flag(objects.label_co2_warmup, LV_OBJ_FLAG_HIDDEN));
    lv_obj_t *screens[] = {objects.page_boot_logo, objects.page_boot_diag,
        objects.page_main_pro, objects.page_settings, objects.page_wifi,
        objects.page_theme, objects.page_clock, objects.page_co2_calib,
        objects.page_auto_night_mode, objects.page_backlight, objects.page_mqtt,
        objects.page_sensors_info, objects.page_dac_settings,
        objects.page_fw_update, objects.page_diag, objects.page_main_pro};
    for (size_t index = 0; index < sizeof(screens) / sizeof(*screens); ++index) {
        assert(screens[index]);
        if (index == 15) {
            const auto co2 = UiCo2CardPresentation::resolve(false, true);
            assert(co2.warmup && !co2.show_value && !co2.show_unit);
            lv_obj_remove_flag(objects.label_co2_warmup, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.label_co2_value_1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.label_co2_unit_1, LV_OBJ_FLAG_HIDDEN);
        }
        lv_screen_load(screens[index]);
        lv_refr_now(display);
        assert(presented && flushed > 0);
        auto path = std::filesystem::path(argv[1]) / (std::to_string(index) + ".ppm");
        FILE *out = std::fopen(path.string().c_str(), "wb");
        assert(out);
        std::fprintf(out, "P6\n800 480\n255\n");
        for (size_t pixel = 0; pixel < first.size(); ++pixel) {
            lv_color_t rgb = ThemeColorStorage::decode(presented[pixel]);
            const uint8_t data[] = {rgb.red, rgb.green, rgb.blue};
            assert(std::fwrite(data, 1, 3, out) == 3);
        }
        std::fclose(out);
    }
    lv_display_delete(display);
    lv_deinit();
    std::puts("All 15 generated screens and CO2 warmup rendered; all 65536 saved RGB565 colors round-trip.");
}
