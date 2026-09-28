#include <unity.h>
#include <errno.h>
#include <thread>
#include <string>
#include <functional>
#include "core/WakePowerGuard.h"
#include "ArduinoMock.h"
#include "modules/SdCardManager.h"

struct Backend {
    std::string calls;
    bool mount_ok = true, unmount_ok = true;
    uint32_t serial = 1;
    bool history_present = false;
    std::function<void()> on_mount;
    static bool mount(void *ctx) {
        auto &b = *static_cast<Backend *>(ctx);
        b.calls += 'M';
        if (b.on_mount) b.on_mount();
        return b.mount_ok;
    }
    static uint32_t cardSerial(void *ctx) { return static_cast<Backend *>(ctx)->serial; }
    static bool historyPresent(void *ctx) { return static_cast<Backend *>(ctx)->history_present; }
    static bool unmount(void *ctx) { auto &b = *static_cast<Backend *>(ctx); b.calls += 'U'; return b.unmount_ok; }
    void attach(SdCardManager &sd) { sd.setTestBackend({mount, unmount, this, cardSerial, historyPresent}); }
};
void setUp() { setMillis(0); WakePowerGuard::resetForTest(); }
void tearDown() { TEST_ASSERT_EQUAL_UINT32(0, WakePowerGuard::activeOperations()); WakePowerGuard::resetForTest(); }

void test_reconnect_waits_for_csv_lease_and_gates_new_access() {
    Backend b; SdCardManager sd; b.attach(sd);
    TEST_ASSERT_TRUE(sd.begin(nullptr));
    TEST_ASSERT_TRUE(sd.acquireFileAccess(0));
    TEST_ASSERT_TRUE(sd.requestRecovery());
    sd.poll(millis(), false);
    std::thread worker([&] { sd.runRecoveryForTest(); }); worker.join();
    TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_TRUE(sd.status().recovery_pending);
    TEST_ASSERT_EQUAL_UINT32(0, sd.status().recovery_attempts);
    sd.releaseFileAccess();
    TEST_ASSERT_FALSE(sd.acquireFileAccess(0));
    advanceMillis(1000);
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
    TEST_ASSERT_TRUE(sd.isReady());
    TEST_ASSERT_TRUE(sd.takeRecoverySuccess());
    TEST_ASSERT_FALSE(sd.takeRecoverySuccess());
    TEST_ASSERT_TRUE(sd.status().write_verification_pending);
    sd.noteStreamReadSuccess();
    TEST_ASSERT_TRUE(sd.status().write_verification_pending);
}

void test_two_io_errors_schedule_recovery_and_preserve_first_cause() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr);
    sd.noteStreamReadFailure("read", ENODEV);
    TEST_ASSERT_FALSE(sd.status().recovery_pending);
    sd.noteStreamReadFailure("close", EIO);
    auto status = sd.status();
    TEST_ASSERT_TRUE(status.recovery_pending);
    TEST_ASSERT_EQUAL_INT(ENODEV, status.first_error_code);
    TEST_ASSERT_EQUAL_STRING("read", status.first_error_stage);
    sd.poll(millis(), false);
    TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
    advanceMillis(5000);
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
}

void test_no_space_readonly_and_busy_do_not_unmount() {
    for (int code : {ENOSPC, EACCES, EBUSY, ENOENT}) {
        Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr);
        sd.noteStreamReadFailure("read", code); sd.noteStreamReadFailure("read", code);
        TEST_ASSERT_FALSE(sd.status().recovery_pending);
        TEST_ASSERT_TRUE(sd.isReady());
    }
}

void test_failed_initial_mount_retries_without_reboot() {
    Backend b; b.mount_ok=false; SdCardManager sd; b.attach(sd);
    TEST_ASSERT_FALSE(sd.begin(nullptr));
    TEST_ASSERT_EQUAL_UINT32(5000, sd.status().recovery_retry_ms);
    advanceMillis(5000); b.mount_ok=true;
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MM", b.calls.c_str());
    TEST_ASSERT_TRUE(sd.isReady());
}

void test_ota_pause_retains_pending_request() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr); sd.requestRecovery();
    sd.poll(millis(), true); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
    TEST_ASSERT_TRUE(sd.status().recovery_pending);
    TEST_ASSERT_FALSE(sd.requestRecovery());
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
}

void test_cleanup_failure_stops_reinitialization() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr); b.unmount_ok=false;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MU", b.calls.c_str());
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_TRUE(sd.status().recovery_blocked);
    TEST_ASSERT_FALSE(sd.requestRecovery());
    advanceMillis(300000); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MU", b.calls.c_str());
}

void test_pending_recovery_can_be_drained_for_shutdown() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr); sd.requestRecovery();
    TEST_ASSERT_TRUE(sd.end());
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_EQUAL_STRING("MU", b.calls.c_str());
}

void test_pause_after_task_dispatch_defers_without_touching_card() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr); sd.requestRecovery();
    sd.poll(millis(), false);
    sd.pauseRecovery(); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
    TEST_ASSERT_TRUE(sd.status().recovery_pending);
    TEST_ASSERT_EQUAL_UINT32(0, sd.status().recovery_attempts);
    advanceMillis(1000); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
}

void test_shutdown_allows_final_flush_only_for_healthy_media() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr);
    sd.requestRecovery();
    TEST_ASSERT_FALSE(sd.isReady());
    sd.prepareForShutdown();
    TEST_ASSERT_TRUE(sd.isReady());
    TEST_ASSERT_FALSE(sd.status().recovery_pending);
    sd.noteStreamReadFailure("read", ENODEV);
    sd.noteStreamReadFailure("close", EIO);
    sd.prepareForShutdown();
    TEST_ASSERT_FALSE(sd.isReady());
}


void enter_wake_phase(WakePowerGuard::Phase phase) {
    using WakePowerGuard::Phase;
    WakePowerGuard::resetForTest();
    if (phase == Phase::FailClosed) { WakePowerGuard::latchFailClosed(); return; }
    WakePowerGuard::request(millis());
    if (phase == Phase::PreQuiet) return;
    (void)WakePowerGuard::evaluateSwitch(millis(), 100, 500);
    advanceMillis(100);
    TEST_ASSERT_TRUE(WakePowerGuard::evaluateSwitch(millis(), 100, 500).ready);
    TEST_ASSERT_TRUE(WakePowerGuard::beginSwitch());
    if (phase == Phase::Switching) return;
    WakePowerGuard::beginSettle(millis(), 200);
    if (phase == Phase::Settle) return;
    advanceMillis(200);
    TEST_ASSERT_TRUE(WakePowerGuard::beginRenderWait(millis()));
}

void test_wake_phases_pause_dispatch_without_touching_card() {
    using WakePowerGuard::Phase;
    for (Phase phase : {Phase::PreQuiet, Phase::Switching, Phase::Settle,
                        Phase::RenderWait, Phase::FailClosed}) {
        Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr);
        TEST_ASSERT_TRUE(sd.requestRecovery());
        enter_wake_phase(phase);
        sd.poll(millis(), false); sd.runRecoveryForTest();
        TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
        TEST_ASSERT_TRUE(sd.status().recovery_paused);
        TEST_ASSERT_TRUE(sd.status().recovery_pending);
        TEST_ASSERT_EQUAL_UINT32(0, sd.status().recovery_attempts);
        WakePowerGuard::cancel();
        sd.poll(millis(), false); sd.runRecoveryForTest();
        TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
    }
}

void test_wake_after_dispatch_defers_recovery_until_guard_releases() {
    Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr); sd.requestRecovery();
    sd.poll(millis(), false);
    enter_wake_phase(WakePowerGuard::Phase::Settle);
    sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("M", b.calls.c_str());
    TEST_ASSERT_TRUE(sd.status().recovery_pending);
    TEST_ASSERT_EQUAL_UINT32(0, sd.status().recovery_attempts);
    TEST_ASSERT_EQUAL_UINT32(0, WakePowerGuard::activeOperations());
    WakePowerGuard::cancel(); advanceMillis(1000);
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_EQUAL_STRING("MUM", b.calls.c_str());
}

void test_started_recovery_holds_wake_activity_until_mount_finishes() {
    for (bool success : {false, true}) {
        WakePowerGuard::resetForTest();
        Backend b; SdCardManager sd; b.attach(sd); sd.begin(nullptr);
        uint32_t active_during_mount = 0;
        bool switch_allowed_during_mount = false;
        b.mount_ok = success;
        b.on_mount = [&] {
            active_during_mount = WakePowerGuard::activeOperations();
            WakePowerGuard::request(millis());
            (void)WakePowerGuard::evaluateSwitch(millis(), 100, 500);
            advanceMillis(1000);
            switch_allowed_during_mount = WakePowerGuard::evaluateSwitch(millis(), 100, 500).ready;
        };
        sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
        TEST_ASSERT_EQUAL_UINT32(1, active_during_mount);
        TEST_ASSERT_FALSE(switch_allowed_during_mount);
        TEST_ASSERT_EQUAL_UINT32(0, WakePowerGuard::activeOperations());
        TEST_ASSERT_FALSE(WakePowerGuard::evaluateSwitch(millis(), 100, 500).ready);
        advanceMillis(100);
        TEST_ASSERT_TRUE(WakePowerGuard::evaluateSwitch(millis(), 100, 500).ready);
        TEST_ASSERT_TRUE(WakePowerGuard::beginSwitch());
        WakePowerGuard::cancel();
    }
}

void test_late_blank_card_stays_bound_before_first_successful_write() {
    Backend b; b.mount_ok = false;
    SdCardManager sd; b.attach(sd); TEST_ASSERT_FALSE(sd.begin(nullptr));
    b.mount_ok = true;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_TRUE(sd.isReady());
    TEST_ASSERT_FALSE(sd.status().has_successful_write);
    // A failed first save can leave our new history directory before committing.
    b.history_present = true;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_TRUE(sd.isReady());
    TEST_ASSERT_FALSE(sd.status().recovery_blocked);
    TEST_ASSERT_TRUE(sd.status().write_verification_pending);
    TEST_ASSERT_FALSE(sd.status().has_successful_write);
    TEST_ASSERT_EQUAL_UINT32(2, sd.status().recovery_successes);
}

void test_late_card_with_existing_history_is_still_blocked() {
    Backend b; b.mount_ok = false;
    SdCardManager sd; b.attach(sd); sd.begin(nullptr);
    b.mount_ok = true; b.history_present = true;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_TRUE(sd.status().recovery_blocked);
    TEST_ASSERT_FALSE(sd.takeRecoverySuccess());
}

void test_unrestored_boot_history_is_still_blocked_on_same_card() {
    Backend b; b.history_present = true;
    SdCardManager sd; b.attach(sd); TEST_ASSERT_TRUE(sd.begin(nullptr));
    // No restore acknowledgement: the boot-time snapshot read did not complete.
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_TRUE(sd.status().recovery_blocked);
}

void test_blank_acceptance_cannot_authorize_a_different_card() {
    Backend b; b.mount_ok = false;
    SdCardManager sd; b.attach(sd); sd.begin(nullptr);
    b.mount_ok = true;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_TRUE(sd.isReady());
    b.serial = 2;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_FALSE(sd.isReady());
    TEST_ASSERT_TRUE(sd.status().recovery_blocked);
    b.serial = 1; b.history_present = true;
    sd.requestRecovery(); sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_TRUE(sd.isReady());
}

void test_manual_reconnect_overrides_five_minute_backoff() {
    Backend b; b.mount_ok = false;
    SdCardManager sd; b.attach(sd); sd.begin(nullptr);
    for (uint32_t delay : {5000U, 30000U, 60000U}) {
        advanceMillis(delay); sd.poll(millis(), false); sd.runRecoveryForTest();
    }
    TEST_ASSERT_EQUAL_UINT32(300000, sd.status().recovery_retry_ms);
    TEST_ASSERT_TRUE(sd.status().can_reconnect);
    b.mount_ok = true;
    TEST_ASSERT_TRUE(sd.requestRecovery());
    TEST_ASSERT_EQUAL_UINT32(0, sd.status().recovery_retry_ms);
    sd.poll(millis(), false); sd.runRecoveryForTest();
    TEST_ASSERT_TRUE(sd.isReady());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_wake_phases_pause_dispatch_without_touching_card);
    RUN_TEST(test_wake_after_dispatch_defers_recovery_until_guard_releases);
    RUN_TEST(test_started_recovery_holds_wake_activity_until_mount_finishes);
    RUN_TEST(test_late_blank_card_stays_bound_before_first_successful_write);
    RUN_TEST(test_late_card_with_existing_history_is_still_blocked);
    RUN_TEST(test_unrestored_boot_history_is_still_blocked_on_same_card);
    RUN_TEST(test_blank_acceptance_cannot_authorize_a_different_card);
    RUN_TEST(test_manual_reconnect_overrides_five_minute_backoff);
    RUN_TEST(test_reconnect_waits_for_csv_lease_and_gates_new_access);
    RUN_TEST(test_two_io_errors_schedule_recovery_and_preserve_first_cause);
    RUN_TEST(test_no_space_readonly_and_busy_do_not_unmount);
    RUN_TEST(test_failed_initial_mount_retries_without_reboot);
    RUN_TEST(test_ota_pause_retains_pending_request);
    RUN_TEST(test_cleanup_failure_stops_reinitialization);
    RUN_TEST(test_pending_recovery_can_be_drained_for_shutdown);
    RUN_TEST(test_pause_after_task_dispatch_defers_without_touching_card);
    RUN_TEST(test_shutdown_allows_final_flush_only_for_healthy_media);
    return UNITY_END();
}
