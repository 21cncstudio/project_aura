// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <esp_system.h>
#include <stdint.h>

extern uint32_t boot_count;
extern uint32_t safe_boot_stage;
extern esp_reset_reason_t boot_reset_reason;
extern bool boot_i2c_recovered;
extern bool boot_touch_detected;
extern bool boot_ui_auto_recovery_reboot;
extern bool boot_board_auto_recovery_reboot;
extern bool boot_board_cold_start;

void boot_mark_ui_auto_recovery_reboot();
bool boot_consume_ui_auto_recovery_reboot();
bool boot_ui_auto_recovery_restart_pending();
void boot_mark_board_auto_recovery_reboot();
bool boot_consume_board_auto_recovery_reboot();
bool boot_board_power_settle_completed();
void boot_mark_board_power_settle_complete();
