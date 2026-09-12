#include <unity.h>
#include "ui/UiCo2CardPresentation.h"
void setUp() {}
void tearDown() {}
void test_startup_hides_value_and_unit_until_co2_ready() {
    const auto initial=UiCo2CardPresentation::resolve(false,true);
    TEST_ASSERT_TRUE(initial.warmup); TEST_ASSERT_FALSE(initial.show_value); TEST_ASSERT_FALSE(initial.show_unit);
    const auto ready=UiCo2CardPresentation::resolve(true,false);
    TEST_ASSERT_FALSE(ready.warmup); TEST_ASSERT_TRUE(ready.show_value); TEST_ASSERT_TRUE(ready.show_unit);
}
void test_fault_shows_missing_value_instead_of_permanent_warmup() {
    const auto failed=UiCo2CardPresentation::resolve(false,false);
    TEST_ASSERT_FALSE(failed.warmup); TEST_ASSERT_TRUE(failed.show_value);
}
void test_valid_reading_takes_precedence_over_stale_warmup_flag() {
    const auto value=UiCo2CardPresentation::resolve(true,true);
    TEST_ASSERT_FALSE(value.warmup); TEST_ASSERT_TRUE(value.show_value);
}
int main(int,char**) {
    UNITY_BEGIN();
    RUN_TEST(test_startup_hides_value_and_unit_until_co2_ready);
    RUN_TEST(test_fault_shows_missing_value_instead_of_permanent_warmup);
    RUN_TEST(test_valid_reading_takes_precedence_over_stale_warmup_flag);
    return UNITY_END();
}
