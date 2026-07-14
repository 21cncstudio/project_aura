#include <stdint.h>
#include "esp_system.h"

uint32_t boot_count = 0;
uint32_t safe_boot_stage = 0;
esp_reset_reason_t boot_reset_reason = ESP_RST_POWERON;
bool boot_i2c_recovered = false;
bool boot_touch_detected = false;
bool boot_ui_auto_recovery_reboot = false;
bool boot_board_auto_recovery_reboot = false;

void boot_mark_ui_auto_recovery_reboot() {
    boot_ui_auto_recovery_reboot = true;
}

bool boot_consume_ui_auto_recovery_reboot() {
    bool flagged = boot_ui_auto_recovery_reboot;
    boot_ui_auto_recovery_reboot = false;
    return flagged;
}

void boot_mark_board_auto_recovery_reboot() {
    boot_board_auto_recovery_reboot = true;
}

bool boot_consume_board_auto_recovery_reboot() {
    const bool flagged = boot_board_auto_recovery_reboot;
    boot_board_auto_recovery_reboot = false;
    return flagged;
}
