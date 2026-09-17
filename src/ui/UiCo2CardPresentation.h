// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace UiCo2CardPresentation {
struct State {
    bool warmup;
    bool show_value;
    bool show_unit;
};
inline State resolve(bool valid, bool warming_up) {
    const bool warmup = warming_up && !valid;
    return {warmup, !warmup, !warmup};
}
}
