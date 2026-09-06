// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace RotatedFramebufferPolicy {

constexpr int FRAME_BUFFER_COUNT = 3;

struct InitialLayout {
    int scanout;
    int renderer_a;
    int renderer_b;

    constexpr InitialLayout(int scanout_index = -1,
                            int first_renderer_index = -1,
                            int second_renderer_index = -1)
        : scanout(scanout_index),
          renderer_a(first_renderer_index),
          renderer_b(second_renderer_index) {}
};

struct FlipLayout {
    int renderer;
    int output_a;
    int output_b;

    constexpr FlipLayout(int renderer_index = -1,
                         int first_output_index = -1,
                         int second_output_index = -1)
        : renderer(renderer_index),
          output_a(first_output_index),
          output_b(second_output_index) {}
};

struct NormalLayout {
    int on_screen;
    int renderer;

    constexpr NormalLayout(int on_screen_index = -1,
                           int renderer_index = -1)
        : on_screen(on_screen_index), renderer(renderer_index) {}
};

constexpr bool validIndex(int index) {
    return index >= 0 && index < FRAME_BUFFER_COUNT;
}

constexpr bool allDistinct(int first, int second, int third) {
    return validIndex(first) && validIndex(second) && validIndex(third) &&
           first != second && first != third && second != third;
}

constexpr bool valid(const FlipLayout &layout) {
    return allDistinct(layout.renderer, layout.output_a, layout.output_b);
}

constexpr bool valid(const InitialLayout &layout) {
    return allDistinct(layout.scanout, layout.renderer_a, layout.renderer_b);
}

constexpr bool valid(const NormalLayout &layout) {
    return validIndex(layout.on_screen) && validIndex(layout.renderer) &&
           layout.on_screen != layout.renderer;
}

constexpr InitialLayout makeInitialLayout(int initial_scanout) {
    if (!validIndex(initial_scanout)) {
        return {};
    }
    InitialLayout layout{};
    layout.scanout = initial_scanout;
    for (int index = 0; index < FRAME_BUFFER_COUNT; ++index) {
        if (index == initial_scanout) {
            continue;
        }
        if (!validIndex(layout.renderer_a)) {
            layout.renderer_a = index;
        } else {
            layout.renderer_b = index;
        }
    }
    return valid(layout) ? layout : InitialLayout{};
}

inline int selectRenderer(int active_scanout, int preferred_renderer) {
    if (!validIndex(active_scanout)) {
        return -1;
    }
    if (validIndex(preferred_renderer) && preferred_renderer != active_scanout) {
        return preferred_renderer;
    }
    for (int index = 0; index < FRAME_BUFFER_COUNT; ++index) {
        if (index != active_scanout) {
            return index;
        }
    }
    return -1;
}

inline FlipLayout makeFlipLayout(int active_scanout,
                                 int preferred_renderer) {
    FlipLayout layout{};
    layout.renderer = selectRenderer(active_scanout, preferred_renderer);
    if (!validIndex(layout.renderer)) {
        return {};
    }

    // Keep the acknowledged scanout in the output pair. The other output is
    // therefore the only safe destination for the first rotated copy.
    layout.output_a = active_scanout;
    for (int index = 0; index < FRAME_BUFFER_COUNT; ++index) {
        if (index != layout.renderer && index != layout.output_a) {
            layout.output_b = index;
            break;
        }
    }
    return valid(layout) ? layout : FlipLayout{};
}

constexpr bool ownsActive(const FlipLayout &layout, int active_scanout) {
    return valid(layout) &&
           (active_scanout == layout.output_a ||
            active_scanout == layout.output_b);
}

inline int selectInactiveOutput(const FlipLayout &layout,
                                int active_scanout) {
    if (!ownsActive(layout, active_scanout)) {
        return -1;
    }
    return active_scanout == layout.output_a ? layout.output_b
                                             : layout.output_a;
}

constexpr NormalLayout makeNormalLayout(const FlipLayout &layout,
                                        int active_scanout) {
    return ownsActive(layout, active_scanout) &&
                   layout.renderer != active_scanout
               ? NormalLayout{active_scanout, layout.renderer}
               : NormalLayout{};
}

} // namespace RotatedFramebufferPolicy
