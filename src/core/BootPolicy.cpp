#include "core/BootPolicy.h"

#include <stdint.h>

StorageManager::BootAction BootPolicy::apply(bool crash_reset,
                                             uint32_t &boot_count,
                                             uint32_t &safe_boot_stage,
                                             uint8_t max_reboots) {
    if (!crash_reset) {
        boot_count = 0;
        safe_boot_stage = 0;
        return StorageManager::BootAction::Normal;
    }

    if (boot_count < UINT32_MAX) {
        boot_count++;
    }

    if (boot_count >= max_reboots) {
        if (safe_boot_stage == 0) {
            safe_boot_stage = 1;
            return StorageManager::BootAction::SafeRollback;
        }
        return StorageManager::BootAction::SafeFactoryReset;
    }

    return StorageManager::BootAction::Normal;
}

bool BootPolicy::markStable(uint32_t now_ms,
                            uint32_t boot_start_ms,
                            uint32_t stable_ms,
                            bool &boot_stable,
                            uint32_t &boot_count,
                            uint32_t &safe_boot_stage) {
    if (boot_stable) {
        return false;
    }
    if ((now_ms - boot_start_ms) < stable_ms) {
        return false;
    }
    boot_count = 0;
    safe_boot_stage = 0;
    boot_stable = true;
    return true;
}
