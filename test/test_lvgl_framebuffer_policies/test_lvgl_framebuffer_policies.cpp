#include <unity.h>

#include "core/RotatedFramebufferPolicy.h"
#include "core/TouchReleaseGatePolicy.h"

void setUp() {}
void tearDown() {}

void test_initial_layout_keeps_both_lvgl_renderers_off_scanout() {
    for (int active = 0; active < 3; ++active) {
        const auto layout =
            RotatedFramebufferPolicy::makeInitialLayout(active);
        TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(layout));
        TEST_ASSERT_EQUAL_INT(active, layout.scanout);
        TEST_ASSERT_NOT_EQUAL(active, layout.renderer_a);
        TEST_ASSERT_NOT_EQUAL(active, layout.renderer_b);
        TEST_ASSERT_NOT_EQUAL(layout.renderer_a, layout.renderer_b);
    }
    TEST_ASSERT_FALSE(RotatedFramebufferPolicy::valid(
        RotatedFramebufferPolicy::makeInitialLayout(-1)));
}

void test_flip_entry_partitions_all_buffers_and_keeps_active_as_output() {
    for (int active = 0; active < 3; ++active) {
        for (int preferred = -1; preferred < 3; ++preferred) {
            const auto layout =
                RotatedFramebufferPolicy::makeFlipLayout(active, preferred);
            TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(layout));
            TEST_ASSERT_NOT_EQUAL(active, layout.renderer);
            TEST_ASSERT_TRUE(
                RotatedFramebufferPolicy::ownsActive(layout, active));
            TEST_ASSERT_NOT_EQUAL(
                active,
                RotatedFramebufferPolicy::selectInactiveOutput(layout, active));
            if (RotatedFramebufferPolicy::validIndex(preferred) &&
                preferred != active) {
                TEST_ASSERT_EQUAL_INT(preferred, layout.renderer);
            }
        }
    }
}

void test_outputs_alternate_away_from_active_source() {
    for (int active = 0; active < 3; ++active) {
        const auto layout =
            RotatedFramebufferPolicy::makeFlipLayout(active, (active + 1) % 3);
        TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(layout));
        const int first =
            RotatedFramebufferPolicy::selectInactiveOutput(layout, active);
        const int second =
            RotatedFramebufferPolicy::selectInactiveOutput(layout, first);
        TEST_ASSERT_NOT_EQUAL(active, first);
        TEST_ASSERT_EQUAL_INT(active, second);
        TEST_ASSERT_NOT_EQUAL(layout.renderer, first);
        TEST_ASSERT_NOT_EQUAL(layout.renderer, second);
    }
}

void test_flip_exit_restores_active_and_safe_offscreen_renderer() {
    for (int active = 0; active < 3; ++active) {
        const auto flip =
            RotatedFramebufferPolicy::makeFlipLayout(active, (active + 1) % 3);
        const int next_active =
            RotatedFramebufferPolicy::selectInactiveOutput(flip, active);
        const auto normal =
            RotatedFramebufferPolicy::makeNormalLayout(flip, next_active);
        TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(normal));
        TEST_ASSERT_EQUAL_INT(next_active, normal.on_screen);
        TEST_ASSERT_EQUAL_INT(flip.renderer, normal.renderer);
        TEST_ASSERT_NOT_EQUAL(normal.on_screen, normal.renderer);
    }
}

void test_flip_enable_disable_cycles_preserve_safe_ownership() {
    int active = 0;
    int preferred_renderer = 1;
    for (int cycle = 0; cycle < 8; ++cycle) {
        const auto flip = RotatedFramebufferPolicy::makeFlipLayout(
            active, preferred_renderer);
        TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(flip));
        TEST_ASSERT_NOT_EQUAL(active, flip.renderer);

        for (int frame = 0; frame < 5; ++frame) {
            const int writable =
                RotatedFramebufferPolicy::selectInactiveOutput(flip, active);
            TEST_ASSERT_TRUE(RotatedFramebufferPolicy::validIndex(writable));
            TEST_ASSERT_NOT_EQUAL(active, writable);
            TEST_ASSERT_NOT_EQUAL(flip.renderer, writable);
            active = writable; // Models publication after callback ACK only.
        }

        const auto normal =
            RotatedFramebufferPolicy::makeNormalLayout(flip, active);
        TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(normal));
        TEST_ASSERT_EQUAL_INT(active, normal.on_screen);
        TEST_ASSERT_NOT_EQUAL(active, normal.renderer);

        // A normal-mode acknowledged handoff makes the former renderer active
        // and the old scanout the preferred off-screen renderer next cycle.
        preferred_renderer = normal.on_screen;
        active = normal.renderer;
    }
}

void test_invalid_or_unowned_state_fails_closed() {
    const auto invalid = RotatedFramebufferPolicy::makeFlipLayout(-1, 0);
    TEST_ASSERT_FALSE(RotatedFramebufferPolicy::valid(invalid));

    const auto layout = RotatedFramebufferPolicy::makeFlipLayout(0, 1);
    TEST_ASSERT_TRUE(RotatedFramebufferPolicy::valid(layout));
    TEST_ASSERT_EQUAL_INT(
        -1, RotatedFramebufferPolicy::selectInactiveOutput(layout, 1));
    TEST_ASSERT_FALSE(RotatedFramebufferPolicy::valid(
        RotatedFramebufferPolicy::makeNormalLayout(layout, 1)));
    TEST_ASSERT_FALSE(RotatedFramebufferPolicy::allDistinct(0, 0, 2));
}

void test_release_gate_explicit_release_always_opens() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate gate;
    gate.begin(false, true);
    TEST_ASSERT_EQUAL(Decision::Open,
                      gate.observe(ProbeResult::Released, true, 100));
    TEST_ASSERT_FALSE(gate.waiting());
}

void test_release_gate_quiet_fallback_needs_two_spaced_samples() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate gate;
    gate.begin(true, false);
    TEST_ASSERT_TRUE(gate.quietFallbackAllowed());
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 100));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 139));
    TEST_ASSERT_EQUAL(Decision::Open,
                      gate.observe(ProbeResult::NoData, false, 140));
    TEST_ASSERT_FALSE(gate.waiting());
}

void test_release_gate_quiet_fallback_requires_safe_begin_state() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate cached_press;
    cached_press.begin(false, false);
    TEST_ASSERT_FALSE(cached_press.quietFallbackAllowed());
    TEST_ASSERT_EQUAL(Decision::Hold,
                      cached_press.observe(ProbeResult::NoData, false, 100));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      cached_press.observe(ProbeResult::NoData, false, 200));

    Gate active_interrupt;
    active_interrupt.begin(true, true);
    TEST_ASSERT_FALSE(active_interrupt.quietFallbackAllowed());
    TEST_ASSERT_EQUAL(
        Decision::Hold,
        active_interrupt.observe(ProbeResult::NoData, false, 100));
    TEST_ASSERT_EQUAL(
        Decision::Hold,
        active_interrupt.observe(ProbeResult::NoData, false, 200));
}

void test_release_gate_press_requires_later_explicit_release() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate gate;
    gate.begin(true, false);
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::Pressed, true, 100));
    TEST_ASSERT_FALSE(gate.quietFallbackAllowed());
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 200));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 300));
    TEST_ASSERT_EQUAL(Decision::Open,
                      gate.observe(ProbeResult::Released, false, 301));
}

void test_release_gate_error_and_active_interrupt_reset_quiet_window() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate gate;
    gate.begin(true, false);
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 100));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::Error, false, 140));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 180));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, true, 220));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 260));
    TEST_ASSERT_EQUAL(Decision::Open,
                      gate.observe(ProbeResult::NoData, false, 300));
}

void test_release_gate_quiet_interval_is_millis_wrap_safe() {
    using TouchReleaseGatePolicy::Decision;
    using TouchReleaseGatePolicy::Gate;
    using TouchReleaseGatePolicy::ProbeResult;

    Gate gate;
    gate.begin(true, false);
    TEST_ASSERT_EQUAL(
        Decision::Hold,
        gate.observe(ProbeResult::NoData, false, UINT32_MAX - 19U));
    TEST_ASSERT_EQUAL(Decision::Hold,
                      gate.observe(ProbeResult::NoData, false, 19U));
    TEST_ASSERT_EQUAL(Decision::Open,
                      gate.observe(ProbeResult::NoData, false, 20U));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_layout_keeps_both_lvgl_renderers_off_scanout);
    RUN_TEST(test_flip_entry_partitions_all_buffers_and_keeps_active_as_output);
    RUN_TEST(test_outputs_alternate_away_from_active_source);
    RUN_TEST(test_flip_exit_restores_active_and_safe_offscreen_renderer);
    RUN_TEST(test_flip_enable_disable_cycles_preserve_safe_ownership);
    RUN_TEST(test_invalid_or_unowned_state_fails_closed);
    RUN_TEST(test_release_gate_explicit_release_always_opens);
    RUN_TEST(test_release_gate_quiet_fallback_needs_two_spaced_samples);
    RUN_TEST(test_release_gate_quiet_fallback_requires_safe_begin_state);
    RUN_TEST(test_release_gate_press_requires_later_explicit_release);
    RUN_TEST(test_release_gate_error_and_active_interrupt_reset_quiet_window);
    RUN_TEST(test_release_gate_quiet_interval_is_millis_wrap_safe);
    return UNITY_END();
}
