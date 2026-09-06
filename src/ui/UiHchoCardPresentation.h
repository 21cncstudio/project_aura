// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>

namespace UiHchoCardPresentation {

enum class Mode : uint8_t {
    AqiFallback = 0,
    Measurement,
    Warmup,
};

struct State {
    Mode mode = Mode::AqiFallback;
    bool use_hcho_identity = false;
    bool show_warmup_label = false;
    bool show_value = true;
    bool show_unit = true;
};

inline State resolve(bool hcho_valid, bool hcho_warmup) {
    State state;
    if (hcho_warmup) {
        state.mode = Mode::Warmup;
        state.use_hcho_identity = true;
        state.show_warmup_label = true;
        state.show_value = false;
        state.show_unit = false;
        return state;
    }

    if (hcho_valid) {
        state.mode = Mode::Measurement;
        state.use_hcho_identity = true;
    }
    return state;
}

} // namespace UiHchoCardPresentation
