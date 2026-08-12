/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_timer.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include <atomic>
#include <string.h>
#undef ESP_UTILS_LOG_TAG
#define ESP_UTILS_LOG_TAG "LvPort"
#include "esp_lib_utils.h"
#include "core/BootState.h"
#include "core/LvglWaitPolicy.h"
#include "core/SharedI2cRuntimeGate.h"
#include "core/TouchWakePolicy.h"
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;

#define LVGL_PORT_ENABLE_ROTATION_OPTIMIZED     (1)
#define LVGL_PORT_BUFFER_NUM_MAX                (2)

static SemaphoreHandle_t lvgl_mux = nullptr;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = nullptr;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static std::atomic<bool> lvgl_port_paused{false};
static std::atomic<bool> lvgl_pause_requested{false};
static std::atomic<bool> lvgl_display_sync_fault{false};
static volatile bool lvgl_vsync_notify_enabled = false;
static LCD *lvgl_port_lcd = nullptr;
static Touch *lvgl_port_touch = nullptr;
static bool lvgl_port_screen_flip_180 = false;
static void *lvgl_port_rotated_fb = nullptr;
static void *lvgl_buf[LVGL_PORT_BUFFER_NUM_MAX] = {};
static std::atomic<uint32_t> lvgl_touch_read_block_until_ms{0};
static std::atomic<bool> lvgl_touch_wait_release_after_block{false};
static SharedI2cRuntimeGate::Gate lvgl_touch_i2c_runtime_gate;
static TouchWakePolicy::StateMachine lvgl_touch_wake_policy;
static portMUX_TYPE lvgl_touch_wake_policy_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t lvgl_touch_interrupt_sem = nullptr;
static StaticSemaphore_t lvgl_touch_interrupt_sem_buffer;
static bool lvgl_touch_interrupt_gated = false;
static uint32_t lvgl_touch_last_sample_ms = 0;
static lv_indev_state_t lvgl_touch_cached_state = LV_INDEV_STATE_RELEASED;
static std::atomic<bool> lvgl_touch_pressed{false};
static TouchPoint lvgl_touch_cached_point = {};
static TouchWakePolicy::ErrorStreak lvgl_touch_error_streak;
static uint32_t lvgl_touch_boot_quiet_until_ms = 0;
static TouchWakePolicy::RecoveryStateMachine lvgl_touch_recovery;
static std::atomic<bool> lvgl_touch_offline{false};
static volatile uint32_t lvgl_diag_timer_handler_count = 0;
static volatile uint32_t lvgl_diag_timer_handler_last_ms = 0;
static volatile uint32_t lvgl_diag_flush_count = 0;
static volatile uint32_t lvgl_diag_flush_last_ms = 0;
static volatile uint32_t lvgl_diag_vsync_count = 0;
static volatile uint32_t lvgl_diag_vsync_last_ms = 0;
static volatile uint32_t lvgl_diag_vsync_wait_timeout_count = 0;
static volatile uint32_t lvgl_diag_lock_fail_count = 0;
static volatile uint32_t lvgl_diag_touch_read_error_count = 0;
static constexpr uint32_t LVGL_TOUCH_POLL_INTERVAL_MS = 12;
static constexpr uint32_t LVGL_TOUCH_READ_RETRY_DELAY_MS = 2;
static constexpr uint32_t LVGL_TOUCH_ERROR_BLOCK_MS_BASE = 400;
static constexpr uint32_t LVGL_TOUCH_ERROR_BLOCK_MS_STREAK = 1800;
static constexpr uint8_t LVGL_TOUCH_RECOVER_ERROR_STREAK = 3;
static constexpr uint32_t LVGL_TOUCH_RECOVER_BLOCK_MS = 300;
static constexpr uint32_t LVGL_TOUCH_RECOVER_PAUSE_MS = 120;
static constexpr uint32_t LVGL_DIAG_MAX_REASONABLE_AGE_MS = 24UL * 60UL * 60UL * 1000UL;

static inline uint32_t get_rtos_ms()
{
    return static_cast<uint32_t>(xTaskGetTickCount()) * portTICK_PERIOD_MS;
}

static inline uint32_t get_rtos_ms_isr()
{
    return static_cast<uint32_t>(xTaskGetTickCountFromISR()) * portTICK_PERIOD_MS;
}

static inline void lvgl_touch_set_cached_state(lv_indev_state_t state)
{
    lvgl_touch_cached_state = state;
    lvgl_touch_pressed.store(state == LV_INDEV_STATE_PRESSED,
                             std::memory_order_release);
}

static bool lvgl_touch_wake_policy_enabled()
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    const bool enabled = lvgl_touch_wake_policy.isEnabled();
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
    return enabled;
}

static bool lvgl_touch_wake_policy_has_pending()
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    const bool pending = lvgl_touch_wake_policy.hasPendingWake();
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
    return pending;
}

static bool lvgl_touch_wake_policy_should_probe(bool interrupt_gated,
                                                bool interrupt_pending,
                                                uint32_t now_ms)
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    const bool should_probe = lvgl_touch_wake_policy.shouldProbe(
        interrupt_gated, interrupt_pending, now_ms);
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
    return should_probe;
}

static void lvgl_touch_wake_policy_record(TouchWakePolicy::Sample sample,
                                          uint32_t now_ms)
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    lvgl_touch_wake_policy.recordProbe(sample, now_ms);
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
}

static void lvgl_touch_wake_policy_set(bool enabled,
                                       bool touch_released,
                                       uint32_t now_ms)
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    lvgl_touch_wake_policy.setEnabled(enabled, touch_released, now_ms);
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
}

static void lvgl_touch_wake_policy_reset()
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    lvgl_touch_wake_policy = TouchWakePolicy::StateMachine{};
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
}

static bool lvgl_touch_wake_policy_take_pending()
{
    portENTER_CRITICAL(&lvgl_touch_wake_policy_mux);
    const bool pending = lvgl_touch_wake_policy.takePendingWake();
    portEXIT_CRITICAL(&lvgl_touch_wake_policy_mux);
    return pending;
}

static inline void lvgl_diag_mark_timer_handler()
{
    lvgl_diag_timer_handler_last_ms = get_rtos_ms();
    ++lvgl_diag_timer_handler_count;
}

static inline void lvgl_diag_mark_flush()
{
    lvgl_diag_flush_last_ms = get_rtos_ms();
    ++lvgl_diag_flush_count;
}

static inline uint32_t lvgl_diag_age_ms(uint32_t now_ms, uint32_t stamp_ms)
{
    if (stamp_ms == 0) {
        return UINT32_MAX;
    }
    const uint32_t age_ms = now_ms - stamp_ms;
    // If stamp is sampled ahead of now due concurrent update, unsigned underflow
    // produces a near-UINT32_MAX age; treat it as unknown.
    if (age_ms > LVGL_DIAG_MAX_REASONABLE_AGE_MS) {
        return UINT32_MAX;
    }
    return age_ms;
}

#if LVGL_PORT_AVOID_TEAR
static void lvgl_port_latch_display_sync_fault(const char *reason)
{
    const bool already_faulted =
        lvgl_display_sync_fault.exchange(true, std::memory_order_acq_rel);
    if (!already_faulted) {
        ESP_UTILS_LOGE("%s; LVGL display task will fail-stop", reason);
    }
}

static bool lvgl_port_wait_for_vsync_after(uint32_t baseline_vsync_count)
{
    // eNoAction updates the notification state, not its value. Clear a stale
    // pending state after taking the baseline, then re-check the counter to
    // cover a VSYNC racing with that clear. Only a strictly newer counter is
    // accepted as ownership acknowledgement for this framebuffer.
    (void)xTaskNotifyStateClear(NULL);
    if (LvglWaitPolicy::hasNewVsync(
            baseline_vsync_count, lvgl_diag_vsync_count)) {
        return true;
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(LvglWaitPolicy::VSYNC_WAIT_TIMEOUT_MS);
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    // A pause/resume/quiesce request uses xTaskAbortDelay() to wake the LVGL
    // task from its normal delay. That same call can also abort this VSYNC
    // notification wait. Treat such an early return as a control wake, not as
    // a display timeout, and keep waiting against the original deadline.
    const uint32_t start_tick = static_cast<uint32_t>(xTaskGetTickCount());
    const uint32_t timeout_tick_count = static_cast<uint32_t>(timeout_ticks);
    for (;;) {
        if (LvglWaitPolicy::hasNewVsync(
                baseline_vsync_count, lvgl_diag_vsync_count)) {
            return true;
        }

        const uint32_t remaining_ticks = LvglWaitPolicy::remainingWaitTicks(
            start_tick,
            static_cast<uint32_t>(xTaskGetTickCount()),
            timeout_tick_count);
        if (remaining_ticks == 0) {
            break;
        }

        (void)xTaskNotifyWait(
            0, 0, nullptr, static_cast<TickType_t>(remaining_ticks));
    }

    // Prefer a VSYNC that raced with the final deadline check over latching a
    // permanent display fault.
    if (LvglWaitPolicy::hasNewVsync(
            baseline_vsync_count, lvgl_diag_vsync_count)) {
        return true;
    }

    const uint32_t timeout_count = ++lvgl_diag_vsync_wait_timeout_count;
    if (LvglWaitPolicy::shouldLogVsyncTimeout(timeout_count)) {
        ESP_UTILS_LOGE(
            "VSYNC wait timed out after %lu ms (count=%lu)",
            static_cast<unsigned long>(LvglWaitPolicy::VSYNC_WAIT_TIMEOUT_MS),
            static_cast<unsigned long>(timeout_count)
        );
    }
    lvgl_port_latch_display_sync_fault("VSYNC acknowledgement timed out");
    return false;
}

static bool lvgl_port_switch_and_confirm_vsync(LCD *lcd, void *frame_buffer)
{
    if (!lcd->switchFrameBufferTo(frame_buffer)) {
        lvgl_port_latch_display_sync_fault("LCD framebuffer switch failed");
        return false;
    }
    const uint32_t baseline_vsync_count = lvgl_diag_vsync_count;
    return lvgl_port_wait_for_vsync_after(baseline_vsync_count);
}

[[noreturn]] static void lvgl_port_fail_stop_display_task()
{
    for (;;) {
        vTaskSuspend(nullptr);
    }
}
#endif

static inline uint32_t get_monotonic_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static inline void lvgl_port_wake_task()
{
    TaskHandle_t task = lvgl_task_handle;
    if (task != nullptr) {
        xTaskAbortDelay(task);
    }
}

static inline bool is_before_deadline(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(deadline_ms - now_ms) > 0;
}

static bool IRAM_ATTR lvgl_touch_interrupt_callback(void *user_data)
{
    (void)user_data;
    BaseType_t higher_priority_task_woken = pdFALSE;
    SemaphoreHandle_t sem = lvgl_touch_interrupt_sem;
    if (sem != nullptr) {
        xSemaphoreGiveFromISR(sem, &higher_priority_task_woken);
    }
    return higher_priority_task_woken == pdTRUE;
}

static inline bool lvgl_touch_take_interrupt()
{
    return lvgl_touch_interrupt_gated &&
           (lvgl_touch_interrupt_sem != nullptr &&
            xSemaphoreTake(lvgl_touch_interrupt_sem, 0) == pdTRUE);
}

static inline void lvgl_touch_clear_interrupt()
{
    if (lvgl_touch_interrupt_sem != nullptr) {
        xSemaphoreTake(lvgl_touch_interrupt_sem, 0);
    }
}

static void lvgl_port_copy_frame_180(const lv_color_t *src, lv_color_t *dst, uint32_t width, uint32_t height)
{
    if (src == nullptr || dst == nullptr) {
        return;
    }

    const uint32_t pixel_count = width * height;
    const lv_color_t *from = src;
    lv_color_t *to = dst + pixel_count - 1;
    for (uint32_t i = 0; i < pixel_count; ++i) {
        *to-- = *from++;
    }
}

static void lvgl_port_apply_screen_flip_to_touch_point(TouchPoint &point)
{
    if (!lvgl_port_screen_flip_180) {
        return;
    }

    lv_disp_t *disp = lv_disp_get_default();
    const int hor_res = disp != nullptr ? lv_disp_get_hor_res(disp) : LV_HOR_RES;
    const int ver_res = disp != nullptr ? lv_disp_get_ver_res(disp) : LV_VER_RES;
    if (hor_res <= 0 || ver_res <= 0 || point.x < 0 || point.y < 0) {
        return;
    }

    if (point.x >= hor_res) {
        point.x = hor_res - 1;
    }
    if (point.y >= ver_res) {
        point.y = ver_res - 1;
    }
    point.x = (hor_res - 1) - point.x;
    point.y = (ver_res - 1) - point.y;
}

static bool lvgl_port_apply_screen_flip_180(bool enabled)
{
    if (lvgl_port_lcd == nullptr) {
        return false;
    }
#if LVGL_PORT_AVOID_TEAR && LVGL_PORT_DIRECT_MODE && (LVGL_PORT_ROTATION_DEGREE == 0)
    if (enabled && lvgl_port_rotated_fb == nullptr) {
        return false;
    }
#endif

    lvgl_port_screen_flip_180 = enabled;
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_wait_release_after_block.store(true, std::memory_order_release);
    lvgl_touch_read_block_until_ms.store(get_monotonic_ms() + 250,
                                         std::memory_order_release);
    lv_obj_invalidate(lv_scr_act());
    ESP_LOGI("LVGL", "screen 180 flip %s", enabled ? "enabled" : "disabled");
    return true;
}

static inline uint32_t lvgl_touch_boot_quiet_window_ms()
{
    return TouchWakePolicy::bootQuietWindowMs(boot_peripherals_cold_start);
}

static inline void lvgl_touch_fill_from_cache(lv_indev_data_t *data)
{
    if (lvgl_touch_cached_state == LV_INDEV_STATE_PRESSED) {
        data->point.x = lvgl_touch_cached_point.x;
        data->point.y = lvgl_touch_cached_point.y;
    }
    data->state = lvgl_touch_cached_state;
}

static int lvgl_touch_read_points_with_retry(
    Touch *tp,
    TouchPoint *point,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    if (!access || tp == nullptr) {
        return -1;
    }

    int result = tp->readPoints(point, 1, 0);
    if (result >= 0) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_READ_RETRY_DELAY_MS));
    return tp->readPoints(point, 1, 0);
}

static inline void lvgl_touch_note_success()
{
    lvgl_touch_error_streak.reset();
}

static bool lvgl_touch_try_soft_recover(
    Touch *tp,
    uint32_t now_ms,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    if (!access || tp == nullptr) {
        return false;
    }

    // Conservative recovery: avoid full touch re-init in read loop
    // (it can race GPIO ISR install and destabilize shared I2C devices).
    vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_RECOVER_PAUSE_MS));

    tp->resetPoints();
    TouchPoint validation_point;
    const int validation_result =
        lvgl_touch_read_points_with_retry(tp, &validation_point, access);
    const bool recovered = validation_result >= 0;
    lvgl_touch_recovery.recordAttempt(recovered, now_ms);
    lvgl_touch_offline.store(lvgl_touch_recovery.isOffline(),
                             std::memory_order_release);

    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    if (!recovered) {
        ++lvgl_diag_touch_read_error_count;
        lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
        ESP_LOGW("LVGL",
                 "touch soft recovery validation failed "
                 "(attempt=%lu, failures=%u, retry=%lu ms)",
                 static_cast<unsigned long>(lvgl_touch_recovery.attempts()),
                 static_cast<unsigned>(lvgl_touch_recovery.failStreak()),
                 static_cast<unsigned long>(lvgl_touch_recovery.cooldownMs()));
        return false;
    }

    lvgl_touch_error_streak.reset();
    lvgl_touch_wait_release_after_block.store(true, std::memory_order_release);
    lvgl_touch_read_block_until_ms.store(now_ms + LVGL_TOUCH_RECOVER_BLOCK_MS,
                                         std::memory_order_release);
    ESP_LOGI("LVGL",
             "touch soft recovery verified (attempt=%lu, success=%lu, pause=%lu ms)",
             static_cast<unsigned long>(lvgl_touch_recovery.attempts()),
             static_cast<unsigned long>(lvgl_touch_recovery.successes()),
             static_cast<unsigned long>(LVGL_TOUCH_RECOVER_PAUSE_MS));
    return true;
}

static inline void lvgl_touch_note_error(
    Touch *tp,
    uint32_t now_ms,
    const char *stage,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    ++lvgl_diag_touch_read_error_count;
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);

    const uint8_t error_streak = lvgl_touch_error_streak.recordError(now_ms);
    const uint32_t block_ms =
        (error_streak >= 2) ? LVGL_TOUCH_ERROR_BLOCK_MS_STREAK
                            : LVGL_TOUCH_ERROR_BLOCK_MS_BASE;
    lvgl_touch_read_block_until_ms.store(now_ms + block_ms,
                                         std::memory_order_release);

    if (error_streak >= LVGL_TOUCH_RECOVER_ERROR_STREAK) {
        if (lvgl_touch_recovery.canAttempt(now_ms)) {
            ESP_LOGW("LVGL",
                     "touch read errors streak=%u at %s, trying soft recovery",
                     static_cast<unsigned>(error_streak),
                     stage ? stage : "?");
            if (lvgl_touch_try_soft_recover(tp, now_ms, access)) {
                lvgl_touch_error_streak.reset();
            }
        } else if (!lvgl_touch_recovery.isOffline()) {
            // A recently successful recovery has a cooldown. If errors return
            // during it, stop polling instead of hammering the shared bus.
            lvgl_touch_recovery.suspendUntilRetry();
            lvgl_touch_offline.store(true, std::memory_order_release);
            ESP_LOGW("LVGL",
                     "touch failed again during recovery cooldown; polling suspended");
        }
    }
}

#if LV_USE_LOG
static uint32_t lvgl_dirty_warn_seen = 0;
static uint32_t lvgl_dirty_warn_suppressed = 0;
static uint32_t lvgl_dirty_warn_last_report_ms = 0;
static constexpr uint32_t LVGL_DIRTY_WARN_REPORT_PERIOD_MS = 2000;

static void lvgl_log_print_cb(const char *buf)
{
    if (buf == nullptr) {
        return;
    }

    // Keep standard LVGL logs in serial output.
    printf("%s", buf);

    if (strstr(buf, "detected modifying dirty areas in render") == nullptr) {
        return;
    }

    ++lvgl_dirty_warn_seen;

    const uint32_t now_ms = get_monotonic_ms();
    if ((lvgl_dirty_warn_last_report_ms != 0) &&
        is_before_deadline(now_ms, lvgl_dirty_warn_last_report_ms + LVGL_DIRTY_WARN_REPORT_PERIOD_MS)) {
        ++lvgl_dirty_warn_suppressed;
        return;
    }

    const char *task_name = pcTaskGetName(nullptr);
    ESP_LOGW("LVGL",
             "dirty-area mutation during render (seen=%lu, suppressed=%lu, task=%s), dumping backtrace",
             static_cast<unsigned long>(lvgl_dirty_warn_seen),
             static_cast<unsigned long>(lvgl_dirty_warn_suppressed),
             task_name ? task_name : "?");

    lvgl_dirty_warn_suppressed = 0;
    lvgl_dirty_warn_last_report_ms = now_ms;
    esp_backtrace_print(16);
}
#endif

#if LVGL_PORT_ROTATION_DEGREE != 0
static void *get_next_frame_buffer(LCD *lcd)
{
    static void *next_fb = NULL;
    static void *fbs[2] = { NULL };

    if (next_fb == NULL) {
        fbs[0] = lcd->getFrameBufferByIndex(0);
        fbs[1] = lcd->getFrameBufferByIndex(1);
        next_fb = fbs[1];
    } else {
        next_fb = (next_fb == fbs[0]) ? fbs[1] : fbs[0];
    }

    return next_fb;
}

__attribute__((always_inline))
static inline void copy_pixel_8bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
}

__attribute__((always_inline))
static inline void copy_pixel_16bpp(uint8_t *to, const uint8_t *from)
{
    *(uint16_t *)to++ = *(const uint16_t *)from++;
}

__attribute__((always_inline))
static inline void copy_pixel_24bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
    *to++ = *from++;
    *to++ = *from++;
}

#define _COPY_PIXEL(_bpp, to, from) copy_pixel_##_bpp##bpp(to, from)
#define COPY_PIXEL(_bpp, to, from)  _COPY_PIXEL(_bpp, to, from)

#define ROTATE_90_ALL_BPP() \
    { \
        to_bytes_per_line = h * to_bytes_per_pixel; \
        to_index_const = (w - x_start - 1) * to_bytes_per_line; \
        for (int from_y = y_start; from_y < y_end + 1; from_y++) { \
            from_index = from_y * from_bytes_per_line + x_start * from_bytes_per_pixel; \
            to_index = to_index_const + from_y * to_bytes_per_pixel; \
            for (int from_x = x_start; from_x < x_end + 1; from_x++) { \
                COPY_PIXEL(LV_COLOR_DEPTH, to + to_index, from + from_index); \
                from_index += from_bytes_per_pixel; \
                to_index -= to_bytes_per_line; \
            } \
        } \
    }

/**
 * @brief Optimized transpose function for RGB565 format.
 *
 * @note  ESP32-P4 1024x600 full-screen: 738ms -> 34ms
 * @note  ESP32-S3 480x480  full-screen: 380ms -> 37ms
 */
#define ROTATE_90_OPTIMIZED_16BPP(block_w, block_h) \
    { \
        for (int i = 0; i < h; i += block_h) { \
            max_height = (i + block_h > h) ? h : (i + block_h); \
            for (int j = 0; j < w; j += block_w) { \
                max_width = (j + block_w > w) ? w : (j + block_w); \
                start_y = w - 1 - j;   \
                for (int x = i; x < max_height; x++) { \
                    from_next = (uint16_t *)from + x * w; \
                    for (int y = j, mirrored_y = start_y; y < max_width; y += 4, mirrored_y -= 4) { \
                        ((uint16_t *)to)[(mirrored_y) * h + x] = *((uint32_t *)(from_next + y)) & 0xFFFF; \
                        ((uint16_t *)to)[(mirrored_y - 1) * h + x] = (*((uint32_t *)(from_next + y)) >> 16) & 0xFFFF; \
                        ((uint16_t *)to)[(mirrored_y - 2) * h + x] = *((uint32_t *)(from_next + y + 2)) & 0xFFFF; \
                        ((uint16_t *)to)[(mirrored_y - 3) * h + x] = (*((uint32_t *)(from_next + y + 2)) >> 16) & 0xFFFF; \
                    } \
                } \
            } \
        } \
    }

#define ROTATE_180_ALL_BPP() \
    { \
        to_bytes_per_line = w * to_bytes_per_pixel; \
        to_index_const = (h - 1) * to_bytes_per_line + (w - x_start - 1) * to_bytes_per_pixel; \
        for (int from_y = y_start; from_y < y_end + 1; from_y++) { \
            from_index = from_y * from_bytes_per_line + x_start * from_bytes_per_pixel; \
            to_index = to_index_const - from_y * to_bytes_per_line; \
            for (int from_x = x_start; from_x < x_end + 1; from_x++) { \
                COPY_PIXEL(LV_COLOR_DEPTH, to + to_index, from + from_index); \
                from_index += from_bytes_per_pixel; \
                to_index -= to_bytes_per_pixel; \
            } \
        } \
    }

#define ROTATE_270_OPTIMIZED_16BPP(block_w, block_h) \
    { \
        for (int i = 0; i < h; i += block_h) { \
            max_height = i + block_h > h ? h : i + block_h; \
            for (int j = 0; j < w; j += block_w) { \
                max_width = j + block_w > w ? w : j + block_w; \
                for (int x = i; x < max_height; x++) { \
                    from_next = (uint16_t *)from + x * w; \
                    for (int y = j; y < max_width; y += 4) { \
                        ((uint16_t *)to)[y * h + (h - 1 - x)] = *((uint32_t *)(from_next + y)) & 0xFFFF; \
                        ((uint16_t *)to)[(y + 1) * h + (h - 1 - x)] = (*((uint32_t *)(from_next + y)) >> 16) & 0xFFFF; \
                        ((uint16_t *)to)[(y + 2) * h + (h - 1 - x)] = *((uint32_t *)(from_next + y + 2)) & 0xFFFF; \
                        ((uint16_t *)to)[(y + 3) * h + (h - 1 - x)] = (*((uint32_t *)(from_next + y + 2)) >> 16) & 0xFFFF; \
                    } \
                } \
            } \
        } \
    }

#define ROTATE_270_ALL_BPP() \
    { \
        to_bytes_per_line = h * to_bytes_per_pixel; \
        from_index_const = x_start * from_bytes_per_pixel; \
        to_index_const = x_start * to_bytes_per_line + (h - 1) * to_bytes_per_pixel; \
        for (int from_y = y_start; from_y < y_end + 1; from_y++) { \
            from_index = from_y * from_bytes_per_line + from_index_const; \
            to_index = to_index_const - from_y * to_bytes_per_pixel; \
            for (int from_x = x_start; from_x < x_end + 1; from_x++) { \
                COPY_PIXEL(LV_COLOR_DEPTH, to + to_index, from + from_index); \
                from_index += from_bytes_per_pixel; \
                to_index += to_bytes_per_line; \
            } \
        } \
    }

__attribute__((always_inline))
IRAM_ATTR static inline void rotate_copy_pixel(
    const uint8_t *from, uint8_t *to, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t w,
    uint16_t h, uint16_t rotate
)
{
    int from_bytes_per_pixel = sizeof(lv_color_t);
    int from_bytes_per_line = w * from_bytes_per_pixel;
    int from_index = 0;

    int to_bytes_per_pixel = LV_COLOR_DEPTH >> 3;
    int to_bytes_per_line;
    int to_index = 0;
    int to_index_const = 0;

#if (LV_COLOR_DEPTH == 16) && LVGL_PORT_ENABLE_ROTATION_OPTIMIZED
    int max_height = 0;
    int max_width = 0;
    int start_y = 0;
    uint16_t *from_next = NULL;
#endif

    // uint32_t time = esp_log_timestamp();
    switch (rotate) {
    case 90:
#if (LV_COLOR_DEPTH == 16) && LVGL_PORT_ENABLE_ROTATION_OPTIMIZED
        ROTATE_90_OPTIMIZED_16BPP(32, 256);
#else
        ROTATE_90_ALL_BPP();
#endif
        break;
    case 180:
        ROTATE_180_ALL_BPP();
        break;
    case 270:
#if (LV_COLOR_DEPTH == 16) && LVGL_PORT_ENABLE_ROTATION_OPTIMIZED
        ROTATE_270_OPTIMIZED_16BPP(32, 256);
#else
        int from_index_const = 0;
        ROTATE_270_ALL_BPP();
#endif
        break;
    default:
        break;
    }
    // ESP_LOGI(TAG, "rotate: end, time used:%d", (int)(esp_log_timestamp() - time));
}
#endif /* LVGL_PORT_ROTATION_DEGREE */

#if LVGL_PORT_AVOID_TEAR
#if LVGL_PORT_DIRECT_MODE
#if LVGL_PORT_ROTATION_DEGREE != 0
typedef struct {
    uint16_t inv_p;
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
} lv_port_dirty_area_t;

static lv_port_dirty_area_t dirty_area;

static void flush_dirty_save(lv_port_dirty_area_t *dirty_area)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    dirty_area->inv_p = disp->inv_p;
    for (int i = 0; i < disp->inv_p; i++) {
        dirty_area->inv_area_joined[i] = disp->inv_area_joined[i];
        dirty_area->inv_areas[i] = disp->inv_areas[i];
    }
}

typedef enum {
    FLUSH_STATUS_PART,
    FLUSH_STATUS_FULL
} lv_port_flush_status_t;

typedef enum {
    FLUSH_PROBE_PART_COPY,
    FLUSH_PROBE_SKIP_COPY,
    FLUSH_PROBE_FULL_COPY,
} lv_port_flush_probe_t;

/**
 * @brief Probe dirty area to copy
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 */
static lv_port_flush_probe_t flush_copy_probe(lv_disp_drv_t *drv)
{
    static lv_port_flush_status_t prev_status = FLUSH_STATUS_PART;
    lv_port_flush_status_t cur_status;
    lv_port_flush_probe_t probe_result;
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    uint32_t flush_ver = 0;
    uint32_t flush_hor = 0;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i] == 0) {
            flush_ver = (disp_refr->inv_areas[i].y2 + 1 - disp_refr->inv_areas[i].y1);
            flush_hor = (disp_refr->inv_areas[i].x2 + 1 - disp_refr->inv_areas[i].x1);
            break;
        }
    }
    /* Check if the current full screen refreshes */
    cur_status = ((flush_ver == drv->ver_res) && (flush_hor == drv->hor_res)) ? (FLUSH_STATUS_FULL) : (FLUSH_STATUS_PART);

    if (prev_status == FLUSH_STATUS_FULL) {
        if ((cur_status == FLUSH_STATUS_PART)) {
            probe_result = FLUSH_PROBE_FULL_COPY;
        } else {
            probe_result = FLUSH_PROBE_SKIP_COPY;
        }
    } else {
        probe_result = FLUSH_PROBE_PART_COPY;
    }
    prev_status = cur_status;

    return probe_result;
}

static inline void *flush_get_next_buf(LCD *lcd)
{
    return get_next_frame_buffer(lcd);
}

/**
 * @brief Copy dirty area
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 */
static void flush_dirty_copy(void *dst, void *src, lv_port_dirty_area_t *dirty_area)
{
    lv_coord_t x_start, x_end, y_start, y_end;
    for (int i = 0; i < dirty_area->inv_p; i++) {
        /* Refresh the unjoined areas*/
        if (dirty_area->inv_area_joined[i] == 0) {
            x_start = dirty_area->inv_areas[i].x1;
            x_end = dirty_area->inv_areas[i].x2;
            y_start = dirty_area->inv_areas[i].y1;
            y_end = dirty_area->inv_areas[i].y2;

            rotate_copy_pixel(
                (uint8_t *)src, (uint8_t *)dst, x_start, y_start, x_end, y_end, LV_HOR_RES, LV_VER_RES,
                LVGL_PORT_ROTATION_DEGREE
            );
        }
    }
}

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lvgl_diag_mark_flush();
    LCD *lcd = (LCD *)drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = NULL;
    lv_port_flush_probe_t probe_result = FLUSH_PROBE_PART_COPY;
    lv_disp_t *disp = lv_disp_get_default();

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        /* Check if the `full_refresh` flag has been triggered */
        if (drv->full_refresh) {
            /* Reset flag */
            drv->full_refresh = 0;

            // Rotate and copy data from the whole screen LVGL's buffer to the next frame buffer
            next_fb = flush_get_next_buf(lcd);
            rotate_copy_pixel(
                (uint8_t *)color_map, (uint8_t *)next_fb, offsetx1, offsety1, offsetx2, offsety2,
                LV_HOR_RES, LV_VER_RES, LVGL_PORT_ROTATION_DEGREE
            );

            if (!lvgl_port_switch_and_confirm_vsync(lcd, next_fb)) {
                lvgl_port_fail_stop_display_task();
            }

            /* Synchronously update the dirty area for another frame buffer */
            flush_dirty_copy(flush_get_next_buf(lcd), color_map, &dirty_area);
            flush_get_next_buf(lcd);
        } else {
            /* Probe the copy method for the current dirty area */
            probe_result = flush_copy_probe(drv);

            if (probe_result == FLUSH_PROBE_FULL_COPY) {
                /* Save current dirty area for next frame buffer */
                flush_dirty_save(&dirty_area);

                /* Set LVGL full-refresh flag and set flush ready in advance */
                drv->full_refresh = 1;
                disp->rendering_in_progress = false;
                lv_disp_flush_ready(drv);

                /* Force to refresh whole screen, and will invoke `flush_callback` recursively */
                lv_refr_now(_lv_refr_get_disp_refreshing());
            } else {
                /* Update current dirty area for next frame buffer */
                next_fb = flush_get_next_buf(lcd);
                flush_dirty_save(&dirty_area);
                flush_dirty_copy(next_fb, color_map, &dirty_area);

                if (!lvgl_port_switch_and_confirm_vsync(lcd, next_fb)) {
                    lvgl_port_fail_stop_display_task();
                }

                if (probe_result == FLUSH_PROBE_PART_COPY) {
                    /* Synchronously update the dirty area for another frame buffer */
                    flush_dirty_save(&dirty_area);
                    flush_dirty_copy(flush_get_next_buf(lcd), color_map, &dirty_area);
                    flush_get_next_buf(lcd);
                }
            }
        }
    }

    lv_disp_flush_ready(drv);
}

#else

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lvgl_diag_mark_flush();
    LCD *lcd = (LCD *)drv->user_data;

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        void *next_frame_buffer = color_map;
        if (lvgl_port_screen_flip_180 && lvgl_port_rotated_fb != nullptr) {
            lvgl_port_copy_frame_180(
                color_map, static_cast<lv_color_t *>(lvgl_port_rotated_fb), drv->hor_res, drv->ver_res
            );
            next_frame_buffer = lvgl_port_rotated_fb;
        }

        if (!lvgl_port_switch_and_confirm_vsync(lcd, next_frame_buffer)) {
            lvgl_port_fail_stop_display_task();
        }
    }

    lv_disp_flush_ready(drv);
}
#endif /* LVGL_PORT_ROTATION_DEGREE */

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_DISP_BUFFER_NUM == 2

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lvgl_diag_mark_flush();
    LCD *lcd = (LCD *)drv->user_data;

    if (!lvgl_port_switch_and_confirm_vsync(lcd, color_map)) {
        lvgl_port_fail_stop_display_task();
    }

    lv_disp_flush_ready(drv);
}

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_DISP_BUFFER_NUM == 3

#if LVGL_PORT_ROTATION_DEGREE == 0
static void *lvgl_port_lcd_last_buf = NULL;
static void *lvgl_port_lcd_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;
#endif

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lvgl_diag_mark_flush();
    LCD *lcd = (LCD *)drv->user_data;

#if LVGL_PORT_ROTATION_DEGREE != 0
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = get_next_frame_buffer(lcd);

    /* Rotate and copy dirty area from the current LVGL's buffer to the next LCD frame buffer */
    rotate_copy_pixel(
        (uint8_t *)color_map, (uint8_t *)next_fb, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES,
        LV_VER_RES, LVGL_PORT_ROTATION_DEGREE
    );

    /* Triple-buffer mode tracks ownership in the VSYNC callback. */
    if (!lcd->switchFrameBufferTo(next_fb)) {
        lvgl_port_latch_display_sync_fault("LCD framebuffer switch failed");
        lvgl_port_fail_stop_display_task();
    }
#else
    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;

    /* Triple-buffer mode tracks ownership in the VSYNC callback. */
    if (!lcd->switchFrameBufferTo(color_map)) {
        lvgl_port_latch_display_sync_fault("LCD framebuffer switch failed");
        lvgl_port_fail_stop_display_task();
    }

    lvgl_port_lcd_next_buf = color_map;
#endif

    lv_disp_flush_ready(drv);
}
#endif

IRAM_ATTR bool onLcdVsyncCallback(void *user_data)
{
    (void)user_data;
    TaskHandle_t task_handle = lvgl_task_handle;
    if (task_handle == nullptr) {
        return false;
    }
    if (!lvgl_vsync_notify_enabled) {
        return false;
    }
    BaseType_t need_yield = pdFALSE;
    lvgl_diag_vsync_last_ms = get_rtos_ms_isr();
    ++lvgl_diag_vsync_count;
#if LVGL_PORT_FULL_REFRESH && (LVGL_PORT_DISP_BUFFER_NUM == 3) && (LVGL_PORT_ROTATION_DEGREE == 0)
    if (lvgl_port_lcd_next_buf != lvgl_port_lcd_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_lcd_last_buf;
        lvgl_port_lcd_last_buf = lvgl_port_lcd_next_buf;
    }
#else
    // Notify that the current LCD frame buffer has been transmitted
    xTaskNotifyFromISR(task_handle, 0, eNoAction, &need_yield);
#endif
    return (need_yield == pdTRUE);
}

#else

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lvgl_diag_mark_flush();
    LCD *lcd = (LCD *)drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)color_map);
    // For RGB LCD, directly notify LVGL that the buffer is ready
    if (lcd->getBus()->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        lv_disp_flush_ready(drv);
    }
}

static void update_callback(lv_disp_drv_t *drv)
{
    LCD *lcd = (LCD *)drv->user_data;
    auto transformation = lcd->getTransformation();
    static bool disp_init_mirror_x = transformation.mirror_x;
    static bool disp_init_mirror_y = transformation.mirror_y;
    static bool disp_init_swap_xy = transformation.swap_xy;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        lcd->swapXY(disp_init_swap_xy);
        lcd->mirrorX(disp_init_mirror_x);
        lcd->mirrorY(disp_init_mirror_y);
        break;
    case LV_DISP_ROT_90:
        lcd->swapXY(!disp_init_swap_xy);
        lcd->mirrorX(disp_init_mirror_x);
        lcd->mirrorY(!disp_init_mirror_y);
        break;
    case LV_DISP_ROT_180:
        lcd->swapXY(disp_init_swap_xy);
        lcd->mirrorX(!disp_init_mirror_x);
        lcd->mirrorY(!disp_init_mirror_y);
        break;
    case LV_DISP_ROT_270:
        lcd->swapXY(!disp_init_swap_xy);
        lcd->mirrorX(!disp_init_mirror_x);
        lcd->mirrorY(disp_init_mirror_y);
        break;
    }

    ESP_UTILS_LOGD("Update display rotation to %d", drv->rotated);

#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    transformation = lcd->getTransformation();
    disp_init_mirror_x = transformation.mirror_x;
    disp_init_mirror_y = transformation.mirror_y;
    disp_init_swap_xy = transformation.swap_xy;
    ESP_UTILS_LOGD("Current mirror x: %d, mirror y: %d, swap xy: %d", disp_init_mirror_x, disp_init_mirror_y, disp_init_swap_xy);
#endif
}

#endif /* LVGL_PORT_AVOID_TEAR */

void rounder_callback(lv_disp_drv_t *drv, lv_area_t *area)
{
    LCD *lcd = (LCD *)drv->user_data;
    uint8_t x_align = lcd->getBasicAttributes().basic_bus_spec.x_coord_align;
    uint8_t y_align = lcd->getBasicAttributes().basic_bus_spec.y_coord_align;

    if (x_align > 1) {
        // round the start of coordinate down to the nearest aligned value
        area->x1 &= ~(x_align - 1);
        // round the end of coordinate up to the nearest aligned value
        area->x2 = (area->x2 & ~(x_align - 1)) + x_align - 1;
    }

    if (y_align > 1) {
        // round the start of coordinate down to the nearest aligned value
        area->y1 &= ~(y_align - 1);
        // round the end of coordinate up to the nearest aligned value
        area->y2 = (area->y2 & ~(y_align - 1)) + y_align - 1;
    }
}

static lv_disp_t *display_init(LCD *lcd)
{
    ESP_UTILS_CHECK_FALSE_RETURN(lcd != nullptr, nullptr, "Invalid LCD device");
    ESP_UTILS_CHECK_FALSE_RETURN(lcd->getRefreshPanelHandle() != nullptr, nullptr, "LCD device is not initialized");

    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;

    // Alloc draw buffers used by LVGL
    auto lcd_width = lcd->getFrameWidth();
    auto lcd_height = lcd->getFrameHeight();
    int buffer_size = 0;

    ESP_UTILS_LOGD("Malloc memory for LVGL buffer");
#if !LVGL_PORT_AVOID_TEAR
    // Avoid tearing function is disabled
    buffer_size = lcd_width * LVGL_PORT_BUFFER_SIZE_HEIGHT;
    for (int i = 0; (i < LVGL_PORT_BUFFER_NUM) && (i < LVGL_PORT_BUFFER_NUM_MAX); i++) {
        lvgl_buf[i] = heap_caps_malloc(buffer_size * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
        if (lvgl_buf[i] == nullptr) {
            ESP_UTILS_LOGE(
                "Allocate LVGL buffer[%d] failed (size=%d)",
                i,
                buffer_size * static_cast<int>(sizeof(lv_color_t)));
            return nullptr;
        }
        ESP_UTILS_LOGD("Buffer[%d] address: %p, size: %d", i, lvgl_buf[i], buffer_size * sizeof(lv_color_t));
    }
#else
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for LCD refresh
    buffer_size = lcd_width * lcd_height;
#if (LVGL_PORT_DISP_BUFFER_NUM >= 3) && (LVGL_PORT_ROTATION_DEGREE == 0) && LVGL_PORT_FULL_REFRESH

    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering,
    // eliminating the need to wait for the LCD's sync signal
    lvgl_port_lcd_last_buf = lcd->getFrameBufferByIndex(0);
    lvgl_buf[0] = lcd->getFrameBufferByIndex(1);
    lvgl_buf[1] = lcd->getFrameBufferByIndex(2);
    lvgl_port_lcd_next_buf = lvgl_port_lcd_last_buf;
    lvgl_port_flush_next_buf = lvgl_buf[1];

#elif (LVGL_PORT_DISP_BUFFER_NUM >= 3) && (LVGL_PORT_ROTATION_DEGREE != 0)

    lvgl_buf[0] = lcd->getFrameBufferByIndex(2);

#elif LVGL_PORT_DISP_BUFFER_NUM >= 2

    for (int i = 0; (i < LVGL_PORT_DISP_BUFFER_NUM) && (i < LVGL_PORT_BUFFER_NUM_MAX); i++) {
        lvgl_buf[i] = lcd->getFrameBufferByIndex(i);
    }
#if LVGL_PORT_DIRECT_MODE && (LVGL_PORT_ROTATION_DEGREE == 0) && (LVGL_PORT_DISP_BUFFER_NUM >= 3)
    lvgl_port_rotated_fb = lcd->getFrameBufferByIndex(2);
#endif

#endif
#endif /* LVGL_PORT_AVOID_TEAR */

    if (lvgl_buf[0] == nullptr) {
        ESP_UTILS_LOGE("Primary LVGL draw buffer is unavailable");
        return nullptr;
    }
#if !LVGL_PORT_AVOID_TEAR
#if LVGL_PORT_BUFFER_NUM > 1
    if (lvgl_buf[1] == nullptr) {
        ESP_UTILS_LOGE("Secondary LVGL draw buffer is unavailable");
        return nullptr;
    }
#endif
#elif !((LVGL_PORT_DISP_BUFFER_NUM >= 3) && (LVGL_PORT_ROTATION_DEGREE != 0))
    if (lvgl_buf[1] == nullptr) {
        ESP_UTILS_LOGE("Secondary LVGL draw buffer is unavailable");
        return nullptr;
    }
#endif

    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, lvgl_buf[0], lvgl_buf[1], buffer_size);

    ESP_UTILS_LOGD("Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = flush_callback;
#if (LVGL_PORT_ROTATION_DEGREE == 90) || (LVGL_PORT_ROTATION_DEGREE == 270)
    disp_drv.hor_res = lcd_height;
    disp_drv.ver_res = lcd_width;
#else
    disp_drv.hor_res = lcd_width;
    disp_drv.ver_res = lcd_height;
#endif
#if LVGL_PORT_AVOID_TEAR    // Only available when the tearing effect is enabled
#if LVGL_PORT_FULL_REFRESH
    disp_drv.full_refresh = 1;
#elif LVGL_PORT_DIRECT_MODE
    disp_drv.direct_mode = 1;
#endif
#else                       // Only available when the tearing effect is disabled
    if (lcd->getBasicAttributes().basic_bus_spec.isFunctionValid(LCD::BasicBusSpecification::FUNC_SWAP_XY) &&
            lcd->getBasicAttributes().basic_bus_spec.isFunctionValid(LCD::BasicBusSpecification::FUNC_MIRROR_X) &&
            lcd->getBasicAttributes().basic_bus_spec.isFunctionValid(LCD::BasicBusSpecification::FUNC_MIRROR_Y)) {
        disp_drv.drv_update_cb = update_callback;
    } else {
        disp_drv.sw_rotate = 1;
    }
#endif /* LVGL_PORT_AVOID_TEAR */
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = (void *)lcd;
    // Only available when the coordinate alignment is enabled
    if ((lcd->getBasicAttributes().basic_bus_spec.x_coord_align > 1) ||
            (lcd->getBasicAttributes().basic_bus_spec.y_coord_align > 1)) {
        disp_drv.rounder_cb = rounder_callback;
    }

    return lv_disp_drv_register(&disp_drv);
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    Touch *tp = (Touch *)indev_drv->user_data;
    TouchPoint point;
    const uint32_t now_ms = get_monotonic_ms();
    if (tp == nullptr) {
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if ((lvgl_touch_boot_quiet_until_ms != 0) &&
        is_before_deadline(now_ms, lvgl_touch_boot_quiet_until_ms)) {
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if (lvgl_touch_recovery.isOffline()) {
        if (lvgl_touch_recovery.canAttempt(now_ms)) {
            ESP_LOGW("LVGL", "touch offline, retrying soft recovery");
            if (lvgl_touch_try_soft_recover(tp, now_ms, access)) {
                lvgl_touch_error_streak.reset();
            }
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if (lvgl_touch_wake_policy_enabled()) {
        const uint32_t block_until_ms =
            lvgl_touch_read_block_until_ms.load(std::memory_order_acquire);
        if (block_until_ms != 0) {
            if (is_before_deadline(now_ms, block_until_ms)) {
                lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
                data->state = lvgl_touch_cached_state;
                return;
            }
            lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
        }

        if (lvgl_touch_wake_policy_has_pending()) {
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
            data->state = lvgl_touch_cached_state;
            return;
        }

        const bool interrupt_pending = lvgl_touch_take_interrupt();
        // Prefer a fresh GT911 interrupt. A sparse fallback probe prevents a
        // lost interrupt from making the dark screen impossible to wake while
        // avoiding the continuous I2C traffic which caused the original fault.
        if (!lvgl_touch_wake_policy_should_probe(
                lvgl_touch_interrupt_gated, interrupt_pending, now_ms)) {
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
            data->state = lvgl_touch_cached_state;
            return;
        }

        int wake_probe =
            lvgl_touch_read_points_with_retry(tp, &point, access);
        if (wake_probe > 0) {
            lvgl_touch_wake_policy_record(TouchWakePolicy::Sample::Pressed, now_ms);
            lvgl_touch_note_success();
        } else if (wake_probe < 0) {
            lvgl_touch_wake_policy_record(TouchWakePolicy::Sample::Error, now_ms);
            lvgl_touch_note_error(tp, now_ms, "wake_probe", access);
        } else {
            lvgl_touch_wake_policy_record(TouchWakePolicy::Sample::Released, now_ms);
            lvgl_touch_note_success();
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    const uint32_t block_until_ms =
        lvgl_touch_read_block_until_ms.load(std::memory_order_acquire);
    if (block_until_ms != 0) {
        if (is_before_deadline(now_ms, block_until_ms)) {
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
            data->state = lvgl_touch_cached_state;
            return;
        }
        lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    }

    // After wake block ends, require a clean release first. This avoids
    // turning a wake touch into an accidental click on a UI control.
    if (lvgl_touch_wait_release_after_block.load(std::memory_order_acquire)) {
        int release_probe =
            lvgl_touch_read_points_with_retry(tp, &point, access);
        if (release_probe <= 0) {
            lvgl_touch_wait_release_after_block.store(false,
                                                       std::memory_order_release);
            if (release_probe < 0) {
                lvgl_touch_note_error(tp, now_ms, "release_probe", access);
            } else {
                lvgl_touch_note_success();
            }
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if ((lvgl_touch_last_sample_ms != 0) &&
        is_before_deadline(now_ms, lvgl_touch_last_sample_ms + LVGL_TOUCH_POLL_INTERVAL_MS)) {
        lvgl_touch_fill_from_cache(data);
        return;
    }

    /* Read data from touch controller */
    lvgl_touch_last_sample_ms = now_ms;
    int read_touch_result =
        lvgl_touch_read_points_with_retry(tp, &point, access);
    if (read_touch_result > 0) {
        lvgl_touch_note_success();
        lvgl_port_apply_screen_flip_to_touch_point(point);
        lvgl_touch_cached_point = point;
        lvgl_touch_set_cached_state(LV_INDEV_STATE_PRESSED);
    } else {
        if (read_touch_result < 0) {
            lvgl_touch_note_error(tp, now_ms, "read", access);
        } else {
            lvgl_touch_note_success();
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    }
    lvgl_touch_fill_from_cache(data);
}

static lv_indev_t *indev_init(Touch *tp)
{
    ESP_UTILS_CHECK_FALSE_RETURN(tp != nullptr, nullptr, "Invalid touch device");
    ESP_UTILS_CHECK_FALSE_RETURN(tp->getPanelHandle() != nullptr, nullptr, "Touch device is not initialized");

    static lv_indev_drv_t indev_drv_tp;

    if (tp->isInterruptEnabled()) {
        lvgl_touch_interrupt_sem =
            xSemaphoreCreateBinaryStatic(&lvgl_touch_interrupt_sem_buffer);
        if (lvgl_touch_interrupt_sem != nullptr &&
            tp->attachInterruptCallback(lvgl_touch_interrupt_callback, nullptr)) {
            lvgl_touch_interrupt_gated = true;
            lvgl_touch_clear_interrupt();
        } else {
            lvgl_touch_interrupt_sem = nullptr;
            ESP_LOGW("LVGL", "touch interrupt gate unavailable; wake probe will use polling fallback");
        }
    }

    ESP_UTILS_LOGD("Register input driver to LVGL");
    lv_indev_drv_init(&indev_drv_tp);
    indev_drv_tp.type = LV_INDEV_TYPE_POINTER;
    indev_drv_tp.read_cb = touchpad_read;
    indev_drv_tp.user_data = (void *)tp;

    return lv_indev_drv_register(&indev_drv_tp);
}

#if !LV_TICK_CUSTOM
static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static bool tick_init(void)
{
    if (lvgl_tick_timer != nullptr) {
        ESP_UTILS_LOGE("LVGL tick timer is already initialized");
        return false;
    }

    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    const esp_err_t create_result =
        esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    if (create_result != ESP_OK) {
        lvgl_tick_timer = nullptr;
        ESP_UTILS_LOGE(
            "Create LVGL tick timer failed: %s", esp_err_to_name(create_result));
        return false;
    }

    const esp_err_t start_result = esp_timer_start_periodic(
        lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
    if (start_result != ESP_OK) {
        ESP_UTILS_LOGE(
            "Start LVGL tick timer failed: %s", esp_err_to_name(start_result));
        const esp_err_t delete_result = esp_timer_delete(lvgl_tick_timer);
        if (delete_result == ESP_OK) {
            lvgl_tick_timer = nullptr;
        } else {
            ESP_UTILS_LOGE(
                "Delete unstarted LVGL tick timer failed: %s",
                esp_err_to_name(delete_result));
        }
        return false;
    }

    return true;
}

static bool tick_deinit(void)
{
    if (lvgl_tick_timer == nullptr) {
        return true;
    }

    bool success = true;
    const esp_err_t stop_result = esp_timer_stop(lvgl_tick_timer);
    if ((stop_result != ESP_OK) &&
        (stop_result != ESP_ERR_INVALID_STATE)) {
        ESP_UTILS_LOGE(
            "Stop LVGL tick timer failed: %s", esp_err_to_name(stop_result));
        success = false;
    }

    const esp_err_t delete_result = esp_timer_delete(lvgl_tick_timer);
    if (delete_result == ESP_OK) {
        lvgl_tick_timer = nullptr;
    } else {
        ESP_UTILS_LOGE(
            "Delete LVGL tick timer failed: %s", esp_err_to_name(delete_result));
        success = false;
    }
    return success;
}
#endif

static bool lvgl_port_cleanup_partial_init(lv_disp_t *disp, lv_indev_t *indev)
{
    // This helper is only used before an LVGL task has been created, so all
    // resources can be released without suspending another task or taking the
    // LVGL mutex.
    bool success = true;
    lvgl_vsync_notify_enabled = false;

    if (lvgl_port_lcd != nullptr) {
        lvgl_port_lcd->attachDrawBitmapFinishCallback(nullptr, nullptr);
    }
    if (lvgl_port_touch != nullptr &&
        lvgl_port_touch->isInterruptEnabled()) {
        lvgl_port_touch->attachInterruptCallback(nullptr, nullptr);
    }
    lvgl_touch_interrupt_gated = false;
    lvgl_touch_interrupt_sem = nullptr;

#if !LV_TICK_CUSTOM
    if (!tick_deinit()) {
        success = false;
    }
#endif

    if (indev != nullptr) {
        lv_indev_delete(indev);
    }
    if (disp != nullptr) {
        lv_disp_remove(disp);
    }

#if LV_USE_LOG
    lv_log_register_print_cb(nullptr);
#endif

    if (lvgl_mux != nullptr) {
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = nullptr;
    }

#if !LVGL_PORT_AVOID_TEAR
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM_MAX; ++i) {
        if (lvgl_buf[i] != nullptr) {
            free(lvgl_buf[i]);
            lvgl_buf[i] = nullptr;
        }
    }
#else
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM_MAX; ++i) {
        lvgl_buf[i] = nullptr;
    }
#endif
#if LVGL_PORT_AVOID_TEAR && LVGL_PORT_FULL_REFRESH && \
    (LVGL_PORT_DISP_BUFFER_NUM == 3) && (LVGL_PORT_ROTATION_DEGREE == 0)
    lvgl_port_lcd_last_buf = nullptr;
    lvgl_port_lcd_next_buf = nullptr;
    lvgl_port_flush_next_buf = nullptr;
#endif

    lvgl_task_handle = nullptr;
    lvgl_pause_requested.store(false, std::memory_order_release);
    lvgl_port_paused.store(true, std::memory_order_release);
    lvgl_port_lcd = nullptr;
    lvgl_port_touch = nullptr;
    lvgl_port_rotated_fb = nullptr;
    lvgl_port_screen_flip_180 = false;
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_wake_policy_reset();
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_cached_point = {};
    lvgl_touch_last_sample_ms = 0;
    lvgl_touch_error_streak.reset();
    lvgl_touch_recovery.reset();
    lvgl_touch_boot_quiet_until_ms = 0;
    lvgl_touch_offline.store(false, std::memory_order_release);

    return success;
}

static void lvgl_port_task(void *arg)
{
    ESP_UTILS_LOGD("Starting LVGL task");

    uint32_t task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_pause_requested.load(std::memory_order_acquire)) {
            if (!lvgl_port_paused.load(std::memory_order_acquire)) {
                lvgl_vsync_notify_enabled = false;
                // In avoid-tear builds the IRAM-safe vendor wrapper rejects a
                // null refresh callback. Keep it installed but inert here.
#if !LV_TICK_CUSTOM
                if (lvgl_tick_timer != nullptr) {
                    esp_timer_stop(lvgl_tick_timer);
                }
#endif
                lvgl_port_paused.store(true, std::memory_order_release);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (lvgl_port_paused.load(std::memory_order_acquire)) {
#if LVGL_PORT_AVOID_TEAR
            if (lvgl_port_lcd != nullptr) {
                lvgl_vsync_notify_enabled = true;
                lvgl_port_lcd->attachRefreshFinishCallback(onLcdVsyncCallback, (void *)lvgl_task_handle);
            }
#endif
#if !LV_TICK_CUSTOM
            if (lvgl_tick_timer != nullptr) {
                esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
            }
#endif
            lvgl_port_paused.store(false, std::memory_order_release);
        }

        if (lvgl_port_lock(-1)) {
            lvgl_diag_mark_timer_handler();
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

IRAM_ATTR bool onDrawBitmapFinishCallback(void *user_data)
{
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_data;

    lv_disp_flush_ready(drv);

    return false;
}

bool lvgl_port_init(LCD *lcd, Touch *tp)
{
    ESP_UTILS_CHECK_FALSE_RETURN(lcd != nullptr, false, "Invalid LCD device");
    ESP_UTILS_CHECK_FALSE_RETURN(
        (lvgl_task_handle == nullptr) && (lvgl_mux == nullptr),
        false,
        "LVGL port is already initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(
        lvgl_touch_i2c_runtime_gate.resetForBoot(),
        false,
        "Touch I2C gate still has active users");
    lvgl_display_sync_fault.store(false, std::memory_order_release);
    lvgl_diag_vsync_wait_timeout_count = 0;
    lvgl_vsync_notify_enabled = false;
    lvgl_task_handle = nullptr;
    lvgl_mux = nullptr;
    lvgl_pause_requested.store(false, std::memory_order_release);
    lvgl_port_paused.store(false, std::memory_order_release);
    lvgl_port_lcd = nullptr;
    lvgl_port_touch = nullptr;
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_wake_policy_reset();
    lvgl_touch_interrupt_sem = nullptr;
    lvgl_touch_interrupt_gated = false;
    lvgl_touch_last_sample_ms = 0;
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_cached_point = {};
    lvgl_touch_error_streak.reset();
    lvgl_touch_recovery.reset();
    lvgl_touch_offline.store(false, std::memory_order_release);
    lvgl_port_rotated_fb = nullptr;
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM_MAX; ++i) {
        lvgl_buf[i] = nullptr;
    }
    lvgl_touch_boot_quiet_until_ms = get_monotonic_ms() + lvgl_touch_boot_quiet_window_ms();
    lvgl_touch_read_block_until_ms.store(lvgl_touch_boot_quiet_until_ms,
                                         std::memory_order_release);

    auto bus_type = lcd->getBus()->getBasicAttributes().type;
#if LVGL_PORT_AVOID_TEAR
    ESP_UTILS_CHECK_FALSE_RETURN(
        (bus_type == ESP_PANEL_BUS_TYPE_RGB) || (bus_type == ESP_PANEL_BUS_TYPE_MIPI_DSI), false,
        "Avoid tearing function only works with RGB/MIPI-DSI LCD now"
    );
    ESP_UTILS_LOGI(
        "Avoid tearing is enabled, mode: %d, rotation: %d", LVGL_PORT_AVOID_TEARING_MODE, LVGL_PORT_ROTATION_DEGREE
    );
#endif

    lv_disp_t *disp = nullptr;
    lv_indev_t *indev = nullptr;

    lv_init();
#if LV_USE_LOG
    lv_log_register_print_cb(lvgl_log_print_cb);
#endif
#if !LV_TICK_CUSTOM
    if (!tick_init()) {
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Initialize LVGL tick failed");
        return false;
    }
#endif

    ESP_UTILS_LOGI("Initializing LVGL display driver");
    disp = display_init(lcd);
    if (disp == nullptr) {
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Initialize LVGL display driver failed");
        return false;
    }
    // Record the initial rotation of the display
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);
    lvgl_port_lcd = lcd;

    // For non-RGB LCD, need to notify LVGL that the buffer is ready when the refresh is finished
    if (bus_type != ESP_PANEL_BUS_TYPE_RGB) {
        ESP_UTILS_LOGD("Attach refresh finish callback to LCD");
        lcd->attachDrawBitmapFinishCallback(onDrawBitmapFinishCallback, (void *)disp->driver);
    }

    if (tp != nullptr) {
        ESP_UTILS_LOGD("Initialize LVGL input driver");
        lvgl_port_touch = tp;
        indev = indev_init(tp);
        if (indev == nullptr) {
            (void)lvgl_port_cleanup_partial_init(disp, indev);
            ESP_UTILS_LOGE("Initialize LVGL input driver failed");
            return false;
        }

#if LVGL_PORT_ROTATION_DEGREE != 0
        auto &transformation = tp->getTransformation();
#if LVGL_PORT_ROTATION_DEGREE == 90
        tp->swapXY(!transformation.swap_xy);
        tp->mirrorY(!transformation.mirror_y);
#elif LVGL_PORT_ROTATION_DEGREE == 180
        tp->mirrorX(!transformation.mirror_x);
        tp->mirrorY(!transformation.mirror_y);
#elif LVGL_PORT_ROTATION_DEGREE == 270
        tp->swapXY(!transformation.swap_xy);
        tp->mirrorX(!transformation.mirror_x);
#endif
#endif
    }

    lvgl_port_screen_flip_180 = false;

    ESP_UTILS_LOGD("Create mutex for LVGL");
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mux == nullptr) {
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Create LVGL mutex failed");
        return false;
    }

    ESP_UTILS_LOGD("Create LVGL task");
    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL,
                     LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);
    if (ret != pdPASS) {
        lvgl_task_handle = nullptr;
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Create LVGL task failed");
        return false;
    }

#if LVGL_PORT_AVOID_TEAR
    lvgl_vsync_notify_enabled = true;
    lcd->attachRefreshFinishCallback(onLcdVsyncCallback, (void *)lvgl_task_handle);
#endif
    lvgl_port_paused.store(false, std::memory_order_release);
    lvgl_pause_requested.store(false, std::memory_order_release);

    return true;
}

bool lvgl_port_lock(int timeout_ms)
{
    ESP_UTILS_CHECK_NULL_RETURN(lvgl_mux, false, "LVGL mutex is not initialized");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    const bool locked = (xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE);
    if (!locked) {
        ++lvgl_diag_lock_fail_count;
    }
    return locked;
}

bool lvgl_port_unlock(void)
{
    ESP_UTILS_CHECK_NULL_RETURN(lvgl_mux, false, "LVGL mutex is not initialized");

    xSemaphoreGiveRecursive(lvgl_mux);

    return true;
}

bool lvgl_port_set_screen_flip_180(bool enabled)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    ESP_UTILS_CHECK_FALSE_RETURN(
        static_cast<bool>(access), false, "Touch I2C runtime is disabled");
    ESP_UTILS_CHECK_FALSE_RETURN(
        lvgl_port_lock(LvglWaitPolicy::SCREEN_FLIP_LOCK_TIMEOUT_MS), false, "Lock LVGL failed"
    );
    const bool ok = lvgl_port_apply_screen_flip_180(enabled);
    ESP_UTILS_CHECK_FALSE_RETURN(lvgl_port_unlock(), false, "Unlock LVGL failed");
    return ok;
}

bool lvgl_port_get_screen_flip_180(void)
{
    return lvgl_port_screen_flip_180;
}

bool lvgl_port_block_touch_read(uint32_t duration_ms)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        return false;
    }
    const uint32_t now_ms = get_monotonic_ms();
    if (duration_ms == 0) {
        lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
        lvgl_touch_wait_release_after_block.store(false,
                                                   std::memory_order_release);
    } else {
        lvgl_touch_read_block_until_ms.store(now_ms + duration_ms,
                                             std::memory_order_release);
        lvgl_touch_wait_release_after_block.store(true,
                                                  std::memory_order_release);
    }
    return true;
}

void lvgl_port_disable_touch_i2c(void)
{
    if (!lvgl_touch_i2c_runtime_gate.disable()) {
        return;
    }

    ESP_LOGW("LVGL", "shared I2C bus offline; new touch transactions disabled");
}

bool lvgl_port_finalize_touch_i2c_disable(uint32_t timeout_ms)
{
    (void)lvgl_touch_i2c_runtime_gate.disable();
    const uint32_t started_ms = get_monotonic_ms();
    while (!lvgl_touch_i2c_runtime_gate.idle()) {
        if (static_cast<uint32_t>(get_monotonic_ms() - started_ms) >= timeout_ms) {
            return false;
        }
        vTaskDelay(1);
    }

    const uint32_t now_ms = get_monotonic_ms();
    lvgl_touch_wake_policy_set(false, true, now_ms);
    lvgl_touch_clear_interrupt();
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_offline.store(true, std::memory_order_release);
    return true;
}

bool lvgl_port_set_wake_touch_probe(bool enabled)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        return !enabled;
    }
    if (enabled == lvgl_touch_wake_policy_enabled()) {
        return true;
    }

    const uint32_t now_ms = get_monotonic_ms();
    const bool touch_released =
        !lvgl_touch_pressed.load(std::memory_order_acquire);
    lvgl_touch_wake_policy_set(enabled, touch_released, now_ms);
    lvgl_touch_clear_interrupt();
    return true;
}

bool lvgl_port_take_wake_touch_pending(void)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        return false;
    }
    return lvgl_touch_wake_policy_take_pending();
}

bool lvgl_port_get_diagnostics(lvgl_port_diagnostics_t *out)
{
    ESP_UTILS_CHECK_FALSE_RETURN(out != nullptr, false, "Invalid diagnostics snapshot");

    const uint32_t now_ms = get_rtos_ms();
    const uint32_t timer_last_ms = lvgl_diag_timer_handler_last_ms;
    const uint32_t flush_last_ms = lvgl_diag_flush_last_ms;
    const uint32_t vsync_last_ms = lvgl_diag_vsync_last_ms;

    out->sample_ms = now_ms;
    out->timer_handler_count = lvgl_diag_timer_handler_count;
    out->timer_handler_age_ms = lvgl_diag_age_ms(now_ms, timer_last_ms);
    out->flush_count = lvgl_diag_flush_count;
    out->flush_age_ms = lvgl_diag_age_ms(now_ms, flush_last_ms);
    out->vsync_count = lvgl_diag_vsync_count;
    out->vsync_age_ms = lvgl_diag_age_ms(now_ms, vsync_last_ms);
    out->vsync_wait_timeout_count = lvgl_diag_vsync_wait_timeout_count;
    out->display_sync_fault =
        lvgl_display_sync_fault.load(std::memory_order_acquire);
    out->lock_fail_count = lvgl_diag_lock_fail_count;
    out->touch_read_error_count = lvgl_diag_touch_read_error_count;
    out->touch_offline = lvgl_touch_offline.load(std::memory_order_acquire);
    out->paused = lvgl_port_paused.load(std::memory_order_acquire);

    return true;
}

bool lvgl_port_pause(void)
{
    return lvgl_port_request_pause() && lvgl_port_is_paused();
}

bool lvgl_port_request_pause(void)
{
    lvgl_pause_requested.store(true, std::memory_order_release);
    lvgl_port_wake_task();
    return true;
}

bool lvgl_port_is_paused(void)
{
    return lvgl_port_paused.load(std::memory_order_acquire);
}

bool lvgl_port_resume(void)
{
    return lvgl_port_request_resume() && !lvgl_port_is_paused();
}

bool lvgl_port_request_resume(void)
{
    lvgl_pause_requested.store(false, std::memory_order_release);
    lvgl_port_wake_task();
    return true;
}

bool lvgl_port_request_quiesce(void)
{
    TaskHandle_t task = lvgl_task_handle;
    if (task == nullptr) {
        return false;
    }

    // Leave VSYNC callbacks and the tick timer alive until the task has
    // cooperatively stopped between handler iterations. Disabling them here
    // could strand an in-flight framebuffer hand-off and turn a clean pause
    // request into a display-sync fail-stop.
    lvgl_pause_requested.store(true, std::memory_order_release);
    lvgl_port_wake_task();
    return true;
}

bool lvgl_port_deinit(void)
{
    TaskHandle_t task = lvgl_task_handle;
    ESP_UTILS_CHECK_FALSE_RETURN(
        (task == nullptr) || (task != xTaskGetCurrentTaskHandle()),
        false,
        "LVGL task cannot deinitialize itself");
    ESP_UTILS_CHECK_FALSE_RETURN(
        lvgl_port_lock(LvglWaitPolicy::DEINIT_LOCK_TIMEOUT_MS),
        false,
        "Lock LVGL for deinit failed");

#if !LV_TICK_CUSTOM
    if (!tick_deinit()) {
        (void)lvgl_port_unlock();
        ESP_UTILS_LOGE("Deinitialize LVGL tick failed");
        return false;
    }
#endif

    // Keep VSYNC notifications alive until the finite mutex acquisition above
    // proves that no flush is still waiting for the current frame hand-off.
    lvgl_vsync_notify_enabled = false;
    if (task != nullptr) {
        vTaskDelete(task);
        lvgl_task_handle = nullptr;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(lvgl_port_unlock(), false, "Unlock LVGL failed");

#if LV_ENABLE_GC || !LV_MEM_CUSTOM
    lv_deinit();
#else
    ESP_UTILS_LOGW("LVGL memory is custom, `lv_deinit()` will not work");
#endif
#if !LVGL_PORT_AVOID_TEAR
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM; i++) {
        if (lvgl_buf[i] != nullptr) {
            free(lvgl_buf[i]);
            lvgl_buf[i] = nullptr;
        }
    }
#endif
    if (lvgl_mux != nullptr) {
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = nullptr;
    }

    return true;
}

bool lvgl_port_prepare_restart(void)
{
    // Request the safe path first. Callers that need to protect shared-I2C
    // shutdown work must wait for this cooperative pause before entering this
    // final, force-capable teardown.
    (void)lvgl_port_request_quiesce();

    // Prevent VSYNC ISR from notifying a task handle during reboot teardown.
    lvgl_vsync_notify_enabled = false;
    if (lvgl_port_lcd != nullptr) {
        lvgl_port_lcd->attachDrawBitmapFinishCallback(nullptr, nullptr);
    }
    // ESP32_Display_Panel rejects a null refresh callback when
    // CONFIG_LCD_RGB_ISR_IRAM_SAFE is enabled. Do not pretend to detach it:
    // the gate above makes the retained callback inert, and the task handle
    // is cleared below after the LVGL task has been suspended.
    if (lvgl_port_touch != nullptr && lvgl_port_touch->isInterruptEnabled()) {
        lvgl_port_touch->attachInterruptCallback(nullptr, nullptr);
    }
    lvgl_touch_interrupt_gated = false;
    lvgl_touch_interrupt_sem = nullptr;
#if !LV_TICK_CUSTOM
    if (lvgl_tick_timer != nullptr) {
        esp_timer_stop(lvgl_tick_timer);
    }
#endif
    TaskHandle_t task = lvgl_task_handle;
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    if ((task != nullptr) && (task != current)) {
        vTaskSuspend(task);
    }
    lvgl_task_handle = nullptr;
    lvgl_pause_requested.store(false, std::memory_order_release);
    lvgl_port_paused.store(true, std::memory_order_release);
    return true;
}
