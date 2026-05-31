#include <unity.h>

#include "web/WebWifiSaveUtils.h"

void setUp() {}
void tearDown() {}

void test_web_wifi_save_utils_parse_accepts_valid_input() {
    const WebWifiSaveUtils::ParseResult result =
        WebWifiSaveUtils::parseSaveInput("AuraNet", "secret");

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("AuraNet", result.update.settings.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", result.update.settings.pass.c_str());
    TEST_ASSERT_TRUE(result.update.settings.enabled);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiAuthMode::Personal),
                          static_cast<int>(result.update.settings.auth_mode));
}

void test_web_wifi_save_utils_parse_rejects_missing_ssid_and_bad_password() {
    WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput("", "secret");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("SSID required", result.error_message.c_str());

    result = WebWifiSaveUtils::parseSaveInput("AuraNet", "bad\npass");
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_UINT16(400, result.status_code);
    TEST_ASSERT_EQUAL_STRING("Password contains unsupported control characters",
                             result.error_message.c_str());
}

void test_web_wifi_save_utils_parse_accepts_peap_and_defaults_identity() {
    WebWifiSaveUtils::SaveInput input{};
    input.ssid = "CorpNet";
    input.auth_mode = "enterprise";
    input.eap_method = "peap";
    input.username = "alice";
    input.enterprise_password = "secret";

    const WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput(input);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiAuthMode::Enterprise),
                          static_cast<int>(result.update.settings.auth_mode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Peap),
                          static_cast<int>(result.update.settings.eap_method));
    TEST_ASSERT_EQUAL_STRING("alice", result.update.settings.identity.c_str());
    TEST_ASSERT_EQUAL_STRING("alice", result.update.settings.username.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", result.update.settings.enterprise_password.c_str());
}

void test_web_wifi_save_utils_parse_accepts_ttls_phase2() {
    WebWifiSaveUtils::SaveInput input{};
    input.ssid = "CorpNet";
    input.auth_mode = "enterprise";
    input.eap_method = "ttls";
    input.ttls_phase2 = "pap";
    input.identity = "outer";
    input.username = "alice";
    input.enterprise_password = "secret";

    const WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput(input);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Ttls),
                          static_cast<int>(result.update.settings.eap_method));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiTtlsPhase2::Pap),
                          static_cast<int>(result.update.settings.ttls_phase2));
    TEST_ASSERT_EQUAL_STRING("outer", result.update.settings.identity.c_str());
}

void test_web_wifi_save_utils_parse_accepts_tls_pem_fields() {
    WebWifiSaveUtils::SaveInput input{};
    input.ssid = "CorpNet";
    input.auth_mode = "enterprise";
    input.eap_method = "tls";
    input.identity = "device-1";
    input.ca_cert_pem = "-----BEGIN CERTIFICATE-----\r\nca\r\n-----END CERTIFICATE-----";
    input.client_cert_pem = "-----BEGIN CERTIFICATE-----\nclient\n-----END CERTIFICATE-----";
    input.client_key_pem = "-----BEGIN PRIVATE KEY-----\nkey\n-----END PRIVATE KEY-----";

    const WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput(input);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::WifiEapMethod::Tls),
                          static_cast<int>(result.update.settings.eap_method));
    TEST_ASSERT_EQUAL_STRING("device-1", result.update.settings.identity.c_str());
    TEST_ASSERT_EQUAL_STRING("-----BEGIN CERTIFICATE-----\nca\n-----END CERTIFICATE-----",
                             result.update.settings.ca_cert_pem.c_str());
}

void test_web_wifi_save_utils_parse_rejects_enterprise_missing_fields_and_long_values() {
    WebWifiSaveUtils::SaveInput input{};
    input.ssid = "CorpNet";
    input.auth_mode = "enterprise";
    input.eap_method = "peap";
    input.username = "alice";

    WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput(input);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Enterprise username and password required",
                             result.error_message.c_str());

    input.enterprise_password = "secret";
    input.identity = "12345678901234567890123456789012345678901234567890123456789012345";
    result = WebWifiSaveUtils::parseSaveInput(input);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Enterprise identity, username, and password must be 64 bytes or less",
                             result.error_message.c_str());
}

void test_web_wifi_save_utils_parse_rejects_encrypted_tls_key() {
    WebWifiSaveUtils::SaveInput input{};
    input.ssid = "CorpNet";
    input.auth_mode = "enterprise";
    input.eap_method = "tls";
    input.identity = "device-1";
    input.client_cert_pem = "-----BEGIN CERTIFICATE-----\nclient\n-----END CERTIFICATE-----";
    input.client_key_pem = "-----BEGIN ENCRYPTED PRIVATE KEY-----\nkey\n-----END ENCRYPTED PRIVATE KEY-----";

    const WebWifiSaveUtils::ParseResult result = WebWifiSaveUtils::parseSaveInput(input);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_STRING("Encrypted private keys and PKCS12/PFX files are not supported",
                             result.error_message.c_str());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_wifi_save_utils_parse_accepts_valid_input);
    RUN_TEST(test_web_wifi_save_utils_parse_rejects_missing_ssid_and_bad_password);
    RUN_TEST(test_web_wifi_save_utils_parse_accepts_peap_and_defaults_identity);
    RUN_TEST(test_web_wifi_save_utils_parse_accepts_ttls_phase2);
    RUN_TEST(test_web_wifi_save_utils_parse_accepts_tls_pem_fields);
    RUN_TEST(test_web_wifi_save_utils_parse_rejects_enterprise_missing_fields_and_long_values);
    RUN_TEST(test_web_wifi_save_utils_parse_rejects_encrypted_tls_key);
    return UNITY_END();
}
