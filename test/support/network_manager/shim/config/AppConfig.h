#pragma once
#include <Arduino.h>
namespace Config {
constexpr char WIFI_AP_SSID[] = "ProjectAura-Setup";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 60000;
constexpr uint32_t WIFI_CONNECT_RETRY_DELAY_MS = 200;
constexpr uint8_t WIFI_CONNECT_MAX_RETRIES = 5;
enum class WifiAuthMode : uint8_t { Personal, Enterprise };
enum class WifiEapMethod : uint8_t { Peap, Ttls, Tls };
enum class WifiTtlsPhase2 : uint8_t { Mschapv2, Mschap, Pap, Chap, Eap };
struct WifiSettings { String ssid; String pass; bool enabled = true; WifiAuthMode auth_mode = WifiAuthMode::Personal;
    WifiEapMethod eap_method = WifiEapMethod::Peap; WifiTtlsPhase2 ttls_phase2 = WifiTtlsPhase2::Mschapv2;
    String identity, username, enterprise_password, ca_cert_pem, client_cert_pem, client_key_pem; };
}
