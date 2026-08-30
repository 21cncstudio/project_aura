#include <unity.h>

#include "ArduinoMock.h"
#include "modules/StorageManager.h"

namespace {

void saveCandidate(StorageManager &storage, const char *ssid) {
    storage.config().wifi_ssid = ssid;
    TEST_ASSERT_TRUE(storage.saveConfig(true));
}

void observeHealthy(StorageManager &storage, uint32_t now_ms) {
    storage.observeLastGoodHealth(now_ms, OperationalHealth::Healthy);
}

void accumulateHealthy(StorageManager &storage,
                       uint32_t &now_ms,
                       uint32_t duration_ms) {
    while (duration_ms > 0) {
        const uint32_t step_ms =
            duration_ms > Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS
                ? Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS
                : duration_ms;
        now_ms += step_ms;
        observeHealthy(storage, now_ms);
        duration_ms -= step_ms;
    }
}

bool loadStoredConfig(const char *path, Config::StoredConfig &out) {
    return StorageManager::getTestStoredConfig(path, out);
}

} // namespace

void setUp() {
    setMillis(0);
    StorageManager::resetTestPersistence();
    StorageManager::setTestForceSaveFailure(false);
    StorageManager::setTestLastGoodCommitFailureCount(0);
}

void tearDown() {
    StorageManager::setTestForceSaveFailure(false);
    StorageManager::setTestLastGoodCommitFailureCount(0);
}

void test_save_wifi_settings_preserves_spaces() {
    StorageManager storage;
    storage.begin();

    TEST_ASSERT_TRUE(storage.saveWiFiSettings("  My SSID  ", "  My Pass  ", true));
    TEST_ASSERT_EQUAL_STRING("  My SSID  ", storage.config().wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("  My Pass  ", storage.config().wifi_pass.c_str());
    TEST_ASSERT_TRUE(storage.config().wifi_enabled);
}

void test_save_wifi_settings_rolls_back_on_failure() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveWiFiSettings("old-ssid", "old-pass", true));

    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveWiFiSettings("new-ssid", "new-pass", true));

    TEST_ASSERT_EQUAL_STRING("old-ssid", storage.config().wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("old-pass", storage.config().wifi_pass.c_str());
    TEST_ASSERT_TRUE(storage.config().wifi_enabled);
}

void test_save_wifi_enterprise_settings_persists_certs() {
    StorageManager storage;
    storage.begin();

    Config::WifiSettings settings{};
    settings.ssid = "CorpNet";
    settings.enabled = true;
    settings.auth_mode = Config::WifiAuthMode::Enterprise;
    settings.eap_method = Config::WifiEapMethod::Ttls;
    settings.ttls_phase2 = Config::WifiTtlsPhase2::Pap;
    settings.identity = "outer";
    settings.username = "alice";
    settings.enterprise_password = "secret";
    settings.ca_cert_pem = "ca";
    settings.client_cert_pem = "client";
    settings.client_key_pem = "key";

    TEST_ASSERT_TRUE(storage.saveWiFiSettings(settings));
    TEST_ASSERT_EQUAL_STRING("CorpNet", storage.config().wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiAuthMode::Enterprise),
                          static_cast<int>(storage.config().wifi_auth_mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Ttls),
                          static_cast<int>(storage.config().wifi_eap_method));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiTtlsPhase2::Pap),
                          static_cast<int>(storage.config().wifi_ttls_phase2));
    TEST_ASSERT_EQUAL_STRING("outer", storage.config().wifi_identity.c_str());

    Config::WifiSettings loaded{};
    storage.loadWiFiSettings(loaded);
    TEST_ASSERT_EQUAL_STRING("ca", loaded.ca_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("client", loaded.client_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("key", loaded.client_key_pem.c_str());
}

void test_save_wifi_enterprise_settings_rolls_back_config_and_certs_on_failure() {
    StorageManager storage;
    storage.begin();

    Config::WifiSettings old_settings{};
    old_settings.ssid = "OldCorp";
    old_settings.auth_mode = Config::WifiAuthMode::Enterprise;
    old_settings.eap_method = Config::WifiEapMethod::Peap;
    old_settings.identity = "old-id";
    old_settings.username = "old-user";
    old_settings.enterprise_password = "old-pass";
    old_settings.ca_cert_pem = "old-ca";
    TEST_ASSERT_TRUE(storage.saveWiFiSettings(old_settings));

    Config::WifiSettings new_settings{};
    new_settings.ssid = "NewCorp";
    new_settings.auth_mode = Config::WifiAuthMode::Enterprise;
    new_settings.eap_method = Config::WifiEapMethod::Tls;
    new_settings.identity = "new-id";
    new_settings.client_cert_pem = "new-client";
    new_settings.client_key_pem = "new-key";
    new_settings.ca_cert_pem = "new-ca";

    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveWiFiSettings(new_settings));

    TEST_ASSERT_EQUAL_STRING("OldCorp", storage.config().wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Peap),
                          static_cast<int>(storage.config().wifi_eap_method));
    TEST_ASSERT_EQUAL_STRING("old-id", storage.config().wifi_identity.c_str());
    Config::WifiSettings loaded{};
    storage.loadWiFiSettings(loaded);
    TEST_ASSERT_EQUAL_STRING("old-ca", loaded.ca_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("", loaded.client_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("", loaded.client_key_pem.c_str());
}

void test_clear_wifi_credentials_rolls_back_config_and_certs_on_failure() {
    StorageManager storage;
    storage.begin();

    Config::WifiSettings old_settings{};
    old_settings.ssid = "CorpNet";
    old_settings.enabled = true;
    old_settings.auth_mode = Config::WifiAuthMode::Enterprise;
    old_settings.eap_method = Config::WifiEapMethod::Tls;
    old_settings.identity = "device-1";
    old_settings.client_cert_pem = "old-client";
    old_settings.client_key_pem = "old-key";
    old_settings.ca_cert_pem = "old-ca";
    TEST_ASSERT_TRUE(storage.saveWiFiSettings(old_settings));

    StorageManager::setTestForceSaveFailure(true);
    storage.clearWiFiCredentials();

    TEST_ASSERT_EQUAL_STRING("CorpNet", storage.config().wifi_ssid.c_str());
    TEST_ASSERT_TRUE(storage.config().wifi_enabled);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiAuthMode::Enterprise),
                          static_cast<int>(storage.config().wifi_auth_mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Tls),
                          static_cast<int>(storage.config().wifi_eap_method));
    TEST_ASSERT_EQUAL_STRING("device-1", storage.config().wifi_identity.c_str());
    Config::WifiSettings loaded{};
    storage.loadWiFiSettings(loaded);
    TEST_ASSERT_EQUAL_STRING("old-ca", loaded.ca_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("old-client", loaded.client_cert_pem.c_str());
    TEST_ASSERT_EQUAL_STRING("old-key", loaded.client_key_pem.c_str());
}

void test_save_mqtt_settings_preserves_spaces() {
    StorageManager storage;
    storage.begin();

    TEST_ASSERT_TRUE(storage.saveMqttSettings("broker.local", 1883, "  user  ", "  pass  ",
                                              "base/topic", "Device", true, false, false, ""));
    TEST_ASSERT_EQUAL_STRING("broker.local", storage.config().mqtt_host.c_str());
    TEST_ASSERT_EQUAL_UINT16(1883, storage.config().mqtt_port);
    TEST_ASSERT_EQUAL_STRING("  user  ", storage.config().mqtt_user.c_str());
    TEST_ASSERT_EQUAL_STRING("  pass  ", storage.config().mqtt_pass.c_str());
    TEST_ASSERT_EQUAL_STRING("base/topic", storage.config().mqtt_base_topic.c_str());
    TEST_ASSERT_FALSE(storage.config().mqtt_tls_enabled);
}

void test_save_mqtt_settings_rolls_back_on_failure() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveMqttSettings("old-host", 1883, "old-user", "old-pass",
                                              "old/topic", "Old Device", false, true, true,
                                              "old-ca"));

    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveMqttSettings("new-host", 1884, "new-user", "new-pass",
                                               "new/topic", "New Device", true, false, false, ""));

    TEST_ASSERT_EQUAL_STRING("old-host", storage.config().mqtt_host.c_str());
    TEST_ASSERT_EQUAL_UINT16(1883, storage.config().mqtt_port);
    TEST_ASSERT_EQUAL_STRING("old-user", storage.config().mqtt_user.c_str());
    TEST_ASSERT_EQUAL_STRING("old-pass", storage.config().mqtt_pass.c_str());
    TEST_ASSERT_EQUAL_STRING("old/topic", storage.config().mqtt_base_topic.c_str());
    TEST_ASSERT_EQUAL_STRING("Old Device", storage.config().mqtt_device_name.c_str());
    TEST_ASSERT_FALSE(storage.config().mqtt_discovery);
    TEST_ASSERT_TRUE(storage.config().mqtt_anonymous);
    TEST_ASSERT_TRUE(storage.config().mqtt_tls_enabled);
    String ca;
    TEST_ASSERT_TRUE(storage.loadMqttCaCertificate(ca));
    TEST_ASSERT_EQUAL_STRING("old-ca", ca.c_str());
}

void test_save_mqtt_settings_persists_and_removes_ca_certificate() {
    StorageManager storage;
    storage.begin();

    const char *pem = "-----BEGIN CERTIFICATE-----\nabc\n-----END CERTIFICATE-----";
    TEST_ASSERT_TRUE(storage.saveMqttSettings("cloud.example.com", 8883, "user", "pass",
                                              "base/topic", "Device", true, false, true, pem));
    TEST_ASSERT_TRUE(storage.config().mqtt_tls_enabled);
    String ca;
    TEST_ASSERT_TRUE(storage.loadMqttCaCertificate(ca));
    TEST_ASSERT_EQUAL_STRING(pem, ca.c_str());

    TEST_ASSERT_TRUE(storage.saveMqttSettings("broker.local", 1883, "user", "pass",
                                              "base/topic", "Device", true, false, false, ""));
    TEST_ASSERT_FALSE(storage.config().mqtt_tls_enabled);
    TEST_ASSERT_FALSE(storage.loadMqttCaCertificate(ca));
}

void test_factory_reset_removes_mqtt_ca_certificate() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveMqttCaCertificate("ca"));

    StorageManager::preserveTestPersistenceForNextBegin();
    storage.begin(StorageManager::BootAction::SafeFactoryReset);

    String ca;
    TEST_ASSERT_FALSE(storage.loadMqttCaCertificate(ca));
}

void test_save_dac_auto_state_persists_mode_and_armed() {
    StorageManager storage;
    storage.begin();

    TEST_ASSERT_TRUE(storage.saveDacAutoState(true, true));
    TEST_ASSERT_TRUE(storage.config().dac_auto_mode);
    TEST_ASSERT_TRUE(storage.config().dac_auto_armed);

    TEST_ASSERT_TRUE(storage.saveDacAutoState(true, false));
    TEST_ASSERT_TRUE(storage.config().dac_auto_mode);
    TEST_ASSERT_FALSE(storage.config().dac_auto_armed);
}

void test_save_dac_auto_state_rolls_back_on_failure() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveDacAutoState(true, true));

    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveDacAutoState(false, false));

    TEST_ASSERT_TRUE(storage.config().dac_auto_mode);
    TEST_ASSERT_TRUE(storage.config().dac_auto_armed);
}

void test_save_rtc_mode_persists_selection() {
    StorageManager storage;
    storage.begin();

    TEST_ASSERT_TRUE(storage.saveRtcMode(Config::RtcMode::Ds3231));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Ds3231),
                          static_cast<int>(storage.config().rtc_mode));

    TEST_ASSERT_TRUE(storage.saveRtcMode(Config::RtcMode::Pcf8523));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Pcf8523),
                          static_cast<int>(storage.config().rtc_mode));
}

void test_save_rtc_mode_rolls_back_on_failure() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveRtcMode(Config::RtcMode::Pcf8523));

    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveRtcMode(Config::RtcMode::Ds3231));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::RtcMode::Pcf8523),
                          static_cast<int>(storage.config().rtc_mode));
}

void test_blob_load_recovers_interrupted_atomic_replace() {
    StorageManager storage;
    storage.begin();
    const uint8_t expected[] = {0x21, 0x43, 0x65, 0x87};
    TEST_ASSERT_TRUE(storage.saveBlobAtomic("/recover.bin.bak", expected, sizeof(expected)));

    uint8_t actual[sizeof(expected)] = {};
    TEST_ASSERT_TRUE(storage.loadBlob("/recover.bin", actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, sizeof(expected));
    TEST_ASSERT_TRUE(storage.blobExists("/recover.bin"));
    TEST_ASSERT_FALSE(storage.blobExists("/recover.bin.bak"));
}

void test_text_load_recovers_interrupted_atomic_replace() {
    StorageManager storage;
    storage.begin();
    TEST_ASSERT_TRUE(storage.saveTextAtomic("/recover.txt.bak", "recovered"));

    String actual;
    TEST_ASSERT_TRUE(storage.loadText("/recover.txt", actual));
    TEST_ASSERT_EQUAL_STRING("recovered", actual.c_str());
}

void test_remove_blob_cleans_atomic_artifacts() {
    StorageManager storage;
    storage.begin();
    const uint8_t value = 42;
    TEST_ASSERT_TRUE(storage.saveBlobAtomic("/cleanup.bin", &value, sizeof(value)));
    TEST_ASSERT_TRUE(storage.saveBlobAtomic("/cleanup.bin.tmp", &value, sizeof(value)));
    TEST_ASSERT_TRUE(storage.saveBlobAtomic("/cleanup.bin.bak", &value, sizeof(value)));

    TEST_ASSERT_TRUE(storage.removeBlob("/cleanup.bin"));
    TEST_ASSERT_FALSE(storage.blobExists("/cleanup.bin"));
    TEST_ASSERT_FALSE(storage.blobExists("/cleanup.bin.tmp"));
    TEST_ASSERT_FALSE(storage.blobExists("/cleanup.bin.bak"));
}

void test_new_save_supersedes_an_interrupted_backup() {
    StorageManager storage;
    storage.begin();
    const uint8_t previous = 21;
    const uint8_t current = 42;
    TEST_ASSERT_TRUE(storage.saveBlobAtomic(
        "/retry.bin.bak", &previous, sizeof(previous)));

    TEST_ASSERT_TRUE(storage.saveBlobAtomic("/retry.bin", &current, sizeof(current)));

    uint8_t actual = 0;
    TEST_ASSERT_TRUE(storage.loadBlob("/retry.bin", &actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_UINT8(current, actual);
    TEST_ASSERT_FALSE(storage.blobExists("/retry.bin.bak"));
}

void test_dirty_config_saves_while_unhealthy_without_promoting_last_good() {
    StorageManager storage;
    storage.begin();
    storage.config().wifi_ssid = "unhealthy-candidate";
    storage.requestSave();
    storage.observeLastGoodHealth(0, OperationalHealth::Unhealthy);

    setMillis(999);
    storage.poll(999);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kConfigPath, stored));

    setMillis(1000);
    storage.poll(1000);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kConfigPath, stored));
    TEST_ASSERT_EQUAL_STRING("unhealthy-candidate", stored.wifi_ssid.c_str());

    storage.observeLastGoodHealth(
        1000 + Config::LAST_GOOD_COMMIT_DELAY_MS,
        OperationalHealth::Unhealthy);
    storage.poll(1000 + Config::LAST_GOOD_COMMIT_DELAY_MS);
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_last_good_requires_exact_accumulated_healthy_dwell() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "exact-dwell");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("exact-dwell", stored.wifi_ssid.c_str());
}

void test_unhealthy_sample_resets_healthy_proof() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "reset-proof");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);
    ++now_ms;
    storage.observeLastGoodHealth(now_ms, OperationalHealth::Unhealthy);
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_unavailable_pause_freezes_proof_and_excludes_pause_time() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "paused-proof");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    constexpr uint32_t first_healthy_ms = 60UL * 1000UL;
    accumulateHealthy(storage, now_ms, first_healthy_ms);

    now_ms += 30UL * 1000UL;
    storage.observeLastGoodHealth(now_ms, OperationalHealth::Unavailable);
    now_ms += 60UL * 1000UL;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - first_healthy_ms - 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_health_sample_gap_boundary_counts_5000_but_excludes_5001() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "gap-boundary");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS);
    now_ms += Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS + 1U;
    observeHealthy(storage, now_ms);

    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS -
            Config::LAST_GOOD_HEALTH_SAMPLE_MAX_GAP_MS - 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_successful_new_save_restarts_health_proof() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "candidate-a");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);

    setMillis(now_ms);
    saveCandidate(storage, "candidate-b");
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_FALSE(loadStoredConfig(StorageManager::kLastGoodPath, stored));

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("candidate-b", stored.wifi_ssid.c_str());
}

void test_failed_config_save_keeps_previous_candidate_and_proof() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "durable-candidate");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(
        storage,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);

    storage.config().wifi_ssid = "unsaved-change";
    StorageManager::setTestForceSaveFailure(true);
    TEST_ASSERT_FALSE(storage.saveConfig(true));
    StorageManager::setTestForceSaveFailure(false);

    accumulateHealthy(storage, now_ms, 1U);
    storage.poll(now_ms);
    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("durable-candidate", stored.wifi_ssid.c_str());
}

void test_unhealthy_candidate_does_not_replace_existing_last_good() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "known-good");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    storage.poll(now_ms);

    setMillis(now_ms);
    saveCandidate(storage, "unhealthy-new-config");
    storage.observeLastGoodHealth(now_ms, OperationalHealth::Unhealthy);
    now_ms += Config::LAST_GOOD_COMMIT_DELAY_MS;
    storage.poll(now_ms);

    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());
}

void test_safe_rollback_loads_last_good_after_reboot() {
    StorageManager first_boot;
    first_boot.begin();
    saveCandidate(first_boot, "known-good");

    uint32_t now_ms = 100;
    observeHealthy(first_boot, now_ms);
    accumulateHealthy(first_boot, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    first_boot.poll(now_ms);

    setMillis(now_ms);
    saveCandidate(first_boot, "unproven-candidate");

    StorageManager::preserveTestPersistenceForNextBegin();
    StorageManager rollback_boot;
    rollback_boot.begin(StorageManager::BootAction::SafeRollback);
    TEST_ASSERT_TRUE(rollback_boot.isConfigLoaded());
    TEST_ASSERT_EQUAL_STRING(
        "known-good",
        rollback_boot.config().wifi_ssid.c_str());

    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kConfigPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());
}

void test_normal_reboot_loads_current_config_and_requires_fresh_health_proof() {
    StorageManager first_boot;
    first_boot.begin();
    saveCandidate(first_boot, "known-good");

    uint32_t now_ms = 100;
    observeHealthy(first_boot, now_ms);
    accumulateHealthy(first_boot, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    first_boot.poll(now_ms);

    setMillis(now_ms);
    saveCandidate(first_boot, "current-candidate");
    observeHealthy(first_boot, now_ms);
    accumulateHealthy(
        first_boot,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);

    StorageManager::preserveTestPersistenceForNextBegin();
    StorageManager normal_boot;
    normal_boot.begin(StorageManager::BootAction::Normal);
    TEST_ASSERT_TRUE(normal_boot.isConfigLoaded());
    TEST_ASSERT_EQUAL_STRING(
        "current-candidate",
        normal_boot.config().wifi_ssid.c_str());

    Config::StoredConfig stored;
    observeHealthy(normal_boot, now_ms);
    normal_boot.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());

    accumulateHealthy(
        normal_boot,
        now_ms,
        Config::LAST_GOOD_COMMIT_DELAY_MS - 1U);
    normal_boot.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());

    accumulateHealthy(normal_boot, now_ms, 1U);
    normal_boot.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("current-candidate", stored.wifi_ssid.c_str());
}

void test_failed_last_good_commit_retries_after_initial_backoff() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "retry-candidate");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    StorageManager::setTestLastGoodCommitFailureCount(1);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, StorageManager::testLastGoodCommitAttemptCount());

    now_ms += Config::LAST_GOOD_RETRY_INITIAL_DELAY_MS - 1U;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, StorageManager::testLastGoodCommitAttemptCount());

    ++now_ms;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(2, StorageManager::testLastGoodCommitAttemptCount());
    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_failed_promotion_preserves_previous_last_good_until_retry_succeeds() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "known-good");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    storage.poll(now_ms);

    setMillis(now_ms);
    saveCandidate(storage, "next-candidate");
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    StorageManager::setTestLastGoodCommitFailureCount(1);
    storage.poll(now_ms);

    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());

    now_ms += Config::LAST_GOOD_RETRY_INITIAL_DELAY_MS - 1U;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("known-good", stored.wifi_ssid.c_str());

    ++now_ms;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
    TEST_ASSERT_EQUAL_STRING("next-candidate", stored.wifi_ssid.c_str());
}

void test_last_good_retry_backoff_doubles_and_caps_at_five_minutes() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "backoff-candidate");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    StorageManager::setTestLastGoodCommitFailureCount(8);
    storage.poll(now_ms);

    const uint32_t retry_delays[] = {
        5UL * 1000UL,
        10UL * 1000UL,
        20UL * 1000UL,
        40UL * 1000UL,
        80UL * 1000UL,
        160UL * 1000UL,
        300UL * 1000UL,
        300UL * 1000UL,
    };
    uint32_t expected_attempts = 1;
    for (uint32_t retry_delay_ms : retry_delays) {
        now_ms += retry_delay_ms - 1U;
        observeHealthy(storage, now_ms);
        storage.poll(now_ms);
        TEST_ASSERT_EQUAL_UINT32(
            expected_attempts,
            StorageManager::testLastGoodCommitAttemptCount());

        ++now_ms;
        observeHealthy(storage, now_ms);
        storage.poll(now_ms);
        ++expected_attempts;
        TEST_ASSERT_EQUAL_UINT32(
            expected_attempts,
            StorageManager::testLastGoodCommitAttemptCount());
    }

    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_unavailable_health_suppresses_ready_retry_until_healthy_again() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "unavailable-retry");

    uint32_t now_ms = 100;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    StorageManager::setTestLastGoodCommitFailureCount(1);
    storage.poll(now_ms);

    now_ms += Config::LAST_GOOD_RETRY_INITIAL_DELAY_MS;
    storage.observeLastGoodHealth(now_ms, OperationalHealth::Unavailable);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, StorageManager::testLastGoodCommitAttemptCount());

    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(2, StorageManager::testLastGoodCommitAttemptCount());
}

void test_healthy_dwell_is_rollover_safe() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "rollover-dwell");

    uint32_t now_ms = UINT32_MAX - Config::LAST_GOOD_COMMIT_DELAY_MS + 100U;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    storage.poll(now_ms);

    Config::StoredConfig stored;
    TEST_ASSERT_TRUE(loadStoredConfig(StorageManager::kLastGoodPath, stored));
}

void test_last_good_retry_deadline_is_rollover_safe() {
    StorageManager storage;
    storage.begin();
    saveCandidate(storage, "rollover-retry");

    uint32_t now_ms =
        UINT32_MAX - Config::LAST_GOOD_COMMIT_DELAY_MS - 1000U;
    observeHealthy(storage, now_ms);
    accumulateHealthy(storage, now_ms, Config::LAST_GOOD_COMMIT_DELAY_MS);
    StorageManager::setTestLastGoodCommitFailureCount(1);
    storage.poll(now_ms);

    now_ms += Config::LAST_GOOD_RETRY_INITIAL_DELAY_MS - 1U;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(1, StorageManager::testLastGoodCommitAttemptCount());

    ++now_ms;
    observeHealthy(storage, now_ms);
    storage.poll(now_ms);
    TEST_ASSERT_EQUAL_UINT32(2, StorageManager::testLastGoodCommitAttemptCount());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_save_wifi_settings_preserves_spaces);
    RUN_TEST(test_save_wifi_settings_rolls_back_on_failure);
    RUN_TEST(test_save_wifi_enterprise_settings_persists_certs);
    RUN_TEST(test_save_wifi_enterprise_settings_rolls_back_config_and_certs_on_failure);
    RUN_TEST(test_clear_wifi_credentials_rolls_back_config_and_certs_on_failure);
    RUN_TEST(test_save_mqtt_settings_preserves_spaces);
    RUN_TEST(test_save_mqtt_settings_rolls_back_on_failure);
    RUN_TEST(test_save_mqtt_settings_persists_and_removes_ca_certificate);
    RUN_TEST(test_factory_reset_removes_mqtt_ca_certificate);
    RUN_TEST(test_save_dac_auto_state_persists_mode_and_armed);
    RUN_TEST(test_save_dac_auto_state_rolls_back_on_failure);
    RUN_TEST(test_save_rtc_mode_persists_selection);
    RUN_TEST(test_save_rtc_mode_rolls_back_on_failure);
    RUN_TEST(test_blob_load_recovers_interrupted_atomic_replace);
    RUN_TEST(test_text_load_recovers_interrupted_atomic_replace);
    RUN_TEST(test_remove_blob_cleans_atomic_artifacts);
    RUN_TEST(test_new_save_supersedes_an_interrupted_backup);
    RUN_TEST(test_dirty_config_saves_while_unhealthy_without_promoting_last_good);
    RUN_TEST(test_last_good_requires_exact_accumulated_healthy_dwell);
    RUN_TEST(test_unhealthy_sample_resets_healthy_proof);
    RUN_TEST(test_unavailable_pause_freezes_proof_and_excludes_pause_time);
    RUN_TEST(test_health_sample_gap_boundary_counts_5000_but_excludes_5001);
    RUN_TEST(test_successful_new_save_restarts_health_proof);
    RUN_TEST(test_failed_config_save_keeps_previous_candidate_and_proof);
    RUN_TEST(test_unhealthy_candidate_does_not_replace_existing_last_good);
    RUN_TEST(test_safe_rollback_loads_last_good_after_reboot);
    RUN_TEST(test_normal_reboot_loads_current_config_and_requires_fresh_health_proof);
    RUN_TEST(test_failed_last_good_commit_retries_after_initial_backoff);
    RUN_TEST(test_failed_promotion_preserves_previous_last_good_until_retry_succeeds);
    RUN_TEST(test_last_good_retry_backoff_doubles_and_caps_at_five_minutes);
    RUN_TEST(test_unavailable_health_suppresses_ready_retry_until_healthy_again);
    RUN_TEST(test_healthy_dwell_is_rollover_safe);
    RUN_TEST(test_last_good_retry_deadline_is_rollover_safe);
    return UNITY_END();
}
