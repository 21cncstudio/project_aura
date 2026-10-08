// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <lvgl.h>

// Single-display, single software draw unit. All calls share the LVGL mutex.
// The port must acknowledge a submitted buffer ONLY after the LCD hand-off.
namespace NativeRotation {
using FaultHandler = void (*)(const char *reason);
bool begin(lv_display_t *display, void *initial_scanout, bool flip_180,
           FaultHandler fault);
bool canSubmit(const void *buffer);
bool acknowledge(const void *buffer);
// Partial-init rollback only, after stopping the LVGL task; not runtime teardown.
void reset();
}
