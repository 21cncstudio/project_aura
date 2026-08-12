// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "core/BootState.h"
#include <esp_attr.h>

namespace {

constexpr uint32_t BOOT_UI_AUTO_RECOVERY_MAGIC = 0xA11A0F5Au;
constexpr uint32_t BOOT_BOARD_AUTO_RECOVERY_MAGIC = 0xB04D1C2Cu;
constexpr uint32_t BOOT_BOARD_POWER_SETTLED_MAGIC = 0xB04D5E77u;
// These markers must survive esp_restart(). Their magic values also make the
// undefined RTC contents after a real power loss safe to reject.
RTC_NOINIT_ATTR uint32_t boot_ui_auto_recovery_magic;
RTC_NOINIT_ATTR uint32_t boot_board_auto_recovery_magic;
RTC_NOINIT_ATTR uint32_t boot_board_power_settled_magic;

} // namespace

RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR uint32_t safe_boot_stage = 0;
esp_reset_reason_t boot_reset_reason = ESP_RST_UNKNOWN;
bool boot_i2c_recovered = false;
bool boot_touch_detected = false;
bool boot_ui_auto_recovery_reboot = false;
bool boot_board_auto_recovery_reboot = false;
bool boot_board_cold_start = false;
bool boot_peripherals_cold_start = false;
bool boot_peripherals_may_have_lost_power = false;
bool boot_ui_auto_recovery_restart_requested = false;
bool boot_board_auto_recovery_restart_requested = false;

void boot_mark_ui_auto_recovery_reboot() {
    boot_ui_auto_recovery_magic = BOOT_UI_AUTO_RECOVERY_MAGIC;
    boot_ui_auto_recovery_restart_requested = true;
}

bool boot_consume_ui_auto_recovery_reboot() {
    bool flagged = (boot_ui_auto_recovery_magic == BOOT_UI_AUTO_RECOVERY_MAGIC);
    boot_ui_auto_recovery_magic = 0;
    return flagged;
}

bool boot_ui_auto_recovery_restart_pending() {
    return boot_ui_auto_recovery_restart_requested;
}

void boot_mark_board_auto_recovery_reboot() {
    boot_board_auto_recovery_magic = BOOT_BOARD_AUTO_RECOVERY_MAGIC;
    boot_board_auto_recovery_restart_requested = true;
}

bool boot_consume_board_auto_recovery_reboot() {
    const bool flagged = (boot_board_auto_recovery_magic == BOOT_BOARD_AUTO_RECOVERY_MAGIC);
    boot_board_auto_recovery_magic = 0;
    return flagged;
}

bool boot_board_auto_recovery_restart_pending() {
    return boot_board_auto_recovery_restart_requested;
}

bool boot_any_auto_recovery_boot() {
    return boot_ui_auto_recovery_reboot || boot_board_auto_recovery_reboot;
}

bool boot_board_power_settle_completed() {
    return boot_board_power_settled_magic == BOOT_BOARD_POWER_SETTLED_MAGIC;
}

void boot_mark_board_power_settle_complete() {
    boot_board_power_settled_magic = BOOT_BOARD_POWER_SETTLED_MAGIC;
}
