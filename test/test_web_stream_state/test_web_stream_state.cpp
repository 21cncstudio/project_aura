#include <unity.h>

#include "web/WebStreamState.h"

void setUp() {}
void tearDown() {}

void test_stream_state_tracks_transfer_pause_window() {
    WebStreamState state;

    state.beginTransfer(100);
    TEST_ASSERT_TRUE(state.shouldPauseMqtt(100));

    state.endTransfer(120);
    TEST_ASSERT_TRUE(state.shouldPauseMqtt(150));
    TEST_ASSERT_EQUAL_UINT32(1970, state.pauseRemainingMs(150));
    TEST_ASSERT_FALSE(state.shouldPauseMqtt(2121));
}

void test_stream_state_shell_priority_uses_recent_sta_window_and_does_not_shorten() {
    WebStreamState state;

    state.noteShellPriority(1000, 1000);
    TEST_ASSERT_TRUE(state.shouldPauseMqtt(20000));

    state.noteShellPriority(10000, 30000);
    TEST_ASSERT_TRUE(state.shouldPauseMqtt(30000));
    TEST_ASSERT_FALSE(state.shouldPauseMqtt(45001));
}

void test_stream_state_snapshot_includes_stats_and_deferred_counts() {
    WebStreamState state;

    state.noteMqttConnectDeferred();
    state.noteMqttPublishDeferred();
    state.recordStreamResult("/diag", 1000, 400, false, StreamAbortReason::SocketWriteError, 250, 11, 200);

    const WebTransferSnapshot snapshot = state.snapshot(0);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.stats.ok_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.abort_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.slow_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.mqtt_connect_deferred_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.stats.mqtt_publish_deferred_count);
    TEST_ASSERT_EQUAL(StreamAbortReason::SocketWriteError, snapshot.stats.last_abort_reason);
    TEST_ASSERT_EQUAL_INT(11, snapshot.stats.last_errno);
    TEST_ASSERT_EQUAL_UINT32(400, static_cast<uint32_t>(snapshot.stats.last_sent));
    TEST_ASSERT_EQUAL_UINT32(1000, static_cast<uint32_t>(snapshot.stats.last_total));
    TEST_ASSERT_EQUAL_UINT32(250, snapshot.stats.last_max_write_ms);
    TEST_ASSERT_EQUAL_STRING("/diag", snapshot.stats.last_uri.c_str());
}

void test_stream_state_reset_clears_counters_and_priority() {
    WebStreamState state;

    state.beginTransfer(10);
    state.noteMqttConnectDeferred();
    state.recordStreamResult("/x", 10, 10, true, StreamAbortReason::None, 0, 0, 200);
    state.reset();

    const WebTransferSnapshot snapshot = state.snapshot(11);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.stats.ok_count);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.stats.mqtt_connect_deferred_count);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.active_transfers);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.mqtt_pause_remaining_ms);
    TEST_ASSERT_EQUAL_STRING("", snapshot.stats.last_uri.c_str());
}

WebStreamFailure failure_at(uint32_t now) {
    WebStreamFailure failure;
    failure.first_ms = failure.last_ms = now;
    failure.reason = StreamAbortReason::ConnectionReset;
    failure.sent = 512;
    failure.total = 6365;
    failure.environment.internal_free = 70000;
    failure.environment.memory_valid = true;
    failure.environment.rssi = -60;
    web_stream_copy_text(failure.request.path, sizeof(failure.request.path), "/");
    return failure;
}

void test_failure_ring_retains_order_capacity_and_counts_across_success() {
    WebStreamState state;
    for (uint32_t i = 0; i < 7; ++i) {
        auto failure = failure_at(i * 10);
        failure.sent += i;
        TEST_ASSERT_TRUE(state.recordFailure(failure));
    }
    state.recordStreamResult("/ok", 10, 10, true, StreamAbortReason::None, 1, 0, 200);
    auto snapshot = state.snapshot(80);
    TEST_ASSERT_EQUAL_UINT32(4, snapshot.failure_count);
    TEST_ASSERT_EQUAL_UINT32(518, snapshot.failures[0].sent);
    TEST_ASSERT_EQUAL_UINT32(515, snapshot.failures[3].sent);
    auto recurrence = failure_at(90); recurrence.sent = 518;
    TEST_ASSERT_TRUE(state.recordFailure(recurrence));
    TEST_ASSERT_EQUAL_UINT32(1, recurrence.count);
    state.reset();
    TEST_ASSERT_EQUAL_UINT32(0, state.snapshot(100).failure_count);
}

void test_repeated_failures_count_and_log_periodically_across_clock_wrap() {
    WebStreamState state;
    auto first = failure_at(UINT32_MAX - 1000);
    TEST_ASSERT_TRUE(state.recordFailure(first));
    auto second = failure_at(1000);
    TEST_ASSERT_FALSE(state.recordFailure(second));
    TEST_ASSERT_EQUAL_UINT32(2, second.count);
    TEST_ASSERT_EQUAL_UINT32(first.first_ms, second.first_ms);
    auto third = failure_at(60000);
    TEST_ASSERT_TRUE(state.recordFailure(third));
    TEST_ASSERT_EQUAL_UINT32(3, third.count);
    const auto snapshot = state.snapshot(60000);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.failure_count);
    TEST_ASSERT_EQUAL_UINT32(1, snapshot.suppressed_failure_logs);
}

void test_network_memory_progress_and_client_changes_are_not_suppressed() {
    WebStreamState state;
    auto first = failure_at(0); state.recordFailure(first);
    auto network = failure_at(1); network.environment.mqtt_connected = true;
    TEST_ASSERT_TRUE(state.recordFailure(network));
    auto progress = network; progress.sent = 0;
    TEST_ASSERT_TRUE(state.recordFailure(progress));
    auto client = progress; web_stream_copy_text(client.request.peer, sizeof(client.request.peer), "127.0.0.1");
    TEST_ASSERT_TRUE(state.recordFailure(client));
    auto memory = client; memory.environment.internal_free -= 9000;
    TEST_ASSERT_TRUE(state.recordFailure(memory));
    auto low_memory = memory; low_memory.environment.internal_free = 32000;
    TEST_ASSERT_TRUE(state.recordFailure(low_memory));
}

void test_gradual_memory_drop_is_compared_to_first_event_in_group() {
    WebStreamState state;
    auto first = failure_at(0); state.recordFailure(first);
    auto second = failure_at(1); second.environment.internal_free -= 5000;
    TEST_ASSERT_FALSE(state.recordFailure(second));
    auto third = failure_at(2); third.environment.internal_free -= 10000;
    TEST_ASSERT_TRUE(state.recordFailure(third));
    TEST_ASSERT_EQUAL_UINT32(2, state.snapshot(2).failure_count);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_failure_ring_retains_order_capacity_and_counts_across_success);
    RUN_TEST(test_repeated_failures_count_and_log_periodically_across_clock_wrap);
    RUN_TEST(test_network_memory_progress_and_client_changes_are_not_suppressed);
    RUN_TEST(test_gradual_memory_drop_is_compared_to_first_event_in_group);
    RUN_TEST(test_stream_state_tracks_transfer_pause_window);
    RUN_TEST(test_stream_state_shell_priority_uses_recent_sta_window_and_does_not_shorten);
    RUN_TEST(test_stream_state_snapshot_includes_stats_and_deferred_counts);
    RUN_TEST(test_stream_state_reset_clears_counters_and_priority);
    return UNITY_END();
}
