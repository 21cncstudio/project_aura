#include <unity.h>

#include <initializer_list>

#include "core/Gt911DiagnosticPolicy.h"

using namespace Gt911DiagnosticPolicy;

void setUp() {}
void tearDown() {}

void test_successful_selection_is_info_but_failed_sequence_warns() {
    TEST_ASSERT_TRUE(selectionSeverity(true) == Severity::Info);
    TEST_ASSERT_TRUE(selectionSeverity(false) == Severity::Warning);
}

void test_configured_identity_requires_successful_read_and_valid_id() {
    TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Configured, ReadResult::Ok, true, false) ==
                     Severity::Info);
    TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Configured, ReadResult::Ok, false, false) ==
                     Severity::Warning);
    for (const auto result : {ReadResult::GenericFailure, ReadResult::Timeout,
                             ReadResult::OtherFailure}) {
        TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Configured, result, false, true) ==
                         Severity::Warning);
        TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Configured, result, true, true) ==
                         Severity::Warning);
    }
}

void test_configured_health_requires_both_identity_and_config() {
    TEST_ASSERT_TRUE(configuredAddressHealthy(true, ReadResult::Ok));
    TEST_ASSERT_FALSE(configuredAddressHealthy(false, ReadResult::Ok));
    for (const auto result : {ReadResult::GenericFailure, ReadResult::Timeout,
                             ReadResult::OtherFailure}) {
        TEST_ASSERT_FALSE(configuredAddressHealthy(true, result));
    }
}

void test_opposite_generic_failure_is_info_only_after_configured_health() {
    TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Opposite, ReadResult::GenericFailure,
                                     false, true) == Severity::Info);
    TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Opposite, ReadResult::GenericFailure,
                                     false, false) == Severity::Warning);
}

void test_opposite_timeout_and_unknown_failure_never_become_expected() {
    for (const auto result : {ReadResult::Timeout, ReadResult::OtherFailure}) {
        for (const bool healthy : {false, true}) {
            TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Opposite, result, false, healthy) ==
                             Severity::Warning);
        }
    }
}

void test_unexpected_opposite_responder_warns_even_with_a_valid_id() {
    for (const bool valid : {false, true}) {
        for (const bool healthy : {false, true}) {
            TEST_ASSERT_TRUE(identitySeverity(ProbeRole::Opposite, ReadResult::Ok,
                                             valid, healthy) == Severity::Warning);
        }
    }
}

void test_config_read_failure_and_opposite_config_remain_warnings() {
    TEST_ASSERT_TRUE(configSeverity(ProbeRole::Configured, ReadResult::Ok) == Severity::Info);
    for (const auto result : {ReadResult::Ok, ReadResult::GenericFailure,
                             ReadResult::Timeout, ReadResult::OtherFailure}) {
        TEST_ASSERT_TRUE(configSeverity(ProbeRole::Opposite, result) == Severity::Warning);
        if (result != ReadResult::Ok) {
            TEST_ASSERT_TRUE(configSeverity(ProbeRole::Configured, result) == Severity::Warning);
        }
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_successful_selection_is_info_but_failed_sequence_warns);
    RUN_TEST(test_configured_identity_requires_successful_read_and_valid_id);
    RUN_TEST(test_configured_health_requires_both_identity_and_config);
    RUN_TEST(test_opposite_generic_failure_is_info_only_after_configured_health);
    RUN_TEST(test_opposite_timeout_and_unknown_failure_never_become_expected);
    RUN_TEST(test_unexpected_opposite_responder_warns_even_with_a_valid_id);
    RUN_TEST(test_config_read_failure_and_opposite_config_remain_warnings);
    return UNITY_END();
}
