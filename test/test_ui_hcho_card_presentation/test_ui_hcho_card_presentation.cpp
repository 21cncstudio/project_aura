#include <unity.h>

#include "ui/UiHchoCardPresentation.h"

namespace {

using UiHchoCardPresentation::Mode;
using UiHchoCardPresentation::State;

void assert_state(const State &state,
                  Mode mode,
                  bool use_hcho_identity,
                  bool show_warmup_label,
                  bool show_value,
                  bool show_unit) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(mode), static_cast<int>(state.mode));
    TEST_ASSERT_EQUAL(use_hcho_identity, state.use_hcho_identity);
    TEST_ASSERT_EQUAL(show_warmup_label, state.show_warmup_label);
    TEST_ASSERT_EQUAL(show_value, state.show_value);
    TEST_ASSERT_EQUAL(show_unit, state.show_unit);
}

} // namespace

void setUp() {}

void tearDown() {}

void test_hcho_card_uses_aqi_fallback_without_sensor_data() {
    assert_state(UiHchoCardPresentation::resolve(false, false),
                 Mode::AqiFallback,
                 false,
                 false,
                 true,
                 true);
}

void test_hcho_card_shows_measurement_and_ppb_when_valid() {
    assert_state(UiHchoCardPresentation::resolve(true, false),
                 Mode::Measurement,
                 true,
                 false,
                 true,
                 true);
}

void test_hcho_card_shows_only_warmup_while_sensor_is_starting() {
    assert_state(UiHchoCardPresentation::resolve(false, true),
                 Mode::Warmup,
                 true,
                 true,
                 false,
                 false);
}

void test_hcho_warmup_hides_stale_measurement() {
    assert_state(UiHchoCardPresentation::resolve(true, true),
                 Mode::Warmup,
                 true,
                 true,
                 false,
                 false);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hcho_card_uses_aqi_fallback_without_sensor_data);
    RUN_TEST(test_hcho_card_shows_measurement_and_ppb_when_valid);
    RUN_TEST(test_hcho_card_shows_only_warmup_while_sensor_is_starting);
    RUN_TEST(test_hcho_warmup_hides_stale_measurement);
    return UNITY_END();
}
