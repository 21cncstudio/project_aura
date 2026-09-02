// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The split firmware profiles own the GT911 strap: 4.3-inch uses 0x14 and
// 7-inch uses 0x5D.
#ifndef AURA_HARDWARE_PROFILE_7
#define AURA_HARDWARE_PROFILE_7 0
#endif
#ifndef AURA_GT911_I2C_ADDRESS
#if AURA_HARDWARE_PROFILE_7
#define AURA_GT911_I2C_ADDRESS 0x5D
#else
#define AURA_GT911_I2C_ADDRESS 0x14
#endif
#endif
#if AURA_HARDWARE_PROFILE_7 != 0 && AURA_HARDWARE_PROFILE_7 != 1
#error "AURA_HARDWARE_PROFILE_7 must be 0 or 1"
#endif
#if AURA_GT911_I2C_ADDRESS != 0x14 && AURA_GT911_I2C_ADDRESS != 0x5D
#error "GT911 supports only the 7-bit addresses 0x14 and 0x5D"
#endif
#if AURA_HARDWARE_PROFILE_7 && AURA_GT911_I2C_ADDRESS != 0x5D
#error "The 7-inch production profile requires GT911 address 0x5D"
#endif
#if !AURA_HARDWARE_PROFILE_7 && AURA_GT911_I2C_ADDRESS != 0x14
#error "The 4.3-inch production profile requires GT911 address 0x14"
#endif
