/*
 * SPDX-FileCopyrightText: 2018-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ESP-IDF v5.3.2 can disable the ESP32-S3 caches while the other core is
 * still running. If that core touches flash during the short window before
 * it is reset and stalled, esp_restart() panics with
 * "Cache disabled but cached memory region accessed" and falls through to
 * an RTC watchdog reset.
 *
 * This is the v5.3.2 implementation with the ordering fix from Espressif
 * commit f99c3c6 (released in ESP-IDF v5.3.3): reset and stall the other CPU
 * before disabling either cache. The linker wraps only esp_restart_noos(), so
 * the normal esp_restart() shutdown handlers still run before this function.
 *
 * Keep the exact-version guard: a future framework migration must use its own
 * upstream implementation instead of silently carrying this private-API
 * backport forward.
 */

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_idf_version.h"
#include "esp_private/rtc_clk.h"
#include "esp_private/system_internal.h"
#include "esp_rom_sys.h"
#include "hal/wdt_hal.h"
#include "soc/dport_reg.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/rtc_periph.h"
#include "soc/soc_memory_layout.h"
#include "soc/syscon_reg.h"
#include "soc/timer_group_reg.h"

#include "esp32s3/rom/cache.h"
#include "esp32s3/rom/rtc.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3) || !CONFIG_IDF_TARGET_ESP32S3
#error "EspRestartNoosBackport is only valid for ESP32-S3"
#endif

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 3, 2)
#error "Remove EspRestartNoosBackport when ESP-IDF is no longer v5.3.2"
#endif

#define AURA_ALIGN_DOWN(value, alignment) \
    ((value) & ~((alignment) - 1))

extern int _bss_end;

void IRAM_ATTR __attribute__((noreturn)) __wrap_esp_restart_noos(void)
{
    // Disable interrupts on the core performing the restart.
    esp_cpu_intr_disable(0xFFFFFFFF);

    // Protect flash boot with the RTC watchdog while the reset is in flight.
    wdt_hal_context_t rtc_wdt_ctx;
    wdt_hal_init(&rtc_wdt_ctx, WDT_RWDT, 0, false);
    const uint32_t stage_timeout_ticks =
        (uint32_t)(1000ULL * rtc_clk_slow_freq_get_hz() / 1000ULL);
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_config_stage(&rtc_wdt_ctx,
                         WDT_STAGE0,
                         stage_timeout_ticks,
                         WDT_STAGE_ACTION_RESET_SYSTEM);
    wdt_hal_config_stage(&rtc_wdt_ctx,
                         WDT_STAGE1,
                         stage_timeout_ticks,
                         WDT_STAGE_ACTION_RESET_RTC);
    wdt_hal_set_flashboot_en(&rtc_wdt_ctx, true);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);

    // Disable the task and interrupt watchdogs before resetting peripherals.
    wdt_hal_context_t wdt0_context = {
        .inst = WDT_MWDT0,
        .mwdt_dev = &TIMERG0,
    };
    wdt_hal_write_protect_disable(&wdt0_context);
    wdt_hal_disable(&wdt0_context);
    wdt_hal_write_protect_enable(&wdt0_context);

    wdt_hal_context_t wdt1_context = {
        .inst = WDT_MWDT1,
        .mwdt_dev = &TIMERG1,
    };
    wdt_hal_write_protect_disable(&wdt1_context);
    wdt_hal_disable(&wdt1_context);
    wdt_hal_write_protect_enable(&wdt1_context);

#ifdef CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY
    if (esp_ptr_external_ram(esp_cpu_get_sp())) {
        const uint32_t new_sp = AURA_ALIGN_DOWN(_bss_end, 16);
        SET_STACK(new_sp);
    }
#endif

    const uint32_t core_id = esp_cpu_get_core_id();
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    // Upstream fix f99c3c6: the other core must not execute flash-backed code
    // after either cache has been disabled.
    const uint32_t other_core_id = (core_id == 0) ? 1 : 0;
    esp_rom_software_reset_cpu(other_core_id);
    esp_cpu_stall(other_core_id);
#endif

    Cache_Disable_ICache();
    Cache_Disable_DCache();

    // Restore the flash GPIO matrix defaults expected by the ROM bootloader.
    WRITE_PERI_REG(GPIO_FUNC0_IN_SEL_CFG_REG, 0x30);
    WRITE_PERI_REG(GPIO_FUNC1_IN_SEL_CFG_REG, 0x30);
    WRITE_PERI_REG(GPIO_FUNC2_IN_SEL_CFG_REG, 0x30);
    WRITE_PERI_REG(GPIO_FUNC3_IN_SEL_CFG_REG, 0x30);
    WRITE_PERI_REG(GPIO_FUNC4_IN_SEL_CFG_REG, 0x30);
    WRITE_PERI_REG(GPIO_FUNC5_IN_SEL_CFG_REG, 0x30);

    esp_system_reset_modules_on_exit();

#if !CONFIG_IDF_ENV_FPGA
    rtc_clk_cpu_set_to_default_config();
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    REG_WRITE(SYSTEM_CORE_1_CONTROL_1_REG, 0);
#endif

    if (core_id == 0) {
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        esp_rom_software_reset_cpu(1);
#endif
        esp_rom_software_reset_cpu(0);
    }
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    else {
        esp_rom_software_reset_cpu(0);
        esp_cpu_unstall(0);
        esp_rom_software_reset_cpu(1);
    }
#endif

    for (;;) {
    }
}
