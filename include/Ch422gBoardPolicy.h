// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// C-compatible: shared by the vendored CH422G driver and the optional probe.
// Both boards route GPIO19/20 to native USB when EXIO5 is LOW. Never preload
// the upstream all-HIGH image before enabling the IO bank.
#ifndef AURA_HARDWARE_PROFILE_7
#define AURA_HARDWARE_PROFILE_7 0
#endif

#define AURA_CH422G_USB_SEL_MASK (1U << 5)
#define AURA_CH422G_BACKLIGHT_MASK (1U << 2)

#if AURA_HARDWARE_PROFILE_7 == 1
// Preserve the reviewed 7-inch LCD/touch/backlight startup image exactly.
#define AURA_CH422G_INITIAL_IO_VALUE 0xD1U
#elif AURA_HARDWARE_PROFILE_7 == 0
// Keep native USB selected and backlight off until the first logo frame.
// All other 4.3-inch startup levels remain unchanged from the 0xDF image.
#define AURA_CH422G_INITIAL_IO_VALUE 0xDBU
#else
#error "AURA_HARDWARE_PROFILE_7 must be 0 or 1"
#endif

#if (AURA_CH422G_INITIAL_IO_VALUE & AURA_CH422G_USB_SEL_MASK) != 0
#error "CH422G startup must keep native USB selected"
#endif

#if (AURA_CH422G_INITIAL_IO_VALUE & AURA_CH422G_BACKLIGHT_MASK) != 0
#error "CH422G startup must keep the backlight off"
#endif
