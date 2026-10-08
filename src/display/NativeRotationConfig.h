// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Local 7-inch candidate. The 4.3-inch and legacy Arduino ports retain their path.
#if (defined(AURA_NATIVE_IDF) && AURA_NATIVE_IDF && AURA_HARDWARE_PROFILE_7) || defined(AURA_NATIVE_ROTATION_HOST_TEST)
#define AURA_NATIVE_ROTATION 1
#else
#define AURA_NATIVE_ROTATION 0
#endif
