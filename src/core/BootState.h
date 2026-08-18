// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <esp_system.h>
#include <stdint.h>

// This storage is intentionally declared before the three legacy BootState
// RTC_NOINIT words in BootState.cpp. The toolchain emits those input sections
// in reverse declaration order, which keeps the deployed 7c3f8e6 addresses at
// 0x50000280/84/88 and appends this storage at 0x5000028c.
constexpr uint32_t BOOT_BACKLIGHT_WAKE_EVIDENCE_WORDS = 10;
extern uint32_t
    boot_backlight_wake_evidence_words[BOOT_BACKLIGHT_WAKE_EVIDENCE_WORDS];

extern uint32_t boot_count;
extern uint32_t safe_boot_stage;
extern esp_reset_reason_t boot_reset_reason;
extern bool boot_i2c_recovered;
extern bool boot_touch_detected;
extern bool boot_ui_auto_recovery_reboot;
extern bool boot_board_auto_recovery_reboot;
extern bool boot_board_cold_start;
extern bool boot_peripherals_cold_start;
extern bool boot_peripherals_may_have_lost_power;

void boot_mark_ui_auto_recovery_reboot();
bool boot_consume_ui_auto_recovery_reboot();
bool boot_ui_auto_recovery_restart_pending();
void boot_mark_board_auto_recovery_reboot();
bool boot_consume_board_auto_recovery_reboot();
bool boot_board_auto_recovery_restart_pending();
bool boot_any_auto_recovery_boot();
bool boot_board_power_settle_completed();
void boot_mark_board_power_settle_complete();
