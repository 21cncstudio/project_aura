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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_stream_state_tracks_transfer_pause_window);
    RUN_TEST(test_stream_state_shell_priority_uses_recent_sta_window_and_does_not_shorten);
    RUN_TEST(test_stream_state_snapshot_includes_stats_and_deferred_counts);
    RUN_TEST(test_stream_state_reset_clears_counters_and_priority);
    return UNITY_END();
}
