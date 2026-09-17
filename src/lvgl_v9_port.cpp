/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_timer.h"
#include "esp_debug_helpers.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include <atomic>
#include <string.h>
#undef ESP_UTILS_LOG_TAG
#define ESP_UTILS_LOG_TAG "LvPort"
#include "esp_lib_utils.h"
#include "core/BootState.h"
#include "core/Gt911RuntimePolicy.h"
#include "core/LvglLockDiagnostics.h"
#include "core/LvglWaitPolicy.h"
#include "core/RotatedFramebufferPolicy.h"
#include "core/ScreenOnTouchPolicy.h"
#include "core/SharedI2cRuntimeGate.h"
#include "core/TouchReleaseGatePolicy.h"
#include "core/TouchWakePolicy.h"
#include "core/WakePowerGuard.h"
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;

static_assert(
    LV_DEF_REFR_PERIOD == ScreenOnTouchPolicy::CALLBACK_INTERVAL_MS,
    "ScreenOnTouchPolicy timing must match the LVGL input callback period");

#define LVGL_PORT_ENABLE_ROTATION_OPTIMIZED     (1)
#define LVGL_PORT_BUFFER_NUM_MAX                (2)

static bool lvgl_flush_in_progress = false;
static SemaphoreHandle_t lvgl_mux = nullptr;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = nullptr;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static std::atomic<bool> lvgl_port_paused{false};
static std::atomic<bool> lvgl_pause_requested{false};
static std::atomic<bool> lvgl_display_sync_fault{false};
static std::atomic<bool> lvgl_display_task_fail_stopped{false};
static std::atomic<bool> lvgl_diagnostics_available{false};
static std::atomic<uint32_t> lvgl_presented_frame_count{0};
static volatile bool lvgl_vsync_notify_enabled = false;
static LCD *lvgl_port_lcd = nullptr;
static Touch *lvgl_port_touch = nullptr;
static std::atomic<bool> lvgl_port_screen_flip_180{false};
static std::atomic<bool> lvgl_rotation_pipeline_active{false};
static void *lvgl_port_driver_fb[RotatedFramebufferPolicy::FRAME_BUFFER_COUNT] = {};
// ESP-IDF starts an RGB panel on framebuffer zero. Every later ownership
// change is published only after the refresh callback acknowledgement.
static int lvgl_port_active_scanout_index = -1;
static RotatedFramebufferPolicy::FlipLayout lvgl_port_flip_layout;
static void *lvgl_buf[LVGL_PORT_BUFFER_NUM_MAX] = {};
static std::atomic<uint32_t> lvgl_touch_read_block_until_ms{0};
static std::atomic<bool> lvgl_touch_wait_release_after_block{false};
static SharedI2cRuntimeGate::Gate lvgl_touch_i2c_runtime_gate;
// Runtime callers are serialized by the recursive LVGL mutex. Initialization
// runs before the LVGL task starts, and shutdown mutates this state only after
// the touch-I2C gate has drained. The GPIO ISR never accesses this object, so a
// separate portMUX would add an interrupt-disabled spin path without protecting
// any real concurrent access.
static TouchWakePolicy::StateMachine lvgl_touch_wake_policy;
static ScreenOnTouchPolicy::State lvgl_touch_screen_on_policy;
static TouchReleaseGatePolicy::Gate lvgl_touch_release_gate;
static std::atomic<uint8_t> lvgl_touch_release_gate_request{0};
static DRAM_ATTR TouchWakePolicy::InterruptLatch lvgl_touch_interrupt_latch;
// `gated` means that the bounded direct ISR is registered. `armed` means that
// its GPIO source is physically enabled. One lifecycle owner may arm it:
// dark-wake detection or screen-on idle after a verified release.
static std::atomic<bool> lvgl_touch_interrupt_gated{false};
static std::atomic<bool> lvgl_touch_interrupt_armed{false};
static std::atomic<esp_lcd_touch_handle_t>
    lvgl_touch_registered_interrupt_handle{nullptr};
static std::atomic<lvgl_port_touch_mode_t> lvgl_touch_mode{
    LVGL_PORT_TOUCH_MODE_SUPPRESSED};
static std::atomic<bool> lvgl_touch_screen_reset_requested{false};
static bool lvgl_touch_irq_config_attempted = false;
static std::atomic<bool> lvgl_touch_irq_config_verified{false};
static std::atomic<int8_t> lvgl_touch_irq_config_mode{-1};
static std::atomic<bool> lvgl_touch_screen_idle_fail_safe{false};
static std::atomic<uint8_t> lvgl_touch_screen_policy_mode_diag{
    static_cast<uint8_t>(ScreenOnTouchPolicy::Mode::FastInteraction)};
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
static std::atomic<uint32_t> lvgl_diag_stack_min_free_bytes{0};
static volatile uint32_t lvgl_diag_flush_count = 0;
static volatile uint32_t lvgl_diag_flush_last_ms = 0;
static volatile uint32_t lvgl_diag_vsync_count = 0;
static volatile uint32_t lvgl_diag_vsync_last_ms = 0;
static volatile uint32_t lvgl_diag_refresh_callback_max_gap_ms = 0;
static volatile uint32_t lvgl_diag_vsync_wait_timeout_count = 0;
static volatile uint32_t lvgl_diag_rotated_copy_switch_count = 0;
static volatile uint32_t lvgl_diag_framebuffer_ownership_violation_count = 0;
static lvgl_port_refresh_callback_semantics_t lvgl_refresh_callback_semantics =
    LVGL_PORT_REFRESH_CALLBACK_UNKNOWN;
static LvglLockDiagnostics::Counters lvgl_diag_lock_counts;
static volatile uint32_t lvgl_diag_touch_read_error_count = 0;
static std::atomic<uint32_t> lvgl_diag_touch_status_read_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_full_read_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_idle_skip_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_idle_entry_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_idle_irq_exit_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_idle_fallback_probe_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_idle_missed_irq_press_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_irq_arm_failure_count{0};
static std::atomic<uint32_t> lvgl_diag_touch_irq_no_frame_count{0};
static constexpr uint32_t LVGL_TOUCH_POLL_INTERVAL_MS = 12;
static constexpr uint32_t LVGL_TOUCH_READ_RETRY_DELAY_MS = 2;
static constexpr uint32_t LVGL_TOUCH_ERROR_BLOCK_MS_BASE = 400;
static constexpr uint32_t LVGL_TOUCH_ERROR_BLOCK_MS_STREAK = 1800;
static constexpr uint8_t LVGL_TOUCH_RECOVER_ERROR_STREAK = 3;
static constexpr uint32_t LVGL_TOUCH_RECOVER_BLOCK_MS = 300;
static constexpr uint32_t LVGL_TOUCH_RECOVER_PAUSE_MS = 120;
static constexpr uint8_t LVGL_TOUCH_RELEASE_GATE_REQUEST_PENDING = 1U << 0;
static constexpr uint8_t LVGL_TOUCH_RELEASE_GATE_REQUIRE_EXPLICIT = 1U << 1;
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
    return lvgl_touch_wake_policy.isEnabled();
}

static bool lvgl_touch_wake_policy_has_pending()
{
    return lvgl_touch_wake_policy.hasPendingWake();
}

static bool lvgl_touch_wake_policy_should_probe(bool interrupt_gated,
                                                bool interrupt_pending,
                                                uint32_t now_ms)
{
    return lvgl_touch_wake_policy.shouldProbe(
        interrupt_gated, interrupt_pending, now_ms);
}

static void lvgl_touch_wake_policy_record(TouchWakePolicy::Sample sample,
                                          uint32_t now_ms,
                                          bool selected_by_fresh_interrupt = false)
{
    lvgl_touch_wake_policy.recordProbe(
        sample, now_ms, selected_by_fresh_interrupt);
}

static bool lvgl_touch_wake_policy_take_fast_retry()
{
    return lvgl_touch_wake_policy.takeFastRetry();
}

static void lvgl_touch_wake_policy_set(bool enabled,
                                       bool touch_released,
                                       uint32_t now_ms)
{
    lvgl_touch_wake_policy.setEnabled(enabled, touch_released, now_ms);
}

static void lvgl_touch_wake_policy_reset()
{
    lvgl_touch_wake_policy = TouchWakePolicy::StateMachine{};
}

static bool lvgl_touch_wake_policy_take_pending()
{
    return lvgl_touch_wake_policy.takePendingWake();
}

static inline lvgl_port_touch_mode_t lvgl_touch_current_mode()
{
    return lvgl_touch_mode.load(std::memory_order_acquire);
}

static inline void lvgl_touch_publish_screen_policy_mode()
{
    lvgl_touch_screen_policy_mode_diag.store(
        static_cast<uint8_t>(lvgl_touch_screen_on_policy.mode()),
        std::memory_order_release);
}

static const char *lvgl_touch_runtime_mode_text()
{
    switch (lvgl_touch_current_mode()) {
    case LVGL_PORT_TOUCH_MODE_SUPPRESSED:
        return "suppressed";
    case LVGL_PORT_TOUCH_MODE_DARK_WAKE:
        return "dark_wake";
    case LVGL_PORT_TOUCH_MODE_DISABLED:
        return "disabled";
    case LVGL_PORT_TOUCH_MODE_SCREEN_ON:
        break;
    }

    switch (static_cast<ScreenOnTouchPolicy::Mode>(
        lvgl_touch_screen_policy_mode_diag.load(std::memory_order_acquire))) {
    case ScreenOnTouchPolicy::Mode::FastInteraction:
        return "fast";
    case ScreenOnTouchPolicy::Mode::ReleasedCalm:
        return "calm";
    case ScreenOnTouchPolicy::Mode::IdleIrq:
        return "idle_irq";
    case ScreenOnTouchPolicy::Mode::PollingOnly:
        return "polling_only";
    }
    return "unknown";
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

static int lvgl_port_framebuffer_index(const void *frame_buffer)
{
    if (frame_buffer == nullptr) {
        return -1;
    }
    for (int index = 0; index < RotatedFramebufferPolicy::FRAME_BUFFER_COUNT;
         ++index) {
        if (lvgl_port_driver_fb[index] == frame_buffer) {
            return index;
        }
    }
    return -1;
}

static bool lvgl_port_framebuffers_are_distinct()
{
    return lvgl_port_driver_fb[0] != nullptr &&
           lvgl_port_driver_fb[1] != nullptr &&
           lvgl_port_driver_fb[2] != nullptr &&
           lvgl_port_driver_fb[0] != lvgl_port_driver_fb[1] &&
           lvgl_port_driver_fb[0] != lvgl_port_driver_fb[2] &&
           lvgl_port_driver_fb[1] != lvgl_port_driver_fb[2];
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
    if (!lvgl_port_wait_for_vsync_after(baseline_vsync_count)) {
        return false;
    }
#if LVGL_PORT_DIRECT_MODE && (LVGL_PORT_ROTATION_DEGREE == 0) && \
    (LVGL_PORT_DISP_BUFFER_NUM >= 3)
    const int acknowledged_index = lvgl_port_framebuffer_index(frame_buffer);
    if (!RotatedFramebufferPolicy::validIndex(acknowledged_index)) {
        ++lvgl_diag_framebuffer_ownership_violation_count;
        lvgl_rotation_pipeline_active.store(false, std::memory_order_release);
        lvgl_port_latch_display_sync_fault(
            "Acknowledged framebuffer is outside the owned RGB set");
        return false;
    }
    lvgl_port_active_scanout_index = acknowledged_index;
#endif
    // Publish only a completed framebuffer hand-off, never flush entry or an
    // unrelated VSYNC. This acknowledges the driver path, not optical output.
    lvgl_presented_frame_count.fetch_add(1, std::memory_order_release);
    return true;
}

[[noreturn]] static void lvgl_port_fail_stop_display_task()
{
    // Publish a terminal ownership acknowledgement before self-suspending.
    // The task performs no shared-state mutation after this release store, so
    // recovery code may safely finalize disabled touch/backlight state without
    // acquiring the recursive mutex that this task intentionally retains.
    lvgl_display_task_fail_stopped.store(true, std::memory_order_release);
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

static void IRAM_ATTR lvgl_touch_interrupt_callback(esp_lcd_touch_handle_t touch_panel)
{
    (void)touch_panel;
    lvgl_touch_interrupt_latch.signal();
}

static inline bool lvgl_touch_take_interrupt()
{
    return lvgl_touch_interrupt_gated && lvgl_touch_interrupt_armed &&
           lvgl_touch_interrupt_latch.take();
}

static inline void lvgl_touch_clear_interrupt()
{
    lvgl_touch_interrupt_latch.clear();
}

static void lvgl_touch_request_release_gate(bool require_explicit_release)
{
    uint8_t request = LVGL_TOUCH_RELEASE_GATE_REQUEST_PENDING;
    if (require_explicit_release) {
        request |= LVGL_TOUCH_RELEASE_GATE_REQUIRE_EXPLICIT;
    }
    lvgl_touch_release_gate_request.fetch_or(request,
                                             std::memory_order_release);
}

enum class LvglTouchInterruptAttachResult : uint8_t {
    Registered,
    PollingFallback,
    UnsafeFailure,
};

static bool lvgl_touch_force_interrupt_off(esp_lcd_touch_handle_t panel,
                                           const char *operation)
{
    if (panel == nullptr || panel->config.int_gpio_num == GPIO_NUM_NC) {
        ESP_LOGW("LVGL", "%s touch IRQ cleanup has no valid panel/GPIO",
                 operation);
        return false;
    }

    const gpio_num_t interrupt_gpio = panel->config.int_gpio_num;
    const esp_err_t initial_disable = gpio_intr_disable(interrupt_gpio);

    // esp_lcd_touch mutates panel->config.interrupt_callback before its GPIO
    // operations. Always follow a failed call with explicit best-effort remove
    // and disable operations so polling fallback never leaves an old handler
    // runnable merely because the configuration pointer was already cleared.
    const esp_err_t unregister_result =
        esp_lcd_touch_register_interrupt_callback(panel, nullptr);
    if (initial_disable == ESP_OK && unregister_result == ESP_OK) {
        return true;
    }

    const esp_err_t remove_result = gpio_isr_handler_remove(interrupt_gpio);
    const esp_err_t final_disable = gpio_intr_disable(interrupt_gpio);
    const bool handler_absent =
        remove_result == ESP_OK || remove_result == ESP_ERR_INVALID_STATE;
    const bool safely_off = handler_absent && final_disable == ESP_OK;
    ESP_LOGW(
        "LVGL",
        "%s touch IRQ cleanup: disable=%s unregister=%s remove=%s final_disable=%s safe=%s",
        operation,
        esp_err_to_name(initial_disable),
        esp_err_to_name(unregister_result),
        esp_err_to_name(remove_result),
        esp_err_to_name(final_disable),
        safely_off ? "yes" : "no");
    return safely_off;
}

static bool lvgl_touch_unregister_direct_interrupt()
{
    esp_lcd_touch_handle_t panel = lvgl_touch_registered_interrupt_handle;
    if (panel == nullptr) {
        lvgl_touch_interrupt_gated = false;
        lvgl_touch_interrupt_armed = false;
        return true;
    }

    if (!lvgl_touch_force_interrupt_off(panel, "unregister direct")) {
        return false;
    }

    lvgl_touch_registered_interrupt_handle = nullptr;
    lvgl_touch_interrupt_gated = false;
    lvgl_touch_interrupt_armed = false;
    return true;
}

static bool lvgl_touch_mask_direct_interrupt()
{
    esp_lcd_touch_handle_t panel = lvgl_touch_registered_interrupt_handle;
    if (!lvgl_touch_interrupt_gated || panel == nullptr) {
        lvgl_touch_interrupt_armed = false;
        lvgl_touch_clear_interrupt();
        return true;
    }

    const gpio_num_t interrupt_gpio = panel->config.int_gpio_num;
    const esp_err_t result = gpio_intr_disable(interrupt_gpio);
    if (result == ESP_OK) {
        // gpio_intr_disable() also clears the peripheral pending bit. Clear the
        // software latch only after the physical source has stopped racing it.
        lvgl_touch_interrupt_armed = false;
        lvgl_touch_clear_interrupt();
        return true;
    }

    ESP_LOGW("LVGL", "failed to mask touch IRQ: %s; retiring direct ISR",
             esp_err_to_name(result));
    lvgl_touch_interrupt_armed = false;
    const bool safely_retired = lvgl_touch_unregister_direct_interrupt();
    lvgl_touch_clear_interrupt();
    return safely_retired;
}

static bool lvgl_touch_arm_direct_interrupt()
{
    esp_lcd_touch_handle_t panel = lvgl_touch_registered_interrupt_handle;
    if (!lvgl_touch_interrupt_gated || panel == nullptr) {
        lvgl_touch_interrupt_armed = false;
        return true;
    }

    const gpio_num_t interrupt_gpio = panel->config.int_gpio_num;
    const esp_err_t result = gpio_intr_enable(interrupt_gpio);
    if (result != ESP_OK) {
        ESP_LOGW("LVGL", "failed to arm touch IRQ: %s; using polling fallback",
                 esp_err_to_name(result));
        lvgl_touch_interrupt_armed = false;
        const bool safely_retired = lvgl_touch_unregister_direct_interrupt();
        lvgl_touch_clear_interrupt();
        return safely_retired;
    }

    lvgl_touch_interrupt_armed = true;
    // gpio_intr_enable() clears stale peripheral status. Sampling immediately
    // afterwards closes the lost-edge window for a touch already holding the
    // GT911 line active. Any concurrent ISR signal coalesces into this latch.
    if (TouchWakePolicy::interruptLineActive(
            panel->config.levels.interrupt,
            gpio_get_level(interrupt_gpio))) {
        lvgl_touch_interrupt_latch.signal();
    }
    return true;
}

static LvglTouchInterruptAttachResult
lvgl_touch_register_direct_interrupt(Touch *tp)
{
    if (tp == nullptr || tp->getPanelHandle() == nullptr) {
        return LvglTouchInterruptAttachResult::UnsafeFailure;
    }
    if (!lvgl_touch_unregister_direct_interrupt()) {
        ESP_LOGW("LVGL", "stale touch IRQ registration could not be removed");
        return LvglTouchInterruptAttachResult::UnsafeFailure;
    }

    esp_lcd_touch_handle_t panel = tp->getPanelHandle();
    // Track the physical handle before touching registration state. If any
    // cleanup fails, a later teardown/init retains the exact handle to retry.
    lvgl_touch_registered_interrupt_handle = panel;

    // The vendor Touch wrapper always gives its own FreeRTOS semaphore from
    // the physical ISR. Disable the line first, then remove that handler.
    if (!lvgl_touch_force_interrupt_off(panel, "replace vendor")) {
        return LvglTouchInterruptAttachResult::UnsafeFailure;
    }
    lvgl_touch_registered_interrupt_handle = nullptr;

    lvgl_touch_clear_interrupt();
    // Track before registration because the low-level API enables the GPIO
    // before adding its handler and mutates callback state before either step.
    lvgl_touch_registered_interrupt_handle = panel;
    const esp_err_t result = esp_lcd_touch_register_interrupt_callback(
        panel, lvgl_touch_interrupt_callback);
    if (result != ESP_OK) {
        ESP_LOGW("LVGL", "failed to register direct touch IRQ callback: %s",
                 esp_err_to_name(result));
        if (!lvgl_touch_unregister_direct_interrupt()) {
            return LvglTouchInterruptAttachResult::UnsafeFailure;
        }
        return LvglTouchInterruptAttachResult::PollingFallback;
    }

    // The library registration call enables the GPIO. Mask it immediately and
    // keep only the bounded latch ISR installed until dark-wake mode is armed.
    lvgl_touch_interrupt_gated = true;
    lvgl_touch_interrupt_armed = true;
    if (!lvgl_touch_mask_direct_interrupt()) {
        ESP_LOGE("LVGL", "direct touch IRQ could not be safely masked");
        return LvglTouchInterruptAttachResult::UnsafeFailure;
    }
    if (!lvgl_touch_interrupt_gated) {
        ESP_LOGW("LVGL", "direct touch IRQ retired during initial mask; using polling fallback");
        return LvglTouchInterruptAttachResult::PollingFallback;
    }
    ESP_LOGI("LVGL", "vendor touch ISR replaced with masked direct IRQ latch");
    return LvglTouchInterruptAttachResult::Registered;
}

static bool lvgl_touch_runtime_irq_available()
{
    return Gt911RuntimePolicy::directIrqAvailable(
        lvgl_touch_irq_config_verified.load(std::memory_order_acquire),
        lvgl_touch_irq_config_mode.load(std::memory_order_acquire),
        lvgl_touch_screen_idle_fail_safe.load(std::memory_order_acquire),
        lvgl_touch_interrupt_gated.load(std::memory_order_acquire),
        lvgl_touch_registered_interrupt_handle.load(
            std::memory_order_acquire) != nullptr);
}

static bool lvgl_touch_interrupt_line_blocks_quiet_fallback()
{
    esp_lcd_touch_handle_t panel =
        lvgl_port_touch != nullptr ? lvgl_port_touch->getPanelHandle() : nullptr;
    if (!lvgl_touch_irq_config_verified.load(std::memory_order_acquire) ||
        panel == nullptr || panel->config.int_gpio_num == GPIO_NUM_NC) {
        // Polling-only fallback may have no usable INT polarity. Its two
        // status-aware NoData samples remain the bounded release evidence for
        // non-touch blocks. Touch-wake gates never allow that fallback.
        return false;
    }
    return TouchWakePolicy::interruptLineActive(
        panel->config.levels.interrupt,
        gpio_get_level(panel->config.int_gpio_num));
}

static bool lvgl_touch_configure_runtime_interrupt(
    Touch *tp,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    if (lvgl_touch_irq_config_attempted) {
        return lvgl_touch_runtime_irq_available();
    }
    lvgl_touch_irq_config_attempted = true;
    lvgl_touch_irq_config_verified = false;
    lvgl_touch_irq_config_mode = -1;

    if (!access || tp == nullptr || tp->getPanelHandle() == nullptr ||
        tp->getPanelHandle()->io == nullptr ||
        lvgl_touch_screen_idle_fail_safe) {
        lvgl_touch_screen_idle_fail_safe = true;
        (void)lvgl_touch_unregister_direct_interrupt();
        return false;
    }
    if (!tp->isInterruptEnabled()) {
        (void)lvgl_touch_unregister_direct_interrupt();
        return false;
    }

    esp_lcd_touch_handle_t panel = tp->getPanelHandle();
    uint8_t module_switch_1 = 0;
    esp_err_t read_result = esp_lcd_panel_io_rx_param(
        panel->io,
        Gt911RuntimePolicy::MODULE_SWITCH_1_REGISTER,
        &module_switch_1,
        1);
    if (read_result != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_READ_RETRY_DELAY_MS));
        read_result = esp_lcd_panel_io_rx_param(
            panel->io,
            Gt911RuntimePolicy::MODULE_SWITCH_1_REGISTER,
            &module_switch_1,
            1);
    }
    if (read_result != ESP_OK) {
        ESP_LOGW("LVGL",
                 "GT911 interrupt mode read failed: %s; using 40 ms polling",
                 esp_err_to_name(read_result));
        lvgl_touch_screen_idle_fail_safe = true;
        (void)lvgl_touch_unregister_direct_interrupt();
        return false;
    }

    const Gt911RuntimePolicy::InterruptConfig config =
        Gt911RuntimePolicy::decodeInterruptConfig(module_switch_1);
    lvgl_touch_irq_config_verified = true;
    lvgl_touch_irq_config_mode = static_cast<int8_t>(config.mode);
    panel->config.levels.interrupt = config.active_high;

    if (!config.direct_irq_supported) {
        ESP_LOGI("LVGL",
                 "GT911 interrupt mode=%u is level-triggered; using 40 ms polling",
                 static_cast<unsigned>(lvgl_touch_irq_config_mode));
        if (!lvgl_touch_unregister_direct_interrupt()) {
            lvgl_touch_screen_idle_fail_safe = true;
        }
        return false;
    }

    if (!lvgl_touch_mask_direct_interrupt()) {
        lvgl_touch_screen_idle_fail_safe = true;
        return false;
    }
    if (!lvgl_touch_interrupt_gated) {
        const LvglTouchInterruptAttachResult attach_result =
            lvgl_touch_register_direct_interrupt(tp);
        if (attach_result != LvglTouchInterruptAttachResult::Registered) {
            lvgl_touch_screen_idle_fail_safe = true;
            return false;
        }
    }

    const gpio_int_type_t edge_type =
        config.positive_edge ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
    const esp_err_t edge_result =
        gpio_set_intr_type(panel->config.int_gpio_num, edge_type);
    if (edge_result != ESP_OK) {
        ESP_LOGW("LVGL",
                 "GT911 edge mode apply failed: %s; using 40 ms polling",
                 esp_err_to_name(edge_result));
        lvgl_touch_screen_idle_fail_safe = true;
        (void)lvgl_touch_unregister_direct_interrupt();
        return false;
    }

    ESP_LOGI("LVGL",
             "GT911 runtime IRQ verified: mode=%u edge=%s",
             static_cast<unsigned>(lvgl_touch_irq_config_mode),
             config.positive_edge ? "rising" : "falling");
    return true;
}

static void lvgl_touch_reset_screen_on_policy(uint32_t now_ms)
{
    if (lvgl_touch_current_mode() == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
        lvgl_touch_interrupt_armed &&
        !lvgl_touch_mask_direct_interrupt()) {
        lvgl_touch_screen_idle_fail_safe = true;
    }
    lvgl_touch_screen_on_policy.reset(
        now_ms,
        lvgl_touch_current_mode() == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
            lvgl_touch_runtime_irq_available());
    lvgl_touch_publish_screen_policy_mode();
}

static void lvgl_touch_latch_screen_idle_fail_safe()
{
    lvgl_touch_screen_idle_fail_safe = true;
    (void)lvgl_touch_mask_direct_interrupt();
    (void)lvgl_touch_unregister_direct_interrupt();
    lvgl_touch_screen_on_policy.usePollingOnly();
    lvgl_touch_publish_screen_policy_mode();
}

static void lvgl_port_copy_frame_180(const uint16_t *src, uint16_t *dst, uint32_t width, uint32_t height)
{
    if (src == nullptr || dst == nullptr) {
        return;
    }

    const uint32_t pixel_count = width * height;
    const uint16_t *from = src;
    uint16_t *to = dst + pixel_count - 1;
    for (uint32_t i = 0; i < pixel_count; ++i) {
        *to-- = *from++;
    }
}

static void lvgl_port_apply_screen_flip_to_touch_point(TouchPoint &point)
{
    if (!lvgl_port_screen_flip_180.load(std::memory_order_acquire)) {
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    const int hor_res = disp != nullptr ? lv_display_get_horizontal_resolution(disp) : 800;
    const int ver_res = disp != nullptr ? lv_display_get_vertical_resolution(disp) : 480;
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
#if LVGL_PORT_AVOID_TEAR && LVGL_PORT_DIRECT_MODE && \
    (LVGL_PORT_ROTATION_DEGREE == 0) && (LVGL_PORT_DISP_BUFFER_NUM >= 3)
    if (enabled == lvgl_port_screen_flip_180.load(std::memory_order_acquire)) {
        return !enabled ||
               (lvgl_rotation_pipeline_active.load(
                    std::memory_order_acquire) &&
                RotatedFramebufferPolicy::ownsActive(
                    lvgl_port_flip_layout,
                    lvgl_port_active_scanout_index));
    }

    lv_display_t *disp = lv_display_get_default();
    lv_draw_buf_t *draw_buf = disp ? lv_display_get_buf_active(disp) : nullptr;
    if (draw_buf == nullptr || lvgl_flush_in_progress ||
        !lvgl_port_framebuffers_are_distinct() ||
        !RotatedFramebufferPolicy::validIndex(
            lvgl_port_active_scanout_index)) {
        return false;
    }

    if (enabled) {
        const int preferred_renderer =
            lvgl_port_framebuffer_index(draw_buf->data);
        const RotatedFramebufferPolicy::FlipLayout layout =
            RotatedFramebufferPolicy::makeFlipLayout(
                lvgl_port_active_scanout_index, preferred_renderer);
        if (!RotatedFramebufferPolicy::valid(layout)) {
            return false;
        }

        // While flipped, LVGL owns exactly one logical renderer. The other
        // two buffers are output-only and alternate around the acknowledged
        // active scanout, so the CPU never writes the buffer being scanned.
        lv_display_set_buffers(disp, lvgl_port_driver_fb[layout.renderer], nullptr,
            lv_display_get_horizontal_resolution(disp) * lv_display_get_vertical_resolution(disp) * sizeof(uint16_t),
            LV_DISPLAY_RENDER_MODE_DIRECT);
        lvgl_port_flip_layout = layout;
        lvgl_rotation_pipeline_active.store(true, std::memory_order_release);
    } else {
        const RotatedFramebufferPolicy::NormalLayout layout =
            RotatedFramebufferPolicy::makeNormalLayout(
                lvgl_port_flip_layout, lvgl_port_active_scanout_index);
        if (!RotatedFramebufferPolicy::valid(layout)) {
            return false;
        }

        // Restore LVGL double buffering with the acknowledged scanout as the
        // on-screen member and the former logical renderer as the safe first
        // draw target. Full invalidation below repaints it unrotated.
        // LVGL 9 starts rendering into the first supplied buffer.
        lv_display_set_buffers(disp, lvgl_port_driver_fb[layout.renderer], lvgl_port_driver_fb[layout.on_screen],
            lv_display_get_horizontal_resolution(disp) * lv_display_get_vertical_resolution(disp) * sizeof(uint16_t),
            LV_DISPLAY_RENDER_MODE_DIRECT);
        lvgl_port_flip_layout = {};
        lvgl_rotation_pipeline_active.store(false, std::memory_order_release);
    }
#else
    if (enabled) {
        ESP_LOGE("LVGL",
                 "screen 180 flip requires avoid-tear direct mode with three framebuffers");
        return false;
    }
#endif

    const bool require_explicit_release =
        lvgl_touch_pressed.load(std::memory_order_acquire);
    lvgl_port_screen_flip_180.store(enabled, std::memory_order_release);
    const uint32_t now_ms = get_monotonic_ms();
    lvgl_touch_reset_screen_on_policy(now_ms);
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_request_release_gate(require_explicit_release);
    lvgl_touch_wait_release_after_block.store(true, std::memory_order_release);
    lvgl_touch_read_block_until_ms.store(now_ms + 250,
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

    ++lvgl_diag_touch_full_read_count;
    int result = tp->readPoints(point, 1, 0);
    if (result >= 0) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_READ_RETRY_DELAY_MS));
    ++lvgl_diag_touch_full_read_count;
    return tp->readPoints(point, 1, 0);
}

enum class LvglTouchFrameKind : uint8_t {
    NoData = 0,
    Released,
    Pressed,
    Error,
};

struct LvglTouchFrameSample {
    LvglTouchFrameKind kind = LvglTouchFrameKind::NoData;
    TouchPoint point{};
    bool preserve_cache_on_error = false;

    LvglTouchFrameSample(LvglTouchFrameKind sample_kind,
                         const TouchPoint &sample_point,
                         bool preserve_cache)
        : kind(sample_kind),
          point(sample_point),
          preserve_cache_on_error(preserve_cache) {}
};

static esp_err_t lvgl_touch_read_coordinate_status(
    Touch *tp,
    uint8_t *status,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    if (!access || tp == nullptr || status == nullptr ||
        tp->getPanelHandle() == nullptr || tp->getPanelHandle()->io == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_lcd_touch_handle_t panel = tp->getPanelHandle();
    ++lvgl_diag_touch_status_read_count;
    esp_err_t result = esp_lcd_panel_io_rx_param(
        panel->io,
        Gt911RuntimePolicy::COORDINATE_STATUS_REGISTER,
        status,
        1);
    if (result == ESP_OK) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_READ_RETRY_DELAY_MS));
    ++lvgl_diag_touch_status_read_count;
    return esp_lcd_panel_io_rx_param(
        panel->io,
        Gt911RuntimePolicy::COORDINATE_STATUS_REGISTER,
        status,
        1);
}

static LvglTouchFrameSample lvgl_touch_read_frame(
    Touch *tp,
    const SharedI2cRuntimeGate::Gate::Access &access)
{
    uint8_t raw_status = 0;
    if (lvgl_touch_read_coordinate_status(tp, &raw_status, access) != ESP_OK) {
        return {LvglTouchFrameKind::Error, {}, false};
    }

    TouchPoint point{};
    int vendor_result = 0;
    if (Gt911RuntimePolicy::vendorReadAction(raw_status) !=
        Gt911RuntimePolicy::VendorReadAction::Skip) {
        vendor_result = lvgl_touch_read_points_with_retry(tp, &point, access);
    }

    const Gt911RuntimePolicy::FrameReconciliation reconciled =
        Gt911RuntimePolicy::reconcileFrame(raw_status, vendor_result);
    switch (reconciled.kind) {
    case Gt911RuntimePolicy::ReconciledFrameKind::NoData:
        return {LvglTouchFrameKind::NoData, {}, false};
    case Gt911RuntimePolicy::ReconciledFrameKind::Released:
        return {LvglTouchFrameKind::Released, {}, false};
    case Gt911RuntimePolicy::ReconciledFrameKind::Pressed:
        return {LvglTouchFrameKind::Pressed, point, false};
    case Gt911RuntimePolicy::ReconciledFrameKind::Error:
        return {LvglTouchFrameKind::Error,
                {},
                reconciled.preserve_cache_on_error};
    }
    return {LvglTouchFrameKind::Error, {}, false};
}

static ScreenOnTouchPolicy::Sample lvgl_touch_policy_sample(
    LvglTouchFrameKind kind)
{
    switch (kind) {
    case LvglTouchFrameKind::NoData:
        return ScreenOnTouchPolicy::Sample::NoData;
    case LvglTouchFrameKind::Released:
        return ScreenOnTouchPolicy::Sample::Released;
    case LvglTouchFrameKind::Pressed:
        return ScreenOnTouchPolicy::Sample::Pressed;
    case LvglTouchFrameKind::Error:
        return ScreenOnTouchPolicy::Sample::Error;
    }
    return ScreenOnTouchPolicy::Sample::Error;
}

static TouchReleaseGatePolicy::ProbeResult lvgl_touch_release_probe_sample(
    LvglTouchFrameKind kind)
{
    switch (kind) {
    case LvglTouchFrameKind::NoData:
        return TouchReleaseGatePolicy::ProbeResult::NoData;
    case LvglTouchFrameKind::Released:
        return TouchReleaseGatePolicy::ProbeResult::Released;
    case LvglTouchFrameKind::Pressed:
        return TouchReleaseGatePolicy::ProbeResult::Pressed;
    case LvglTouchFrameKind::Error:
        return TouchReleaseGatePolicy::ProbeResult::Error;
    }
    return TouchReleaseGatePolicy::ProbeResult::Error;
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

    const bool require_explicit_release =
        lvgl_touch_pressed.load(std::memory_order_acquire);

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

    lvgl_touch_reset_screen_on_policy(now_ms);
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
    lvgl_touch_request_release_gate(require_explicit_release);
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
    const SharedI2cRuntimeGate::Gate::Access &access,
    bool preserve_cached_state = false)
{
    ++lvgl_diag_touch_read_error_count;
    lvgl_touch_reset_screen_on_policy(now_ms);
    if (!preserve_cached_state) {
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    }
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

static void lvgl_log_print_cb(lv_log_level_t, const char *buf)
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

static void flush_callback(lv_display_t *drv, const lv_area_t *, uint8_t *color_map)
{
    lvgl_flush_in_progress = true;
    lvgl_diag_mark_flush();
    LCD *lcd = static_cast<LCD *>(lv_display_get_user_data(drv));

    /* Action after last area refresh */
    if (lv_display_flush_is_last(drv)) {
        void *next_frame_buffer = color_map;
        if (lvgl_port_screen_flip_180.load(std::memory_order_acquire)) {
            const int color_map_index =
                lvgl_port_framebuffer_index(color_map);
            const int output_index =
                RotatedFramebufferPolicy::selectInactiveOutput(
                    lvgl_port_flip_layout,
                    lvgl_port_active_scanout_index);
            if (color_map_index != lvgl_port_flip_layout.renderer ||
                !RotatedFramebufferPolicy::validIndex(output_index) ||
                output_index == lvgl_port_active_scanout_index ||
                output_index == color_map_index) {
                ++lvgl_diag_framebuffer_ownership_violation_count;
                lvgl_rotation_pipeline_active.store(false,
                                                    std::memory_order_release);
                lvgl_port_latch_display_sync_fault(
                    "Rotated framebuffer ownership violation");
                lvgl_port_fail_stop_display_task();
            }
            next_frame_buffer = lvgl_port_driver_fb[output_index];
            lvgl_port_copy_frame_180(
                reinterpret_cast<const uint16_t *>(color_map),
                static_cast<uint16_t *>(next_frame_buffer),
                lv_display_get_horizontal_resolution(drv),
                lv_display_get_vertical_resolution(drv)
            );
        }

        if (!lvgl_port_switch_and_confirm_vsync(lcd, next_frame_buffer)) {
            lvgl_port_fail_stop_display_task();
        }
        if (lvgl_port_screen_flip_180.load(std::memory_order_acquire)) {
            ++lvgl_diag_rotated_copy_switch_count;
        }
    }

    lv_display_flush_ready(drv);
    lvgl_flush_in_progress = false;
}

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
    const uint32_t callback_ms = get_rtos_ms_isr();
    const uint32_t previous_callback_ms = lvgl_diag_vsync_last_ms;
    if (previous_callback_ms != 0) {
        const uint32_t gap_ms = callback_ms - previous_callback_ms;
        if (gap_ms > lvgl_diag_refresh_callback_max_gap_ms) {
            lvgl_diag_refresh_callback_max_gap_ms = gap_ms;
        }
    }
    lvgl_diag_vsync_last_ms = callback_ms;
    ++lvgl_diag_vsync_count;

    // Notify that the current LCD frame buffer has been transmitted
    xTaskNotifyFromISR(task_handle, 0, eNoAction, &need_yield);

    return (need_yield == pdTRUE);
}


static lv_display_t *display_init(LCD *lcd)
{
    static_assert(LVGL_VERSION_MAJOR == 9 && LV_COLOR_DEPTH == 16,
                  "Native Aura display requires LVGL 9 and RGB565");
    static_assert(LVGL_PORT_DIRECT_MODE && LVGL_PORT_DISP_BUFFER_NUM == 3 && LVGL_PORT_ROTATION_DEGREE == 0,
                  "Native Aura display uses three RGB buffers with runtime 180-degree flip");
    if (!lcd || !lcd->getRefreshPanelHandle()) return nullptr;
    const int width = lcd->getFrameWidth(), height = lcd->getFrameHeight();
    for (int index = 0; index < RotatedFramebufferPolicy::FRAME_BUFFER_COUNT; ++index)
        lvgl_port_driver_fb[index] = lcd->getFrameBufferByIndex(index);
    if (!lvgl_port_framebuffers_are_distinct()) return nullptr;
    const auto layout = RotatedFramebufferPolicy::makeInitialLayout(0);
    if (!RotatedFramebufferPolicy::valid(layout)) return nullptr;
    lvgl_buf[0] = lvgl_port_driver_fb[layout.renderer_a];
    lvgl_buf[1] = lvgl_port_driver_fb[layout.renderer_b];
    lvgl_port_active_scanout_index = layout.scanout;
    lvgl_port_flip_layout = {};
    lv_display_t *display = lv_display_create(width, height);
    if (!display) return nullptr;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, lvgl_buf[0], lvgl_buf[1], width * height * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_user_data(display, lcd);
    lv_display_set_flush_cb(display, flush_callback);
    return display;
}

static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    Touch *tp = static_cast<Touch *>(lv_indev_get_user_data(indev_drv));
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

    const lvgl_port_touch_mode_t touch_mode = lvgl_touch_current_mode();
    if (touch_mode == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
        lvgl_touch_screen_reset_requested.exchange(
            false, std::memory_order_acq_rel)) {
        lvgl_touch_reset_screen_on_policy(now_ms);
    }
    if (touch_mode == LVGL_PORT_TOUCH_MODE_SUPPRESSED ||
        touch_mode == LVGL_PORT_TOUCH_MODE_DISABLED) {
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

    if (touch_mode == LVGL_PORT_TOUCH_MODE_DARK_WAKE &&
        lvgl_touch_wake_policy_enabled()) {
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
        const bool fast_retry_pending =
            lvgl_touch_wake_policy_take_fast_retry();
        const bool urgent_probe = interrupt_pending || fast_retry_pending;
        // Prefer a fresh GT911 interrupt. A sparse fallback probe prevents a
        // lost interrupt from making the dark screen impossible to wake while
        // avoiding the continuous I2C traffic which caused the original fault.
        if (!lvgl_touch_wake_policy_should_probe(
                lvgl_touch_interrupt_gated && lvgl_touch_interrupt_armed,
                urgent_probe, now_ms)) {
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
            data->state = lvgl_touch_cached_state;
            return;
        }

        LvglTouchFrameSample wake_probe = lvgl_touch_read_frame(tp, access);
        if (interrupt_pending &&
            wake_probe.kind == LvglTouchFrameKind::NoData) {
            // GT911 may assert INT just before publishing the ready bit. One
            // bounded retry avoids turning that gap into a synthetic release.
            vTaskDelay(pdMS_TO_TICKS(LVGL_TOUCH_READ_RETRY_DELAY_MS));
            wake_probe = lvgl_touch_read_frame(tp, access);
        }
        if (wake_probe.kind == LvglTouchFrameKind::Pressed) {
            lvgl_touch_wake_policy_record(
                TouchWakePolicy::Sample::Pressed, now_ms, interrupt_pending);
            lvgl_touch_note_success();
        } else if (wake_probe.kind == LvglTouchFrameKind::Released) {
            lvgl_touch_wake_policy_record(
                TouchWakePolicy::Sample::Released, now_ms, interrupt_pending);
            lvgl_touch_note_success();
        } else if (wake_probe.kind == LvglTouchFrameKind::NoData) {
            lvgl_touch_wake_policy_record(
                TouchWakePolicy::Sample::NoData, now_ms, interrupt_pending);
            lvgl_touch_note_success();
            if (interrupt_pending) {
                ++lvgl_diag_touch_irq_no_frame_count;
            }
        } else {
            lvgl_touch_wake_policy_record(
                TouchWakePolicy::Sample::Error, now_ms, interrupt_pending);
            lvgl_touch_note_error(
                tp,
                now_ms,
                "wake_probe",
                access,
                wake_probe.preserve_cache_on_error);
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if (touch_mode != LVGL_PORT_TOUCH_MODE_SCREEN_ON) {
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

    if (!lvgl_touch_irq_config_attempted) {
        const bool irq_available =
            lvgl_touch_configure_runtime_interrupt(tp, access);
        lvgl_touch_screen_on_policy.reset(now_ms, irq_available);
        lvgl_touch_publish_screen_policy_mode();
    }

    // After a block, suppress input until either an explicit GT911 release is
    // observed or a non-touch-wake transition has two spaced, status-aware
    // quiet samples. A dark-screen touch wake always requires the real release.
    if (lvgl_touch_wait_release_after_block.load(std::memory_order_acquire)) {
        const uint8_t gate_request =
            lvgl_touch_release_gate_request.exchange(
                0, std::memory_order_acq_rel);
        if ((gate_request & LVGL_TOUCH_RELEASE_GATE_REQUEST_PENDING) != 0) {
            const bool require_explicit =
                (gate_request &
                 LVGL_TOUCH_RELEASE_GATE_REQUIRE_EXPLICIT) != 0;
            lvgl_touch_release_gate.begin(
                !require_explicit,
                lvgl_touch_interrupt_line_blocks_quiet_fallback());
        }
        if ((lvgl_touch_last_sample_ms != 0) &&
            is_before_deadline(
                now_ms,
                lvgl_touch_last_sample_ms + LVGL_TOUCH_POLL_INTERVAL_MS)) {
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
            data->state = lvgl_touch_cached_state;
            return;
        }
        lvgl_touch_last_sample_ms = now_ms;
        const LvglTouchFrameSample release_probe =
            lvgl_touch_read_frame(tp, access);
        const TouchReleaseGatePolicy::ProbeResult probe_result =
            lvgl_touch_release_probe_sample(release_probe.kind);
        const TouchReleaseGatePolicy::Decision release_decision =
            lvgl_touch_release_gate.observe(
                probe_result,
                lvgl_touch_interrupt_line_blocks_quiet_fallback(),
                now_ms);
        if (probe_result == TouchReleaseGatePolicy::ProbeResult::Error) {
            lvgl_touch_note_error(
                tp,
                now_ms,
                "release_probe",
                access,
                release_probe.preserve_cache_on_error);
        } else {
            lvgl_touch_note_success();
        }
        const bool keep_waiting =
            release_decision == TouchReleaseGatePolicy::Decision::Hold;
        // Store after the error/recovery path because that path also updates
        // the block and release-gate state.
        lvgl_touch_wait_release_after_block.store(keep_waiting,
                                                   std::memory_order_release);
        if (!keep_waiting) {
            lvgl_touch_screen_on_policy.reset(
                now_ms, lvgl_touch_runtime_irq_available());
            (void)lvgl_touch_screen_on_policy.recordRead(
                ScreenOnTouchPolicy::Action::ReadFast,
                ScreenOnTouchPolicy::Sample::Released,
                now_ms);
            lvgl_touch_publish_screen_policy_mode();
        }
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        data->state = lvgl_touch_cached_state;
        return;
    }

    if (lvgl_touch_screen_on_policy.mode() ==
            ScreenOnTouchPolicy::Mode::IdleIrq &&
        (!lvgl_touch_interrupt_armed ||
         !lvgl_touch_runtime_irq_available())) {
        ++lvgl_diag_touch_irq_arm_failure_count;
        lvgl_touch_latch_screen_idle_fail_safe();
    }

    const bool interrupt_pending =
        lvgl_touch_screen_on_policy.mode() ==
                ScreenOnTouchPolicy::Mode::IdleIrq
            ? lvgl_touch_take_interrupt()
            : false;
    const ScreenOnTouchPolicy::Decision decision =
        lvgl_touch_screen_on_policy.decide(now_ms, interrupt_pending);

    if (decision.action == ScreenOnTouchPolicy::Action::RequestIdleIrq) {
        // Close the interval since the last calm poll before exposing the
        // interrupt-only window. A completed short tap remains queued in
        // 0x814E even when its physical edge happened while IRQ was masked.
        const LvglTouchFrameSample boundary_sample =
            lvgl_touch_read_frame(tp, access);
        const ScreenOnTouchPolicy::Sample boundary_policy_sample =
            lvgl_touch_policy_sample(boundary_sample.kind);
        const bool boundary_allows_arm =
            lvgl_touch_screen_on_policy.recordIdleBoundarySample(
                boundary_policy_sample, now_ms);
        lvgl_touch_publish_screen_policy_mode();

        if (boundary_sample.kind == LvglTouchFrameKind::Pressed) {
            lvgl_touch_note_success();
            TouchPoint adjusted_point = boundary_sample.point;
            lvgl_port_apply_screen_flip_to_touch_point(adjusted_point);
            lvgl_touch_cached_point = adjusted_point;
            lvgl_touch_set_cached_state(LV_INDEV_STATE_PRESSED);
            lvgl_touch_fill_from_cache(data);
            return;
        }
        if (boundary_sample.kind == LvglTouchFrameKind::Released) {
            lvgl_touch_note_success();
            lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
        } else if (boundary_sample.kind == LvglTouchFrameKind::NoData) {
            lvgl_touch_note_success();
        } else {
            lvgl_touch_note_error(
                tp,
                now_ms,
                "idle_boundary",
                access,
                boundary_sample.preserve_cache_on_error);
            lvgl_touch_fill_from_cache(data);
            return;
        }

        if (!boundary_allows_arm) {
            lvgl_touch_fill_from_cache(data);
            return;
        }
        lvgl_touch_clear_interrupt();
        const bool arm_call_succeeded = lvgl_touch_arm_direct_interrupt();
        const bool armed = arm_call_succeeded &&
                           lvgl_touch_interrupt_armed &&
                           lvgl_touch_runtime_irq_available();
        lvgl_touch_screen_on_policy.recordIdleIrqArm(armed, now_ms);
        lvgl_touch_publish_screen_policy_mode();
        if (armed) {
            ++lvgl_diag_touch_idle_entry_count;
        } else {
            ++lvgl_diag_touch_irq_arm_failure_count;
            lvgl_touch_latch_screen_idle_fail_safe();
        }
        lvgl_touch_fill_from_cache(data);
        return;
    }

    if (!ScreenOnTouchPolicy::isReadAction(decision.action)) {
        ++lvgl_diag_touch_idle_skip_count;
        lvgl_touch_fill_from_cache(data);
        return;
    }

    bool idle_irq_mask_failed = false;
    if (decision.action == ScreenOnTouchPolicy::Action::ReadIdleIrq) {
        ++lvgl_diag_touch_idle_irq_exit_count;
        // `interrupt_pending` was consumed above. Masking now cannot erase the
        // event which selected this read and prevents another ISR during I2C.
        idle_irq_mask_failed = !lvgl_touch_mask_direct_interrupt();
    } else if (decision.action ==
               ScreenOnTouchPolicy::Action::ReadIdleFallback) {
        ++lvgl_diag_touch_idle_fallback_probe_count;
    }

    const LvglTouchFrameSample sample = lvgl_touch_read_frame(tp, access);
    if (decision.action == ScreenOnTouchPolicy::Action::ReadIdleIrq &&
        sample.kind == LvglTouchFrameKind::NoData) {
        // GT911 may assert INT shortly before the ready bit. Returning to the
        // 40 ms path preserves a bounded retry without inventing a release.
        ++lvgl_diag_touch_irq_no_frame_count;
    }

    const ScreenOnTouchPolicy::Sample policy_sample =
        lvgl_touch_policy_sample(sample.kind);
    bool fallback_irq_observed_after_read = false;
    if (decision.action == ScreenOnTouchPolicy::Action::ReadIdleFallback &&
        sample.kind == LvglTouchFrameKind::Pressed) {
        fallback_irq_observed_after_read = lvgl_touch_take_interrupt();
        if (fallback_irq_observed_after_read) {
            ++lvgl_diag_touch_idle_irq_exit_count;
            idle_irq_mask_failed = !lvgl_touch_mask_direct_interrupt();
        }
    }
    const ScreenOnTouchPolicy::Action recorded_source =
        ScreenOnTouchPolicy::reconcileIdleReadSource(
            decision.action,
            policy_sample,
            fallback_irq_observed_after_read);
    const ScreenOnTouchPolicy::ReadEffect effect =
        lvgl_touch_screen_on_policy.recordRead(
            recorded_source,
            policy_sample,
            now_ms);
    lvgl_touch_publish_screen_policy_mode();
    if (effect.disarm_idle_irq &&
        recorded_source != ScreenOnTouchPolicy::Action::ReadIdleIrq &&
        !lvgl_touch_mask_direct_interrupt()) {
        idle_irq_mask_failed = true;
    }
    if (effect.missed_idle_irq) {
        ++lvgl_diag_touch_idle_missed_irq_press_count;
    }
    if (idle_irq_mask_failed || effect.missed_idle_irq) {
        lvgl_touch_latch_screen_idle_fail_safe();
    }

    if (sample.kind == LvglTouchFrameKind::Pressed) {
        lvgl_touch_note_success();
        TouchPoint adjusted_point = sample.point;
        lvgl_port_apply_screen_flip_to_touch_point(adjusted_point);
        lvgl_touch_cached_point = adjusted_point;
        lvgl_touch_set_cached_state(LV_INDEV_STATE_PRESSED);
    } else if (sample.kind == LvglTouchFrameKind::Released) {
        lvgl_touch_note_success();
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    } else if (sample.kind == LvglTouchFrameKind::NoData) {
        lvgl_touch_note_success();
    } else {
        const char *stage =
            recorded_source == ScreenOnTouchPolicy::Action::ReadIdleIrq
                ? "idle_irq"
                : (decision.action ==
                           ScreenOnTouchPolicy::Action::ReadIdleFallback
                       ? "idle_fallback"
                       : "read");
        lvgl_touch_note_error(
            tp, now_ms, stage, access, sample.preserve_cache_on_error);
    }
    lvgl_touch_fill_from_cache(data);
}

static lv_indev_t *indev_init(Touch *tp)
{
    ESP_UTILS_CHECK_FALSE_RETURN(tp != nullptr, nullptr, "Invalid touch device");
    ESP_UTILS_CHECK_FALSE_RETURN(tp->getPanelHandle() != nullptr, nullptr, "Touch device is not initialized");


    if (tp->isInterruptEnabled()) {
        const LvglTouchInterruptAttachResult attach_result =
            lvgl_touch_register_direct_interrupt(tp);
        if (attach_result == LvglTouchInterruptAttachResult::Registered) {
            // Registered but masked until a verified edge mode owns the line.
        } else if (attach_result ==
                   LvglTouchInterruptAttachResult::PollingFallback) {
            lvgl_touch_screen_idle_fail_safe = true;
            ESP_LOGW("LVGL", "touch interrupt gate unavailable; wake probe will use polling fallback");
        } else {
            ESP_LOGE("LVGL", "touch IRQ replacement failed without confirmed cleanup");
            return nullptr;
        }
    }

    ESP_UTILS_LOGD("Register input driver to LVGL");
    lv_indev_t *indev = lv_indev_create();
    if (!indev) return nullptr;
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
    lv_indev_set_user_data(indev, tp);
    lv_indev_set_display(indev, lv_display_get_default());
    return indev;
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

static bool lvgl_port_cleanup_partial_init(lv_display_t *disp, lv_indev_t *indev)
{
    // This helper is used only while no LVGL task remains. Resources can be
    // released without suspending another task or taking the LVGL mutex.
    bool success = true;
    lvgl_vsync_notify_enabled = false;

    if (lvgl_port_lcd != nullptr) {
        lvgl_port_lcd->attachDrawBitmapFinishCallback(nullptr, nullptr);
    }
    if (!lvgl_touch_unregister_direct_interrupt()) {
        success = false;
    }
    lvgl_touch_clear_interrupt();

#if !LV_TICK_CUSTOM
    if (!tick_deinit()) {
        success = false;
    }
#endif

    if (indev != nullptr) {
        lv_indev_delete(indev);
    }
    if (disp != nullptr) {
        lv_display_delete(disp);
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
    lvgl_display_task_fail_stopped.store(false, std::memory_order_release);
    lvgl_port_lcd = nullptr;
    lvgl_diagnostics_available.store(false, std::memory_order_release);
    lvgl_port_touch = nullptr;
    for (int index = 0;
         index < RotatedFramebufferPolicy::FRAME_BUFFER_COUNT;
         ++index) {
        lvgl_port_driver_fb[index] = nullptr;
    }
    lvgl_port_active_scanout_index = -1;
    lvgl_port_flip_layout = {};
    lvgl_port_screen_flip_180.store(false, std::memory_order_release);
    lvgl_rotation_pipeline_active.store(false, std::memory_order_release);
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_wake_policy_reset();
    lvgl_touch_screen_on_policy.reset(0, false);
    lvgl_touch_publish_screen_policy_mode();
    lvgl_touch_mode.store(LVGL_PORT_TOUCH_MODE_DISABLED,
                          std::memory_order_release);
    lvgl_touch_screen_reset_requested.store(false,
                                             std::memory_order_release);
    lvgl_touch_release_gate_request.store(0, std::memory_order_release);
    lvgl_touch_irq_config_attempted = false;
    lvgl_touch_irq_config_verified = false;
    lvgl_touch_irq_config_mode = -1;
    lvgl_touch_screen_idle_fail_safe = false;
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
    uint32_t stack_sample_ms = 0;
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
            // A task may have passed the loop-top pause check and then waited
            // behind UiController on the LVGL mutex. Recheck after acquiring
            // the serialization boundary so a late waiter cannot run one more
            // handler after headless recovery has drained and disabled its I2C
            // entry points.
            if (lvgl_pause_requested.load(std::memory_order_acquire)) {
                lvgl_port_unlock();
                task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
                continue;
            }
            const uint32_t handler_now_ms = get_monotonic_ms();
            WakePowerGuard::Activity handler_activity =
                WakePowerGuard::tryAcquireActivity(handler_now_ms);
            const bool post_wake_render =
                WakePowerGuard::phase(handler_now_ms) ==
                WakePowerGuard::Phase::RenderWait;
            // Check after acquiring the same mutex used by UiController. This
            // closes the race where a handler was already waiting on the lock
            // when the guarded wake entered Switching or Settle. RenderWait
            // deliberately permits exactly the first post-settle handler while
            // background network admission remains closed. A normal handler
            // holds an Activity lease so a wake requested by one of its LVGL
            // callbacks receives a full quiet interval after the callback and
            // mutex release.
            if (handler_activity || post_wake_render) {
                lvgl_diag_mark_timer_handler();
                task_delay_ms = lv_timer_handler();
            } else {
                task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
            }
            lvgl_port_unlock();
        }
        const uint32_t now_ms = get_rtos_ms();
        if (stack_sample_ms == 0 || now_ms - stack_sample_ms >= 1000) {
            // IDF reports the task's lifetime minimum in bytes. Sample from
            // this task after rendering, outside the UI lock; HTTP only reads
            // the cached value and never walks a live task's stack.
            lvgl_diag_stack_min_free_bytes.store(
                uxTaskGetStackHighWaterMark(nullptr), std::memory_order_relaxed);
            stack_sample_ms = now_ms;
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
    lv_display_t *drv = (lv_display_t *)user_data;

    lv_display_flush_ready(drv);

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
        lvgl_touch_unregister_direct_interrupt(),
        false,
        "Stale touch IRQ registration could not be removed");

    ESP_UTILS_CHECK_FALSE_RETURN(
        lvgl_touch_i2c_runtime_gate.resetForBoot(),
        false,
        "Touch I2C gate still has active users");
    lvgl_display_sync_fault.store(false, std::memory_order_release);
    lvgl_display_task_fail_stopped.store(false, std::memory_order_release);
    lvgl_diag_vsync_count = 0;
    lvgl_diag_vsync_last_ms = 0;
    lvgl_diag_vsync_wait_timeout_count = 0;
    lvgl_diag_refresh_callback_max_gap_ms = 0;
    lvgl_diag_rotated_copy_switch_count = 0;
    lvgl_diag_framebuffer_ownership_violation_count = 0;
    lvgl_diag_touch_read_error_count = 0;
    lvgl_diag_touch_status_read_count = 0;
    lvgl_diag_touch_full_read_count = 0;
    lvgl_diag_touch_idle_skip_count = 0;
    lvgl_diag_touch_idle_entry_count = 0;
    lvgl_diag_touch_idle_irq_exit_count = 0;
    lvgl_diag_touch_idle_fallback_probe_count = 0;
    lvgl_diag_touch_idle_missed_irq_press_count = 0;
    lvgl_diag_touch_irq_arm_failure_count = 0;
    lvgl_diag_touch_irq_no_frame_count = 0;
    lvgl_presented_frame_count.store(0, std::memory_order_release);
    lvgl_refresh_callback_semantics = LVGL_PORT_REFRESH_CALLBACK_UNKNOWN;
    lvgl_vsync_notify_enabled = false;
    lvgl_task_handle = nullptr;
    lvgl_mux = nullptr;
    lvgl_pause_requested.store(false, std::memory_order_release);
    lvgl_port_paused.store(false, std::memory_order_release);
    lvgl_port_lcd = nullptr;
    lvgl_diagnostics_available.store(false, std::memory_order_release);
    lvgl_port_touch = nullptr;
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_wake_policy_reset();
    lvgl_touch_screen_on_policy.reset(0, false);
    lvgl_touch_publish_screen_policy_mode();
    lvgl_touch_mode.store(LVGL_PORT_TOUCH_MODE_SUPPRESSED,
                          std::memory_order_release);
    lvgl_touch_screen_reset_requested.store(false,
                                             std::memory_order_release);
    lvgl_touch_release_gate_request.store(0, std::memory_order_release);
    lvgl_touch_irq_config_attempted = false;
    lvgl_touch_irq_config_verified = false;
    lvgl_touch_irq_config_mode = -1;
    lvgl_touch_screen_idle_fail_safe = false;
    lvgl_touch_clear_interrupt();
    lvgl_touch_interrupt_gated = false;
    lvgl_touch_interrupt_armed = false;
    lvgl_touch_last_sample_ms = 0;
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_cached_point = {};
    lvgl_touch_error_streak.reset();
    lvgl_touch_recovery.reset();
    lvgl_touch_offline.store(false, std::memory_order_release);
    for (int index = 0;
         index < RotatedFramebufferPolicy::FRAME_BUFFER_COUNT;
         ++index) {
        lvgl_port_driver_fb[index] = nullptr;
    }
    lvgl_port_active_scanout_index = -1;
    lvgl_port_flip_layout = {};
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM_MAX; ++i) {
        lvgl_buf[i] = nullptr;
    }
    lvgl_touch_boot_quiet_until_ms = get_monotonic_ms() + lvgl_touch_boot_quiet_window_ms();
    lvgl_touch_read_block_until_ms.store(lvgl_touch_boot_quiet_until_ms,
                                         std::memory_order_release);

    auto bus_type = lcd->getBus()->getBasicAttributes().type;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    if (bus_type == ESP_PANEL_BUS_TYPE_RGB) {
        lvgl_refresh_callback_semantics =
            LVGL_PORT_REFRESH_CALLBACK_FRAME_BUFFER_COMPLETE;
    }
#else
    // BoardInit configures a non-zero RGB bounce buffer for every supported
    // Aura RGB profile, so ESP32_Display_Panel registers this exact callback.
    if (bus_type == ESP_PANEL_BUS_TYPE_RGB) {
        lvgl_refresh_callback_semantics =
            LVGL_PORT_REFRESH_CALLBACK_BOUNCE_FRAME_FINISH;
    }
#endif
    if (bus_type == ESP_PANEL_BUS_TYPE_MIPI_DSI) {
        lvgl_refresh_callback_semantics =
            LVGL_PORT_REFRESH_CALLBACK_DSI_REFRESH_DONE;
    }
#if LVGL_PORT_AVOID_TEAR
    ESP_UTILS_CHECK_FALSE_RETURN(
        (bus_type == ESP_PANEL_BUS_TYPE_RGB) || (bus_type == ESP_PANEL_BUS_TYPE_MIPI_DSI), false,
        "Avoid tearing function only works with RGB/MIPI-DSI LCD now"
    );
    ESP_UTILS_LOGI(
        "Avoid tearing is enabled, mode: %d, rotation: %d", LVGL_PORT_AVOID_TEARING_MODE, LVGL_PORT_ROTATION_DEGREE
    );
#endif

    lv_display_t *disp = nullptr;
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
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
    lvgl_port_lcd = lcd;
    lvgl_diagnostics_available.store(true, std::memory_order_release);

    // For non-RGB LCD, need to notify LVGL that the buffer is ready when the refresh is finished
    if (bus_type != ESP_PANEL_BUS_TYPE_RGB) {
        ESP_UTILS_LOGD("Attach refresh finish callback to LCD");
        lcd->attachDrawBitmapFinishCallback(onDrawBitmapFinishCallback, (void *)disp);
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

    lvgl_port_screen_flip_180.store(false, std::memory_order_release);
    lvgl_rotation_pipeline_active.store(false, std::memory_order_release);

    ESP_UTILS_LOGD("Create mutex for LVGL");
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mux == nullptr) {
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Create LVGL mutex failed");
        return false;
    }

    // Hold the serialization boundary while the task is created and the RGB
    // hand-off callback is attached. LVGL marks its first refresh ready
    // immediately, so allowing the new task to enter lv_timer_handler() before
    // callback registration could turn the first flush into a false sync
    // timeout.
    if (xSemaphoreTakeRecursive(lvgl_mux, portMAX_DELAY) != pdTRUE) {
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Lock LVGL startup barrier failed");
        return false;
    }

    ESP_UTILS_LOGD("Create LVGL task");
    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL,
                     LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);
    if (ret != pdPASS) {
        lvgl_task_handle = nullptr;
        (void)xSemaphoreGiveRecursive(lvgl_mux);
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Create LVGL task failed");
        return false;
    }

#if LVGL_PORT_AVOID_TEAR
    if (!lcd->attachRefreshFinishCallback(onLcdVsyncCallback,
                                          (void *)lvgl_task_handle)) {
        vTaskDelete(lvgl_task_handle);
        lvgl_task_handle = nullptr;
        (void)xSemaphoreGiveRecursive(lvgl_mux);
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Attach LCD refresh finish callback failed");
        return false;
    }
    lvgl_vsync_notify_enabled = true;
#endif
    lvgl_port_paused.store(false, std::memory_order_release);
    lvgl_pause_requested.store(false, std::memory_order_release);

    if (xSemaphoreGiveRecursive(lvgl_mux) != pdTRUE) {
        lvgl_vsync_notify_enabled = false;
        vTaskDelete(lvgl_task_handle);
        lvgl_task_handle = nullptr;
        (void)lvgl_port_cleanup_partial_init(disp, indev);
        ESP_UTILS_LOGE("Release LVGL startup barrier failed");
        return false;
    }

    return true;
}

static bool lvgl_port_lock_for(int timeout_ms, LvglLockDiagnostics::Purpose purpose)
{
    ESP_UTILS_CHECK_NULL_RETURN(lvgl_mux, false, "LVGL mutex is not initialized");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    const bool locked = (xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE);
    return lvgl_diag_lock_counts.recordAttempt(purpose, locked);
}

bool lvgl_port_lock(int timeout_ms)
{
    return lvgl_port_lock_for(timeout_ms, LvglLockDiagnostics::Purpose::Runtime);
}

bool lvgl_port_lock_startup_logo(int timeout_ms)
{
    // This explicit call site distinguishes bounded logo retries from normal
    // callers. No global startup phase suppresses failures on other tasks.
    return lvgl_port_lock_for(timeout_ms, LvglLockDiagnostics::Purpose::StartupLogo);
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
    return lvgl_port_screen_flip_180.load(std::memory_order_acquire);
}

bool lvgl_port_block_touch_read(uint32_t duration_ms,
                                bool require_explicit_release)
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
        lvgl_touch_release_gate_request.store(0, std::memory_order_release);
    } else {
        // This API is callable from Core 0 without the LVGL mutex. Defer all
        // plain policy/GPIO changes to the next serialized input callback.
        lvgl_touch_screen_reset_requested.store(true,
                                                 std::memory_order_release);
        lvgl_touch_request_release_gate(
            require_explicit_release ||
            lvgl_touch_pressed.load(std::memory_order_acquire));
        lvgl_touch_read_block_until_ms.store(now_ms + duration_ms,
                                             std::memory_order_release);
        lvgl_touch_wait_release_after_block.store(true,
                                                  std::memory_order_release);
    }
    return true;
}

bool lvgl_port_prepare_touch_hard_recovery(void)
{
    if (!lvgl_port_is_paused() || !lvgl_touch_i2c_runtime_gate.available() ||
        lvgl_port_touch == nullptr || lvgl_port_touch->getPanelHandle() == nullptr) {
        return false;
    }

    const bool require_explicit_release =
        lvgl_touch_pressed.load(std::memory_order_acquire);
    if (!lvgl_port_set_touch_mode(LVGL_PORT_TOUCH_MODE_SUPPRESSED)) {
        return false;
    }

    lvgl_touch_request_release_gate(require_explicit_release);
    lvgl_touch_clear_interrupt();
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    return true;
}

bool lvgl_port_complete_touch_hard_recovery(bool recovered,
                                            bool wake_probe_enabled)
{
    if (!lvgl_port_is_paused() || lvgl_port_touch == nullptr ||
        lvgl_port_touch->getPanelHandle() == nullptr) {
        return false;
    }

    esp_lcd_touch_handle_t panel = lvgl_port_touch->getPanelHandle();
    const gpio_num_t interrupt_gpio = panel->config.int_gpio_num;
    if (interrupt_gpio == GPIO_NUM_NC) {
        recovered = false;
    } else {
        gpio_config_t int_gpio_config = {};
        int_gpio_config.mode = GPIO_MODE_INPUT;
        int_gpio_config.intr_type = panel->config.levels.interrupt
                                        ? GPIO_INTR_POSEDGE
                                        : GPIO_INTR_NEGEDGE;
        int_gpio_config.pin_bit_mask = BIT64(interrupt_gpio);
        if (gpio_config(&int_gpio_config) != ESP_OK) {
            recovered = false;
        }
    }

    const uint32_t now_ms = get_monotonic_ms();
    lvgl_port_touch->resetPoints();
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_cached_point = {};
    lvgl_touch_last_sample_ms = 0;
    lvgl_touch_clear_interrupt();

    // RESET/INT address selection can reload the controller configuration.
    // Preserve a boot-sticky missed-IRQ fail-safe, otherwise verify 0x804D
    // again before any direct source is armed.
    if (!lvgl_touch_screen_idle_fail_safe) {
        lvgl_touch_irq_config_attempted = false;
        lvgl_touch_irq_config_verified = false;
        lvgl_touch_irq_config_mode = -1;
    }

    if (!recovered) {
        lvgl_touch_release_gate_request.store(0, std::memory_order_release);
        lvgl_touch_recovery.suspendUntilRetry();
        lvgl_touch_offline.store(true, std::memory_order_release);
        (void)lvgl_port_set_touch_mode(
            wake_probe_enabled ? LVGL_PORT_TOUCH_MODE_DARK_WAKE
                               : LVGL_PORT_TOUCH_MODE_SCREEN_ON);
        return false;
    }

    lvgl_touch_error_streak.reset();
    lvgl_touch_recovery.reset();
    lvgl_touch_offline.store(false, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(true, std::memory_order_release);
    lvgl_touch_read_block_until_ms.store(now_ms + LVGL_TOUCH_RECOVER_BLOCK_MS,
                                         std::memory_order_release);

    SharedI2cRuntimeGate::Gate::Access access =
        lvgl_touch_i2c_runtime_gate.acquire();
    if (!access) {
        return false;
    }
    if (!lvgl_touch_screen_idle_fail_safe) {
        (void)lvgl_touch_configure_runtime_interrupt(lvgl_port_touch, access);
    }
    return lvgl_port_set_touch_mode(
        wake_probe_enabled ? LVGL_PORT_TOUCH_MODE_DARK_WAKE
                           : LVGL_PORT_TOUCH_MODE_SCREEN_ON);
}

void lvgl_port_disable_touch_i2c(void)
{
    if (!lvgl_touch_i2c_runtime_gate.disable()) {
        return;
    }

    ESP_LOGW("LVGL", "shared I2C bus offline; new touch transactions disabled");
}

bool lvgl_port_wait_touch_i2c_idle(uint32_t timeout_ms)
{
    if (lvgl_touch_i2c_runtime_gate.available()) {
        return false;
    }
    const uint32_t started_ms = get_monotonic_ms();
    while (!lvgl_touch_i2c_runtime_gate.idle()) {
        if (static_cast<uint32_t>(get_monotonic_ms() - started_ms) >= timeout_ms) {
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}

bool lvgl_port_finalize_touch_i2c_disable_after_drain(void)
{
    if (lvgl_touch_i2c_runtime_gate.available() ||
        !lvgl_touch_i2c_runtime_gate.idle()) {
        return false;
    }
    lvgl_touch_mode.store(LVGL_PORT_TOUCH_MODE_DISABLED,
                          std::memory_order_release);
    lvgl_touch_screen_reset_requested.store(false,
                                             std::memory_order_release);
    lvgl_touch_release_gate_request.store(0, std::memory_order_release);
    lvgl_touch_screen_on_policy.reset(0, false);
    lvgl_touch_publish_screen_policy_mode();
    lvgl_touch_wake_policy_set(false, true, get_monotonic_ms());
    if (!lvgl_touch_unregister_direct_interrupt()) {
        return false;
    }
    lvgl_touch_clear_interrupt();
    lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
    lvgl_touch_wait_release_after_block.store(false, std::memory_order_release);
    lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    lvgl_touch_offline.store(true, std::memory_order_release);
    return true;
}

bool lvgl_port_set_touch_mode(lvgl_port_touch_mode_t mode)
{
    // This transition performs no I2C. Its callers already own the LVGL
    // serialization boundary, so only honor the lifecycle admission state;
    // do not enter the refcounted atomic RMW path at the wake edge.
    if (mode < LVGL_PORT_TOUCH_MODE_SUPPRESSED ||
        mode > LVGL_PORT_TOUCH_MODE_DISABLED) {
        return false;
    }
    if (!lvgl_touch_i2c_runtime_gate.available()) {
        if (mode == LVGL_PORT_TOUCH_MODE_SUPPRESSED ||
            mode == LVGL_PORT_TOUCH_MODE_DISABLED) {
            bool physical_state_confirmed =
                lvgl_touch_mask_direct_interrupt();
            if (physical_state_confirmed &&
                mode == LVGL_PORT_TOUCH_MODE_DISABLED) {
                physical_state_confirmed =
                    lvgl_touch_unregister_direct_interrupt();
            }
            lvgl_touch_mode.store(mode, std::memory_order_release);
            lvgl_touch_screen_reset_requested.store(
                false, std::memory_order_release);
            lvgl_touch_screen_on_policy.reset(0, false);
            lvgl_touch_publish_screen_policy_mode();
            lvgl_touch_release_gate_request.store(
                0, std::memory_order_release);
            lvgl_touch_wait_release_after_block.store(
                false, std::memory_order_release);
            lvgl_touch_read_block_until_ms.store(
                0, std::memory_order_release);
            lvgl_touch_wake_policy_set(
                false, true, get_monotonic_ms());
            lvgl_touch_clear_interrupt();
            return physical_state_confirmed;
        }
        return false;
    }

    const uint32_t now_ms = get_monotonic_ms();
    const bool touch_released =
        !lvgl_touch_pressed.load(std::memory_order_acquire);

    // Every ownership transition first masks the physical source. This also
    // protects the shared CH422G call which follows Suppressed mode.
    if (!lvgl_touch_mask_direct_interrupt()) {
        return false;
    }

    lvgl_touch_mode.store(mode, std::memory_order_release);
    lvgl_touch_screen_reset_requested.store(false,
                                             std::memory_order_release);
    lvgl_touch_screen_on_policy.reset(
        now_ms,
        mode == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
            lvgl_touch_runtime_irq_available());
    lvgl_touch_publish_screen_policy_mode();
    lvgl_touch_wake_policy_set(
        mode == LVGL_PORT_TOUCH_MODE_DARK_WAKE,
        touch_released,
        now_ms);

    if (mode != LVGL_PORT_TOUCH_MODE_SCREEN_ON) {
        lvgl_touch_release_gate_request.store(0, std::memory_order_release);
        lvgl_touch_wait_release_after_block.store(
            false, std::memory_order_release);
        lvgl_touch_read_block_until_ms.store(0, std::memory_order_release);
        lvgl_touch_set_cached_state(LV_INDEV_STATE_RELEASED);
    }
    if (mode != LVGL_PORT_TOUCH_MODE_DARK_WAKE ||
        !lvgl_touch_runtime_irq_available()) {
        lvgl_touch_clear_interrupt();
        return true;
    }

    // Publish dark-wake policy before the source can fire. The arm helper
    // samples the active level immediately so a held touch is not lost.
    lvgl_touch_clear_interrupt();
    if (!lvgl_touch_arm_direct_interrupt()) {
        return false;
    }
    if (!lvgl_touch_interrupt_gated) {
        lvgl_touch_clear_interrupt();
    }
    return true;
}

bool lvgl_port_take_wake_touch_pending(void)
{
    if (!lvgl_touch_i2c_runtime_gate.available()) {
        return false;
    }
    return lvgl_touch_wake_policy_take_pending();
}

bool lvgl_port_get_presented_frame_count(uint32_t *out)
{
#if LVGL_PORT_AVOID_TEAR && (LVGL_PORT_DIRECT_MODE || (LVGL_PORT_FULL_REFRESH && (LVGL_PORT_DISP_BUFFER_NUM == 2)))
    if (out == nullptr || lvgl_task_handle == nullptr ||
        lvgl_pause_requested.load(std::memory_order_acquire) ||
        lvgl_port_paused.load(std::memory_order_acquire) ||
        lvgl_display_sync_fault.load(std::memory_order_acquire) ||
        lvgl_display_task_fail_stopped.load(std::memory_order_acquire)) {
        return false;
    }
    *out = lvgl_presented_frame_count.load(std::memory_order_acquire);
    return true;
#else
    // Triple-buffer full-refresh and non-tearing paths have no synchronous
    // framebuffer acknowledgement here. Do not qualify them by flush count.
    (void)out;
    return false;
#endif
}

bool lvgl_port_wait_presented_frame(uint32_t baseline, uint32_t timeout_ms)
{
    if (xTaskGetCurrentTaskHandle() == lvgl_task_handle) {
        return false;
    }
    const uint32_t started_ms = get_monotonic_ms();
    for (;;) {
        uint32_t current = 0;
        if (!lvgl_port_get_presented_frame_count(&current)) {
            return false;
        }
        if (current != baseline) {
            return true;
        }
        if (static_cast<uint32_t>(get_monotonic_ms() - started_ms) >= timeout_ms) {
            return false;
        }
        vTaskDelay(1);
    }
}

bool lvgl_port_get_diagnostics(lvgl_port_diagnostics_t *out)
{
    ESP_UTILS_CHECK_FALSE_RETURN(out != nullptr, false, "Invalid diagnostics snapshot");
    if (!lvgl_diagnostics_available.load(std::memory_order_acquire)) {
        return false;
    }

    const uint32_t now_ms = get_rtos_ms();
    const uint32_t timer_last_ms = lvgl_diag_timer_handler_last_ms;
    const uint32_t flush_last_ms = lvgl_diag_flush_last_ms;
    const uint32_t vsync_last_ms = lvgl_diag_vsync_last_ms;

    out->sample_ms = now_ms;
    out->timer_handler_count = lvgl_diag_timer_handler_count;
    out->timer_handler_age_ms = lvgl_diag_age_ms(now_ms, timer_last_ms);
    out->task_stack_size_bytes = LVGL_PORT_TASK_STACK_SIZE;
    out->task_stack_min_free_bytes = lvgl_diag_stack_min_free_bytes.load(std::memory_order_relaxed);
    out->flush_count = lvgl_diag_flush_count;
    out->flush_age_ms = lvgl_diag_age_ms(now_ms, flush_last_ms);
    out->vsync_count = lvgl_diag_vsync_count;
    out->vsync_age_ms = lvgl_diag_age_ms(now_ms, vsync_last_ms);
    out->refresh_callback_max_gap_ms =
        lvgl_diag_refresh_callback_max_gap_ms;
    out->refresh_callback_semantics = lvgl_refresh_callback_semantics;
    out->vsync_wait_timeout_count = lvgl_diag_vsync_wait_timeout_count;
    out->presented_frame_count =
        lvgl_presented_frame_count.load(std::memory_order_acquire);
    out->display_sync_fault =
        lvgl_display_sync_fault.load(std::memory_order_acquire);
    const LvglLockDiagnostics::Snapshot lock_counts = lvgl_diag_lock_counts.snapshot();
    out->lock_fail_count = lock_counts.runtime_failures;
    out->startup_lock_miss_count = lock_counts.startup_logo_misses;
    out->touch_read_error_count = lvgl_diag_touch_read_error_count;
    out->touch_offline = lvgl_touch_offline.load(std::memory_order_acquire);
    out->touch_mode = lvgl_touch_runtime_mode_text();
    out->touch_irq_registered =
        lvgl_touch_interrupt_gated.load(std::memory_order_acquire);
    out->touch_irq_armed =
        lvgl_touch_interrupt_armed.load(std::memory_order_acquire);
    out->touch_irq_config_verified =
        lvgl_touch_irq_config_verified.load(std::memory_order_acquire);
    out->touch_irq_config_mode =
        lvgl_touch_irq_config_mode.load(std::memory_order_acquire);
    out->touch_screen_idle_enabled =
        lvgl_touch_current_mode() == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
        lvgl_touch_runtime_irq_available();
    out->touch_screen_idle_active =
        lvgl_touch_current_mode() == LVGL_PORT_TOUCH_MODE_SCREEN_ON &&
        static_cast<ScreenOnTouchPolicy::Mode>(
            lvgl_touch_screen_policy_mode_diag.load(
                std::memory_order_acquire)) ==
            ScreenOnTouchPolicy::Mode::IdleIrq;
    out->touch_screen_idle_fail_safe =
        lvgl_touch_screen_idle_fail_safe.load(std::memory_order_acquire);
    out->touch_status_read_count =
        lvgl_diag_touch_status_read_count.load(std::memory_order_relaxed);
    out->touch_full_read_count =
        lvgl_diag_touch_full_read_count.load(std::memory_order_relaxed);
    out->touch_idle_skip_count =
        lvgl_diag_touch_idle_skip_count.load(std::memory_order_relaxed);
    out->touch_idle_entry_count =
        lvgl_diag_touch_idle_entry_count.load(std::memory_order_relaxed);
    out->touch_idle_irq_exit_count =
        lvgl_diag_touch_idle_irq_exit_count.load(std::memory_order_relaxed);
    out->touch_idle_fallback_probe_count =
        lvgl_diag_touch_idle_fallback_probe_count.load(
            std::memory_order_relaxed);
    out->touch_idle_missed_irq_press_count =
        lvgl_diag_touch_idle_missed_irq_press_count.load(
            std::memory_order_relaxed);
    out->touch_irq_arm_failure_count =
        lvgl_diag_touch_irq_arm_failure_count.load(std::memory_order_relaxed);
    out->touch_irq_no_frame_count =
        lvgl_diag_touch_irq_no_frame_count.load(std::memory_order_relaxed);
    out->screen_flip_180 =
        lvgl_port_screen_flip_180.load(std::memory_order_acquire);
    out->rotation_pipeline_active =
        lvgl_rotation_pipeline_active.load(std::memory_order_acquire);
    out->rotated_copy_switch_count = lvgl_diag_rotated_copy_switch_count;
    out->framebuffer_ownership_violation_count =
        lvgl_diag_framebuffer_ownership_violation_count;
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

bool lvgl_port_display_task_fail_stopped(void)
{
    return lvgl_display_task_fail_stopped.load(std::memory_order_acquire);
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
    bool lifecycle_empty =
        lvgl_task_handle == nullptr &&
        lvgl_mux == nullptr &&
        lvgl_port_lcd == nullptr &&
        lvgl_port_touch == nullptr &&
        !lvgl_vsync_notify_enabled &&
        lvgl_touch_registered_interrupt_handle == nullptr;
#if !LV_TICK_CUSTOM
    lifecycle_empty = lifecycle_empty && lvgl_tick_timer == nullptr;
#endif
    if (lifecycle_empty) {
        return true;
    }

    // The vendor RGB refresh callback cannot be detached safely in every
    // supported build, and LV_MEM_CUSTOM prevents a complete LVGL teardown.
    // Deleting the task would therefore leave an ISR with a stale TCB and make
    // a same-boot reinitialization assume framebuffer zero is active again.
    // Keep the live lifecycle untouched and fail closed. Reboot teardown uses
    // lvgl_port_prepare_restart(), which retains the suspended task storage
    // until reset and makes the callback inert first.
    ESP_UTILS_LOGE(
        "Runtime LVGL deinit/reinit is unsupported; use restart preparation");
    return false;
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
    lvgl_touch_mode.store(LVGL_PORT_TOUCH_MODE_DISABLED,
                          std::memory_order_release);
    lvgl_touch_screen_reset_requested.store(false,
                                             std::memory_order_release);
    lvgl_touch_screen_on_policy.reset(0, false);
    lvgl_touch_publish_screen_policy_mode();
    (void)lvgl_touch_unregister_direct_interrupt();
    lvgl_touch_clear_interrupt();
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
