// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#if AURA_NATIVE_IDF
#include "lvgl.h"
#include "esp_heap_caps.h"
#include <cstring>

// Keep the previous PSRAM-first policy with internal RAM fallback. The display
// framebuffers are allocated separately by the RGB driver.
extern "C" {
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}
void *lv_malloc_core(size_t size) {
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void *lv_realloc_core(void *pointer, size_t size) {
    return heap_caps_realloc_prefer(pointer, size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void lv_free_core(void *pointer) { heap_caps_free(pointer); }
void lv_mem_monitor_core(lv_mem_monitor_t *monitor) {
    // The allocator is shared with the rest of the application. Use Aura's
    // MemoryMonitor for system heap metrics, not invented LVGL-only totals.
    if (monitor) std::memset(monitor, 0, sizeof(*monitor));
}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
}
#endif
