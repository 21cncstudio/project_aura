// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

namespace RgbDmaDiagnostics {

// Runtime observations from the exact ESP-IDF 5.3.2 RGB driver used by the
// firmware. UINT32_MAX timestamps and ages mean that no matching ISR start has
// occurred since boot.
struct Snapshot {
    bool instrumented = false;
    bool layout_valid = false;
    bool snapshot_coherent = false;
    uint32_t expected_eof_per_frame = 0;
    uint32_t rgb_isr_dma_start_count = 0;
    uint32_t rgb_isr_dma_start_last_ms = UINT32_MAX;
    uint32_t rgb_isr_dma_start_age_ms = UINT32_MAX;
    uint32_t rgb_isr_dma_start_failure_count = 0;
};

bool getSnapshot(Snapshot *out);

}  // namespace RgbDmaDiagnostics
