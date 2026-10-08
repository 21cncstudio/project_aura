// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "display/NativeRotationConfig.h"
#if AURA_NATIVE_ROTATION
#include "display/NativeRotation.h"
#include "src/draw/lv_draw_private.h"
#include "src/display/lv_display_private.h"
#include "src/misc/lv_area_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include <cstdlib>
#include <cstring>

#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6 || LVGL_VERSION_PATCH != 0
#error Revalidate native rendering against the new LVGL blend and sync contracts.
#endif
#if LV_USE_OS != LV_OS_NONE || LV_DRAW_SW_DRAW_UNIT_CNT != 1 || LV_DRAW_TRANSFORM_USE_MATRIX || LV_USE_VECTOR_GRAPHIC || LV_USE_DRAW_SW_ASM
#error Native rotation requires the validated single-unit software renderer.
#endif

namespace NativeRotation {
namespace {
lv_display_t *display = nullptr;
void *buffers[2] = {};
const void *presented = nullptr;
int width = 0, height = 0;
bool flipped = false;
FaultHandler faultHandler = nullptr;

[[noreturn]] void fail(const char *reason) {
    if(faultHandler) faultHandler(reason);
    std::abort(); // Never continue writing if a fault callback returns.
}
bool isRoot(const void *p) { return p && (p == buffers[0] || p == buffers[1]); }
lv_area_t physical(const lv_area_t &a) {
    if(!flipped) return a;
    return {width - 1 - a.x2, height - 1 - a.y2,
            width - 1 - a.x1, height - 1 - a.y1};
}

void blend(lv_draw_task_t *task, const lv_draw_sw_blend_dsc_t *d) {
    auto *layer = task->target_layer;
    auto *draw = layer->draw_buf;
    lv_area_t a;
    if(!lv_area_intersect(&a, d->blend_area, &task->clip_area)) return;
    if(d->src_buf && !lv_area_intersect(&a, &a, d->src_area)) return;
    if(d->src_buf && d->mask_area && !lv_area_intersect(&a, &a, d->mask_area)) return;
    const bool root = isRoot(draw->data);
    if(root && !canSubmit(draw->data)) fail("Native draw targets active scanout");
    if(root && (layer->buf_area.x1 != 0 || layer->buf_area.y1 != 0 ||
                layer->buf_area.x2 != width - 1 || layer->buf_area.y2 != height - 1))
        fail("Native root layer is not a full framebuffer");
    if(a.x1 < layer->buf_area.x1 || a.y1 < layer->buf_area.y1 ||
       a.x2 > layer->buf_area.x2 || a.y2 > layer->buf_area.y2)
        fail("Native blend outside layer");
    const bool rotate = root && flipped;
    const int stride = draw->header.stride;
    const uint8_t *mask = d->mask_buf && d->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER ? d->mask_buf : nullptr;
    const int maskStride = mask ? (d->mask_stride ? d->mask_stride : lv_area_get_width(d->mask_area)) : 0;
    if(mask) mask += (a.y1 - d->mask_area->y1) * maskStride + a.x1 - d->mask_area->x1;

    // Flat fills are invariant under 180 degrees, including their opacity.
    // Keep LVGL's optimized span kernels and map only the destination rectangle.
    // Unrotated root buffers and RGB565 intermediate layers use normal spans too.
    if(!d->src_buf && (!rotate || !mask)) {
        lv_area_t dest = rotate ? physical(a) : a;
        lv_area_move(&dest, -layer->buf_area.x1, -layer->buf_area.y1);
        lv_draw_sw_blend_fill_dsc_t f{};
        f.dest_buf = draw->data + dest.y1 * stride + dest.x1 * 2;
        f.dest_w = lv_area_get_width(&a); f.dest_h = lv_area_get_height(&a);
        f.dest_stride = stride; f.color = d->color; f.opa = d->opa;
        f.mask_buf = mask; f.mask_stride = maskStride; f.relative_area = dest;
        lv_draw_sw_blend_color_to_rgb565(&f);
        return;
    }
    const int bits = d->src_buf ? lv_color_format_get_bpp(d->src_color_format) : 0;
    if(d->src_buf && bits != 1 && bits < 8) fail("Unsupported sub-byte native source");
    if(d->src_buf && !rotate) {
        lv_draw_sw_blend_image_dsc_t im{};
        im.dest_buf = draw->data + (a.y1 - layer->buf_area.y1) * stride + (a.x1 - layer->buf_area.x1) * 2;
        im.dest_w = lv_area_get_width(&a); im.dest_h = lv_area_get_height(&a); im.dest_stride = stride;
        im.src_buf = static_cast<const uint8_t *>(d->src_buf) + (a.y1 - d->src_area->y1) * d->src_stride + ((a.x1 - d->src_area->x1) * bits >> 3);
        im.src_stride = d->src_stride; im.src_color_format = d->src_color_format;
        im.opa = d->opa; im.blend_mode = d->blend_mode;
        im.mask_buf = mask; im.mask_stride = maskStride;
        im.relative_area = a; lv_area_move(&im.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
        im.src_area = *d->src_area; lv_area_move(&im.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
        lv_draw_sw_blend_image_to_rgb565(&im);
        return;
    }
    // Masked fills/images: blend directly into descending physical addresses.
    // Stock scalar kernels preserve exact 9.6 alpha/mask rounding and formats.
    // No scratch row, transformed image or post-render rotation is allocated.
    for(int y = a.y1; y <= a.y2; ++y) {
        auto *dst = reinterpret_cast<uint16_t *>(draw->data + (height - 1 - y) * stride) + width - 1 - a.x1;
        const auto *maskRow = mask ? mask + (y - a.y1) * maskStride : nullptr;
        const auto *srcRow = d->src_buf ? static_cast<const uint8_t *>(d->src_buf) + (y - d->src_area->y1) * d->src_stride : nullptr;
        for(int x = a.x1; x <= a.x2; ++x) {
            auto *pixel = dst - (x - a.x1);
            const auto *pixelMask = maskRow ? maskRow + x - a.x1 : nullptr;
            if(!srcRow) {
                lv_draw_sw_blend_fill_dsc_t f{};
                f.dest_buf = pixel; f.dest_w = f.dest_h = 1; f.dest_stride = 2;
                f.color = d->color; f.opa = d->opa; f.mask_buf = pixelMask; f.mask_stride = 1;
                lv_draw_sw_blend_color_to_rgb565(&f);
            } else {
                const int sx = x - d->src_area->x1;
                if(d->src_color_format == LV_COLOR_FORMAT_RGB565 && d->blend_mode == LV_BLEND_MODE_NORMAL && !pixelMask && d->opa >= LV_OPA_MAX) {
                    std::memcpy(pixel, srcRow + sx * 2, 2);
                    continue;
                }
                uint8_t bit = bits == 1 ? ((srcRow[sx / 8] >> (7 - (sx & 7))) & 1) << 7 : 0;
                lv_draw_sw_blend_image_dsc_t im{};
                im.dest_buf = pixel; im.dest_w = im.dest_h = 1; im.dest_stride = 2;
                im.src_buf = bits == 1 ? &bit : srcRow + sx * (bits / 8);
                im.src_stride = d->src_stride; im.src_color_format = d->src_color_format;
                im.opa = d->opa; im.blend_mode = d->blend_mode; im.mask_buf = pixelMask; im.mask_stride = 1;
                lv_draw_sw_blend_image_to_rgb565(&im);
            }
        }
    }
}

void sync(lv_display_t *d, const lv_area_t *logical) {
    auto *dest = lv_display_get_buf_active(d);
    if(d != display || !canSubmit(dest->data) || !isRoot(presented)) fail("Native sync ownership violation");
    const lv_area_t a = physical(*logical);
    if(a.x1 < 0 || a.y1 < 0 || a.x2 >= width || a.y2 >= height || a.x1 > a.x2 || a.y1 > a.y2)
        fail("Native sync outside framebuffer");
    const int stride = dest->header.stride;
    for(int y = a.y1; y <= a.y2; ++y) {
        const int offset = y * stride + a.x1 * 2;
        std::memcpy(dest->data + offset, static_cast<const uint8_t *>(presented) + offset, (a.x2 - a.x1 + 1) * 2);
    }
    lv_display_sync_ready(d);
}
} // namespace

bool begin(lv_display_t *d, void *initial_scanout, bool flip_180, FaultHandler fault) {
    if(display || !d || !fault || lv_draw_sw_get_blend_handler(LV_COLOR_FORMAT_RGB565)) return false;
    auto *a = d->buf_1, *b = d->buf_2;
    const int w = lv_display_get_horizontal_resolution(d), h = lv_display_get_vertical_resolution(d);
    if(!a || !b || !a->data || !b->data || a->data == b->data ||
       initial_scanout != b->data || lv_display_get_buf_active(d) != a || d->buf_3 ||
       (lv_display_get_render_mode(d) != LV_DISPLAY_RENDER_MODE_DIRECT && lv_display_get_render_mode(d) != LV_DISPLAY_RENDER_MODE_FULL) ||
       lv_display_get_color_format(d) != LV_COLOR_FORMAT_RGB565 || w != 800 || h != 480 ||
       a->header.stride != b->header.stride || a->header.stride < unsigned(w * 2) ||
       (a->header.stride & 1) || a->data_size < a->header.stride * h || b->data_size < b->header.stride * h)
        return false;
    lv_draw_sw_custom_blend_handler_t handler{LV_COLOR_FORMAT_RGB565, blend};
    if(!lv_draw_sw_register_blend_handler(&handler)) return false;
    display = d; buffers[0] = a->data; buffers[1] = b->data; presented = initial_scanout;
    width = w; height = h; flipped = flip_180; faultHandler = fault;
    lv_display_set_sync_cb(d, sync);
    return true;
}
bool canSubmit(const void *buffer) {
    return display && isRoot(buffer) && buffer != presented && lv_display_get_buf_active(display)->data == buffer;
}
bool acknowledge(const void *buffer) {
    if(!canSubmit(buffer)) return false;
    presented = buffer;
    return true;
}
void reset() {
    if(display) {
        lv_display_set_sync_cb(display, nullptr);
        lv_draw_sw_unregister_blend_handler(LV_COLOR_FORMAT_RGB565);
    }
    display = nullptr; buffers[0] = buffers[1] = nullptr; presented = nullptr;
    faultHandler = nullptr;
}
} // namespace NativeRotation
#endif
