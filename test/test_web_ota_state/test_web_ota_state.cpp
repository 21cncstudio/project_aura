#include <unity.h>

#include "web/WebOtaState.h"

void setUp() {}
void tearDown() {}

void test_web_ota_state_begin_upload_resets_previous_state() {
    WebOtaState state;
    state.beginUpload(10);
    state.setSlotSize(1024);
    state.setExpectedSize(true, 512);
    state.addWritten(100);
    state.setErrorOnce("fail");

    state.beginUpload(200);
    const WebOtaSnapshot snapshot = state.snapshot();

    TEST_ASSERT_TRUE(snapshot.upload_seen);
    TEST_ASSERT_TRUE(snapshot.active);
    TEST_ASSERT_FALSE(snapshot.success);
    TEST_ASSERT_FALSE(snapshot.size_known);
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
    state.setErrorOnce("first");
    state.setErrorOnce("second");

    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_FALSE(snapshot.active);
    TEST_ASSERT_FALSE(snapshot.success);
    TEST_ASSERT_EQUAL_STRING("first", snapshot.error.c_str());
}

void test_web_ota_state_success_and_expected_size_match() {
    WebOtaState state;
    state.beginUpload(50);
    state.setExpectedSize(true, 256);
    state.addWritten(256);
    TEST_ASSERT_TRUE(state.writtenMatchesExpected());

    state.markFinalizeDuration(12);
    state.markSuccess();
    const WebOtaSnapshot snapshot = state.snapshot();
    TEST_ASSERT_FALSE(snapshot.active);
    TEST_ASSERT_TRUE(snapshot.success);
    TEST_ASSERT_EQUAL_UINT32(12, snapshot.finalize_ms);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_ota_state_begin_upload_resets_previous_state);
    RUN_TEST(test_web_ota_state_tracks_chunks_and_sizes);
    RUN_TEST(test_web_ota_state_error_is_sticky_and_clears_active);
    RUN_TEST(test_web_ota_state_success_and_expected_size_match);
    return UNITY_END();
}
