#include <stdint.h>
#include "esp_system.h"

uint32_t boot_count = 0;
uint32_t safe_boot_stage = 0;
esp_reset_reason_t boot_reset_reason = ESP_RST_POWERON;
bool boot_i2c_recovered = false;
bool boot_touch_detected = false;
bool boot_ui_auto_recovery_reboot = false;
bool boot_board_auto_recovery_reboot = false;
bool boot_board_cold_start = true;
bool boot_peripherals_cold_start = true;
bool boot_peripherals_may_have_lost_power = true;

namespace {
bool board_power_settle_completed = false;
bool ui_auto_recovery_restart_pending = false;
bool board_auto_recovery_restart_pending = false;
}

void boot_mark_ui_auto_recovery_reboot() {
    boot_ui_auto_recovery_reboot = true;
    ui_auto_recovery_restart_pending = true;
}

bool boot_consume_ui_auto_recovery_reboot() {
    bool flagged = boot_ui_auto_recovery_reboot;
    boot_ui_auto_recovery_reboot = false;
    return flagged;
}

bool boot_ui_auto_recovery_restart_pending() {
    return ui_auto_recovery_restart_pending;
}

void boot_mark_board_auto_recovery_reboot() {
    boot_board_auto_recovery_reboot = true;
    board_auto_recovery_restart_pending = true;
}

bool boot_consume_board_auto_recovery_reboot() {
    const bool flagged = boot_board_auto_recovery_reboot;
    boot_board_auto_recovery_reboot = false;
    return flagged;
}

bool boot_board_auto_recovery_restart_pending() {
    return board_auto_recovery_restart_pending;
}

bool boot_any_auto_recovery_boot() {
    return boot_ui_auto_recovery_reboot || boot_board_auto_recovery_reboot;
}

bool boot_board_power_settle_completed() {
    return board_power_settle_completed;
}

void boot_mark_board_power_settle_complete() {
    board_power_settle_completed = true;
}
