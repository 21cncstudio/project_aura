#include <unity.h>

#include <atomic>
#include <chrono>
#include <thread>

// Compile the production bridge in this focused native test. WebUiBridge.cpp
// uses a local header include so test/mocks/web/WebUiBridge.h cannot shadow it.
#include "../../src/web/WebUiBridge.cpp"

namespace {

using namespace std::chrono_literals;

WebUiBridge::ApplyResult directSettingsApply(
    const WebUiBridge::SettingsUpdate &update,
    void *ctx) {
    auto *call_count = static_cast<uint32_t *>(ctx);
    ++(*call_count);
    WebUiBridge::ApplyResult result{};
    result.success = update.has_backlight;
    result.status_code = 200;
    return result;
}

bool waitForSettingsRequest(WebUiBridge &bridge,
                            WebUiBridge::SettingsUpdate &update,
                            uint32_t &request_id) {
    for (uint32_t attempt = 0; attempt < 200; ++attempt) {
        if (bridge.consumePendingSettingsRequest(update, request_id)) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

void configureDeferredBridge(WebUiBridge &bridge, uint32_t &callback_count) {
    bridge.bindSettingsApplier(&callback_count, &directSettingsApply);
    bridge.setDispatchMode(WebUiBridge::DispatchMode::DeferredReply);
}

} // namespace

void setUp() {
    // Keep the production 5000-tick policy while making it 50 ms in native tests.
    FreeRtosSemaphoreMock::setTickDurationMicroseconds(10);
}

void tearDown() {
    FreeRtosSemaphoreMock::resetTickDuration();
}

void test_direct_dispatch_contract_is_unchanged() {
    WebUiBridge bridge;
    uint32_t callback_count = 0;
    bridge.bindSettingsApplier(&callback_count, &directSettingsApply);

    WebUiBridge::SettingsUpdate update{};
    update.has_backlight = true;
    const WebUiBridge::ApplyResult result = bridge.applySettings(update);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT16(200, result.status_code);
    TEST_ASSERT_EQUAL_UINT32(1, callback_count);
}

void test_unconsumed_request_times_out_and_is_cancelled() {
    uint32_t callback_count = 0;
    WebUiBridge bridge;
    configureDeferredBridge(bridge, callback_count);
    WebUiBridge::SettingsUpdate update{};
    update.has_backlight = true;

    const WebUiBridge::ApplyResult timed_out = bridge.applySettings(update);

    TEST_ASSERT_FALSE(timed_out.success);
    TEST_ASSERT_EQUAL_UINT16(504, timed_out.status_code);
    WebUiBridge::SettingsUpdate consumed{};
    uint32_t request_id = 0;
    TEST_ASSERT_FALSE(bridge.consumePendingSettingsRequest(consumed, request_id));

    WebUiBridge::ApplyResult retry_result{};
    std::thread retry([&]() { retry_result = bridge.applySettings(update); });
    const bool retry_consumed = waitForSettingsRequest(bridge, consumed, request_id);
    WebUiBridge::ApplyResult completed{};
    completed.success = true;
    completed.status_code = 202;
    if (retry_consumed) {
        bridge.completePendingSettingsRequest(request_id, completed);
    }
    retry.join();

    TEST_ASSERT_TRUE(retry_consumed);
    TEST_ASSERT_TRUE(retry_result.success);
    TEST_ASSERT_EQUAL_UINT16(202, retry_result.status_code);
    TEST_ASSERT_EQUAL_UINT32(0, callback_count);
}

void test_consumed_request_remains_owned_past_initial_timeout() {
    uint32_t callback_count = 0;
    WebUiBridge bridge;
    configureDeferredBridge(bridge, callback_count);
    WebUiBridge::SettingsUpdate update{};
    update.has_backlight = true;
    std::atomic<bool> first_finished{false};
    WebUiBridge::ApplyResult first_result{};
    std::thread first([&]() {
        first_result = bridge.applySettings(update);
        first_finished.store(true);
    });

    WebUiBridge::SettingsUpdate consumed{};
    uint32_t first_request_id = 0;
    const bool first_consumed = waitForSettingsRequest(bridge, consumed, first_request_id);
    std::this_thread::sleep_for(70ms);
    const bool finished_before_completion = first_finished.load();

    const WebUiBridge::ApplyResult concurrent = bridge.applySettings(update);

    WebUiBridge::ApplyResult completed{};
    completed.success = true;
    completed.status_code = 200;
    if (first_consumed) {
        bridge.completePendingSettingsRequest(first_request_id, completed);
    }
    first.join();

    TEST_ASSERT_TRUE(first_consumed);
    TEST_ASSERT_FALSE(finished_before_completion);
    TEST_ASSERT_FALSE(concurrent.success);
    TEST_ASSERT_EQUAL_UINT16(503, concurrent.status_code);
    TEST_ASSERT_TRUE(first_result.success);
    TEST_ASSERT_EQUAL_UINT16(200, first_result.status_code);
}

void test_wrong_or_stale_completion_cannot_signal_another_generation() {
    uint32_t callback_count = 0;
    WebUiBridge bridge;
    configureDeferredBridge(bridge, callback_count);
    WebUiBridge::SettingsUpdate update{};
    update.has_backlight = true;

    WebUiBridge::ApplyResult first_result{};
    std::thread first([&]() { first_result = bridge.applySettings(update); });
    WebUiBridge::SettingsUpdate consumed{};
    uint32_t first_request_id = 0;
    const bool first_consumed = waitForSettingsRequest(bridge, consumed, first_request_id);
    if (!first_consumed) {
        first.join();
        TEST_FAIL_MESSAGE("first deferred request was not consumed");
        return;
    }

    WebUiBridge::ApplyResult wrong{};
    wrong.success = false;
    wrong.status_code = 418;
    bridge.completePendingSettingsRequest(first_request_id + 1, wrong);
    std::this_thread::sleep_for(70ms);

    const WebUiBridge::ApplyResult still_busy = bridge.applySettings(update);

    WebUiBridge::ApplyResult first_completed{};
    first_completed.success = true;
    first_completed.status_code = 201;
    bridge.completePendingSettingsRequest(first_request_id, first_completed);
    first.join();
    TEST_ASSERT_EQUAL_UINT16(503, still_busy.status_code);
    TEST_ASSERT_EQUAL_UINT16(201, first_result.status_code);

    std::atomic<bool> second_finished{false};
    WebUiBridge::ApplyResult second_result{};
    std::thread second([&]() {
        second_result = bridge.applySettings(update);
        second_finished.store(true);
    });
    uint32_t second_request_id = 0;
    const bool second_consumed = waitForSettingsRequest(bridge, consumed, second_request_id);
    if (!second_consumed) {
        second.join();
        TEST_FAIL_MESSAGE("second deferred request was not consumed");
        return;
    }

    bridge.completePendingSettingsRequest(first_request_id, wrong);
    std::this_thread::sleep_for(10ms);
    const bool stale_completion_finished_second = second_finished.load();

    WebUiBridge::ApplyResult second_completed{};
    second_completed.success = true;
    second_completed.status_code = 202;
    bridge.completePendingSettingsRequest(second_request_id, second_completed);
    second.join();
    TEST_ASSERT_NOT_EQUAL(first_request_id, second_request_id);
    TEST_ASSERT_FALSE(stale_completion_finished_second);
    TEST_ASSERT_EQUAL_UINT16(202, second_result.status_code);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_direct_dispatch_contract_is_unchanged);
    RUN_TEST(test_unconsumed_request_times_out_and_is_cancelled);
    RUN_TEST(test_consumed_request_remains_owned_past_initial_timeout);
    RUN_TEST(test_wrong_or_stale_completion_cannot_signal_another_generation);
    return UNITY_END();
}
