#include <unity.h>

#include "modules/StorageManager.h"

void setUp() {
    StorageManager::setTestForceSaveFailure(false);
}

void tearDown() {
    StorageManager::setTestForceSaveFailure(false);
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
    return UNITY_END();
}
