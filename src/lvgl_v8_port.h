/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "sdkconfig.h"
#ifdef CONFIG_ARDUINO_RUNNING_CORE
#include <Arduino.h>
#endif
#include "esp_display_panel.hpp"
#include "lvgl.h"

// *INDENT-OFF*

/**
 * LVGL related parameters, can be adjusted by users
 */
#define LVGL_PORT_TICK_PERIOD_MS                (2) // The period of the LVGL tick task, in milliseconds

/**
 *
 * LVGL buffer related parameters, can be adjusted by users:
 *
 *  (These parameters will be useless if the avoid tearing function is enabled)
 *
 *  - Memory type for buffer allocation:
 *      - MALLOC_CAP_SPIRAM: Allocate LVGL buffer in PSRAM
 *      - MALLOC_CAP_INTERNAL: Allocate LVGL buffer in SRAM
 *
 *      (The SRAM is faster than PSRAM, but the PSRAM has a larger capacity)
 *      (For SPI/QSPI LCD, it is recommended to allocate the buffer in SRAM, because the SPI DMA does not directly support PSRAM now)
 *
 *  - The size (in bytes) and number of buffers:
 *      - Lager buffer size can improve FPS, but it will occupy more memory. Maximum buffer size is `width * height`.
 *      - The number of buffers should be 1 or 2.
 */
#define LVGL_PORT_BUFFER_MALLOC_CAPS            (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)       // Allocate LVGL buffer in SRAM
// #define LVGL_PORT_BUFFER_MALLOC_CAPS            (MALLOC_CAP_SPIRAM)      // Allocate LVGL buffer in PSRAM
#define LVGL_PORT_BUFFER_SIZE_HEIGHT            (20)
#define LVGL_PORT_BUFFER_NUM                    (2)

/**
 * LVGL timer handle task related parameters, can be adjusted by users
 */
#define LVGL_PORT_TASK_MAX_DELAY_MS             (500)       // The maximum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_MIN_DELAY_MS             (2)         // The minimum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_STACK_SIZE               (6 * 1024)  // The stack size of the LVGL timer task, in bytes
#define LVGL_PORT_TASK_PRIORITY                 (2)         // The priority of the LVGL timer task
#ifdef ARDUINO_RUNNING_CORE
#define LVGL_PORT_TASK_CORE                     (ARDUINO_RUNNING_CORE)  // Valid if using Arduino
#else
#define LVGL_PORT_TASK_CORE                     (1)                     // Valid if using ESP-IDF
#endif
                                                            // The core of the LVGL timer task, `-1` means the don't specify the core
                                                            // Default is the same as the main core
                                                            // This can be set to `1` only if the SoCs support dual-core,
                                                            // otherwise it should be set to `-1` or `0`

/**
 * Avoid tering related configurations, can be adjusted by users.
 *
 *  (Currently, This function only supports RGB LCD and the version of LVGL must be >= 8.3.9)
 */
/**
 * Set the avoid tearing mode:
 *      - 0: Disable avoid tearing function
 *      - 1: LCD double-buffer & LVGL full-refresh
 *      - 2: LCD triple-buffer & LVGL full-refresh
 *      - 3: LCD double-buffer & LVGL direct-mode (recommended)
 */
#ifdef CONFIG_LVGL_PORT_AVOID_TEARING_MODE
#define LVGL_PORT_AVOID_TEARING_MODE            (CONFIG_LVGL_PORT_AVOID_TEARING_MODE)
                                                        // Valid if using ESP-IDF
#else
#define LVGL_PORT_AVOID_TEARING_MODE            (3)     // Valid if using Arduino
#endif

#if LVGL_PORT_AVOID_TEARING_MODE != 0
/**
 * When avoid tearing is enabled, the LVGL software rotation `lv_disp_set_rotation()` is not supported.
 * But users can set the rotation degree(0/90/180/270) here, but this function will reduce FPS.
 *
 * Set the rotation degree:
 *      - 0: 0 degree
 *      - 90: 90 degree
 *      - 180: 180 degree
 *      - 270: 270 degree
 */
#ifdef CONFIG_LVGL_PORT_ROTATION_DEGREE
#define LVGL_PORT_ROTATION_DEGREE               (CONFIG_LVGL_PORT_ROTATION_DEGREE)
                                                        // Valid if using ESP-IDF
#else
#define LVGL_PORT_ROTATION_DEGREE               (0)     // Valid if using Arduino
#endif

/**
 * Here, some important configurations will be set based on different anti-tearing modes and rotation angles.
 * No modification is required here.
 *
 * Users should use `lcd_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);` to set the buffer number before. If screen drifting occurs, please refer to the Troubleshooting section in the README.
 * initializing the LCD bus
 */
#define LVGL_PORT_AVOID_TEAR                    (1)
// Set the buffer number and refresh mode according to the different modes
#if LVGL_PORT_AVOID_TEARING_MODE == 1
    #define LVGL_PORT_DISP_BUFFER_NUM           (2)
    #define LVGL_PORT_FULL_REFRESH              (1)
#elif LVGL_PORT_AVOID_TEARING_MODE == 2
    #define LVGL_PORT_DISP_BUFFER_NUM           (3)
    #define LVGL_PORT_FULL_REFRESH              (1)
#elif LVGL_PORT_AVOID_TEARING_MODE == 3
    #define LVGL_PORT_DISP_BUFFER_NUM           (3)
    #define LVGL_PORT_DIRECT_MODE               (1)
#else
    #error "Invalid avoid tearing mode, please set macro `LVGL_PORT_AVOID_TEARING_MODE` to one of `LVGL_PORT_AVOID_TEARING_MODE_*`"
#endif
// Check rotation
#if (LVGL_PORT_ROTATION_DEGREE != 0) && (LVGL_PORT_ROTATION_DEGREE != 90) && (LVGL_PORT_ROTATION_DEGREE != 180) && \
    (LVGL_PORT_ROTATION_DEGREE != 270)
    #error "Invalid rotation degree, please set to 0, 90, 180 or 270"
#elif LVGL_PORT_ROTATION_DEGREE != 0
    #ifdef LVGL_PORT_DISP_BUFFER_NUM
        #undef LVGL_PORT_DISP_BUFFER_NUM
        #define LVGL_PORT_DISP_BUFFER_NUM           (3)
    #endif
#endif
#endif /* LVGL_PORT_AVOID_TEARING_MODE */

// *INDENT-ON*

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Porting LVGL with LCD and touch panel. This function should be called after the initialization of the LCD and touch panel.
 *
 * @param lcd The pointer to the LCD panel device, mustn't be nullptr
 * @param tp  The pointer to the touch panel device, set to nullptr if is not used
 *
 * If initialization fails, all timers, callbacks and partially registered
 * LVGL resources created by this call are released before returning.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_init(esp_panel::drivers::LCD *lcd, esp_panel::drivers::Touch *tp);

/**
 * @brief Deinitialize the LVGL porting.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_deinit(void);

/**
 * @brief Pause LVGL task and tick timer.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_pause(void);

/**
 * @brief Request cooperative pause of the LVGL task.
 *
 * The request is handled by the LVGL task itself after it finishes the current
 * timer-handler iteration and releases the LVGL mutex.
 *
 * @return true if the request was accepted
 */
bool lvgl_port_request_pause(void);

/**
 * @brief Return whether the LVGL port is currently paused.
 *
 * @return true when paused
 */
bool lvgl_port_is_paused(void);

/**
 * @brief Return whether the display task entered its terminal fail-stop.
 *
 * The acknowledgement is published immediately before the task suspends
 * itself and guarantees that it will not mutate LVGL or touch state again in
 * this boot.
 */
bool lvgl_port_display_task_fail_stopped(void);

/**
 * @brief Resume LVGL task and tick timer.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_resume(void);

/**
 * @brief Request cooperative resume of the LVGL task.
 *
 * @return true if the request was accepted
 */
bool lvgl_port_request_resume(void);

/**
 * @brief Begin a non-forcing LVGL shutdown.
 *
 * Requests a cooperative pause but keeps the task handle, callbacks and timers
 * intact. This must be used before shared-I2C shutdown work so a task is never
 * force-suspended while it may own the bus.
 *
 * @return true when an LVGL task exists and was asked to pause
 */
bool lvgl_port_request_quiesce(void);

/**
 * @brief Prepare LVGL port for immediate system restart.
 *
 * Disables VSYNC task notifications and detaches refresh callback to avoid
 * ISR activity using stale task handles during reboot transition.
 *
 * @return true if request accepted
 */
bool lvgl_port_prepare_restart(void);

/**
 * @brief Lock the LVGL mutex. This function should be called before calling any LVGL APIs when not in LVGL task,
 *        and the `lvgl_port_unlock()` function should be called later.
 *
 * @param timeout_ms The timeout of the mutex lock, in milliseconds. If the timeout is set to `-1`, it will wait indefinitely.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * @brief Unlock the LVGL mutex. This function should be called after using LVGL APIs when not in LVGL task, and the
 *        `lvgl_port_lock()` function should be called before.
 *
 * @return true if success, otherwise false
 */
bool lvgl_port_unlock(void);

/**
 * @brief Rotate the visible screen and touch coordinates by 180 degrees.
 *
 * In RGB direct-mode this uses a dedicated rotated framebuffer instead of
 * panel mirroring because RGB panels keep scanning the framebuffer in the
 * original order.
 *
 * @param enabled True for 180-degree flipped mounting.
 *
 * @return true if the display port accepted the setting.
 */
bool lvgl_port_set_screen_flip_180(bool enabled);

/**
 * @brief Return the active 180-degree screen flip setting.
 *
 * @return true when screen flip is enabled.
 */
bool lvgl_port_get_screen_flip_180(void);

/**
 * @brief Temporarily suppress touch reads in LVGL input callback.
 *
 * During this window LVGL receives `RELEASED` state without querying the touch controller.
 * Useful right after wake-up to avoid unstable I2C reads.
 *
 * @param duration_ms Blocking window duration in milliseconds
 *
 * @return true if the touch-read blocking window was updated.
 */
bool lvgl_port_block_touch_read(uint32_t duration_ms);

/**
 * @brief Prepare paused LVGL touch state for a coordinated GT911 hard reset.
 *
 * The LVGL task must already be paused. This masks the direct touch IRQ and
 * clears cached input without disabling the one-way runtime I2C gate.
 *
 * @return true when the touch path is safe for the external reset sequence.
 */
bool lvgl_port_prepare_touch_hard_recovery(void);

/**
 * @brief Restore GT911 GPIO/input state after a coordinated hard reset.
 *
 * @param recovered True only after product ID was verified at address 0x14.
 * @param wake_probe_enabled Restore dark-screen wake probing when true.
 * @return true when the recovered touch and IRQ state were restored.
 */
bool lvgl_port_complete_touch_hard_recovery(bool recovered,
                                            bool wake_probe_enabled);

/**
 * @brief Permanently disable touch-controller I2C traffic for this boot.
 *
 * The gate is reset only by a new lvgl_port_init() lifecycle. Once disabled,
 * the input callback returns RELEASED before normal reads, wake probing, or
 * soft recovery can access the shared bus.
 */
void lvgl_port_disable_touch_i2c(void);

/**
 * @brief Wait for touch-I2C users admitted before disable to drain.
 *
 * This function has no touch-policy or interrupt side effects. The touch gate
 * must already be disabled.
 */
bool lvgl_port_wait_touch_i2c_idle(uint32_t timeout_ms);

/**
 * @brief Finalize disabled touch state after drain and LVGL serialization.
 *
 * The caller must hold the LVGL mutex (or otherwise prove the LVGL task is
 * paused) so plain cached input and interrupt-policy state cannot race a late
 * handler.
 */
bool lvgl_port_finalize_touch_i2c_disable_after_drain(void);

/**
 * @brief Enable/disable wake-touch probe mode.
 *
 * In probe mode touch reads do not generate LVGL pressed events.
 * When the controller exposes an interrupt line, touch is sampled only after
 * a new interrupt, with a sparse polling fallback if interrupts are lost.
 * The physical interrupt is armed only in probe mode and remains masked while
 * the screen is on; normal screen-on touch input uses the regular LVGL poll.
 *
 * @param enabled True to enable wake-touch probe mode.
 *
 * @return true if the requested physical state was confirmed, or direct IRQ
 *         control was safely retired in favor of polling fallback.
 */
bool lvgl_port_set_wake_touch_probe(bool enabled);

/**
 * @brief Consume pending wake request captured by wake-touch probe mode.
 *
 * @return true if a wake touch was captured since the last consume call.
 */
bool lvgl_port_take_wake_touch_pending(void);

typedef struct {
    uint32_t sample_ms;
    uint32_t timer_handler_count;
    uint32_t timer_handler_age_ms;
    uint32_t flush_count;
    uint32_t flush_age_ms;
    uint32_t vsync_count;
    uint32_t vsync_age_ms;
    uint32_t vsync_wait_timeout_count;
    bool display_sync_fault;
    uint32_t lock_fail_count;
    uint32_t touch_read_error_count;
    bool touch_offline;
    bool paused;
} lvgl_port_diagnostics_t;

/**
 * @brief Return LVGL/runtime diagnostics snapshot for freeze investigation.
 *
 * @param out Pointer to destination snapshot.
 *
 * @return true if snapshot stored, false for invalid argument.
 */
bool lvgl_port_get_diagnostics(lvgl_port_diagnostics_t *out);

#ifdef __cplusplus
}
#endif
