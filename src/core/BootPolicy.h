#pragma once

#include <stdint.h>

#include "modules/StorageManager.h"

namespace BootPolicy {
    StorageManager::BootAction apply(bool crash_reset,
                                     uint32_t &boot_count,
                                     uint32_t &safe_boot_stage,
                                     uint8_t max_reboots);
    bool markStable(uint32_t now_ms,
                    uint32_t boot_start_ms,
                    uint32_t stable_ms,
                    bool &boot_stable,
                    uint32_t &boot_count,
                    uint32_t &safe_boot_stage);
}
