#include <unity.h>

#include "web/WebOtaState.h"

void setUp() {}
void tearDown() {}

void test_web_ota_state_begin_upload_resets_previous_state() {
    WebOtaState state;
    state.beginUpload(10);
    TEST_ASSERT_TRUE(state.isBusy());
    state.setSlotSize(1024);
    state.setExpectedSize(true, 512);
    state.addWritten(100);
    state.setErrorOnce("fail", 20);

    state.beginUpload(200);
    const WebOtaSnapshot snapshot = state.snapshot();

    TEST_ASSERT_TRUE(snapshot.upload_seen);
    TEST_ASSERT_TRUE(snapshot.active);
    TEST_ASSERT_TRUE(state.isBusy());
    TEST_ASSERT_FALSE(snapshot.success);
    TEST_ASSERT_FALSE(snapshot.reboot_pending);
    TEST_ASSERT_FALSE(snapshot.size_known);
    TEST_ASSERT_TRUE(snapshot.session_id != 0);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(snapshot.written_size));
    TEST_ASSERT_EQUAL_UINT32(200, snapshot.upload_start_ms);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(snapshot.error.length()));
}

void test_web_ota_state_tracks_chunks_and_sizes() {
    WebOtaState state;
    state.beginUpload(100);
    state.setSlotSize(1000);

    TEST_ASSERT_TRUE(state.noteChunk(120, 130));
    state.addWritten(120);
    TEST_ASSERT_FALSE(state.noteChunk(300, 180));
    state.addWritten(300);

    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_TRUE(snapshot.first_chunk_seen);
    TEST_ASSERT_EQUAL_UINT32(30, snapshot.firstChunkDelayMs());
    TEST_ASSERT_EQUAL_UINT32(2, snapshot.chunk_count);
    TEST_ASSERT_EQUAL_UINT32(120, static_cast<uint32_t>(snapshot.chunk_min_size));
    TEST_ASSERT_EQUAL_UINT32(300, static_cast<uint32_t>(snapshot.chunk_max_size));
    TEST_ASSERT_EQUAL_UINT32(210, static_cast<uint32_t>(snapshot.avgChunkSize()));
    TEST_ASSERT_EQUAL_UINT32(50, snapshot.transferPhaseMs());
    TEST_ASSERT_FALSE(state.wouldExceedSlot(400));
    TEST_ASSERT_TRUE(state.wouldExceedSlot(700));
}

void test_web_ota_state_error_is_sticky_and_clears_active() {
    WebOtaState state;
    state.beginUpload(10);
    TEST_ASSERT_FALSE(state.hasError());
    state.setErrorOnce("first", 40);
    state.setErrorOnce("second", 50);

    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_FALSE(snapshot.active);
    TEST_ASSERT_TRUE(state.isBusy());
    TEST_ASSERT_TRUE(state.hasError());
    TEST_ASSERT_FALSE(snapshot.success);
    TEST_ASSERT_TRUE(snapshot.hasTerminalResult(60));
    TEST_ASSERT_EQUAL_STRING("first", snapshot.error.c_str());

    state.clearBusy();
    TEST_ASSERT_FALSE(state.isBusy());

    state.reset();
    TEST_ASSERT_FALSE(state.isBusy());
    TEST_ASSERT_FALSE(state.hasError());
}

void test_web_ota_state_success_and_expected_size_match() {
    WebOtaState state;
    state.beginUpload(50);
    state.setExpectedSize(true, 256);
    state.addWritten(256);
    TEST_ASSERT_TRUE(state.writtenMatchesExpected());

    state.markFinalizeDuration(12);
    state.markSuccess(90);
    state.markRebootPending();
    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_FALSE(snapshot.active);
    TEST_ASSERT_TRUE(state.isBusy());
    TEST_ASSERT_TRUE(snapshot.success);
    TEST_ASSERT_TRUE(snapshot.reboot_pending);
    TEST_ASSERT_TRUE(snapshot.hasTerminalResult(100));
    TEST_ASSERT_EQUAL_UINT32(12, snapshot.finalize_ms);

    state.clearBusy();
    TEST_ASSERT_FALSE(state.isBusy());

    state.reset();
    TEST_ASSERT_FALSE(state.isBusy());
}

void test_web_ota_state_total_timeout_expires_from_upload_start() {
    WebOtaState state;
    state.beginUpload(100);
    state.setTotalTimeoutMs(250);

    TEST_ASSERT_FALSE(state.totalTimeoutExceeded(349));
    TEST_ASSERT_TRUE(state.totalTimeoutExceeded(350));
}

void test_web_ota_state_terminal_result_expires_after_ttl() {
    WebOtaState state;
    state.beginUpload(100);
    state.setErrorOnce("timeout", 150);
    TEST_ASSERT_TRUE(state.snapshot().hasTerminalResult(200));

    state.poll(150 + WebOtaState::terminalResultTtlMs() - 1);
    TEST_ASSERT_TRUE(state.snapshot().hasTerminalResult(150 + WebOtaState::terminalResultTtlMs() - 1));

    state.poll(150 + WebOtaState::terminalResultTtlMs());
    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_FALSE(snapshot.upload_seen);
    TEST_ASSERT_EQUAL_UINT32(0, snapshot.session_id);
    TEST_ASSERT_FALSE(snapshot.hasTerminalResult(150 + WebOtaState::terminalResultTtlMs()));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_ota_state_begin_upload_resets_previous_state);
    RUN_TEST(test_web_ota_state_tracks_chunks_and_sizes);
    RUN_TEST(test_web_ota_state_error_is_sticky_and_clears_active);
    RUN_TEST(test_web_ota_state_success_and_expected_size_match);
    RUN_TEST(test_web_ota_state_total_timeout_expires_from_upload_start);
    RUN_TEST(test_web_ota_state_terminal_result_expires_after_ttl);
    return UNITY_END();
}
