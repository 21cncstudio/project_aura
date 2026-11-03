// Host harness for the real patched Arduino event callback. No WiFi hardware.
#include <cassert>
#include <cstring>
#define ARDUHAL_LOG_LEVEL 0
#define ARDUHAL_LOG_LEVEL_VERBOSE 5
#define CONFIG_LWIP_IPV6 0
#define log_v(...) ((void)0)
#define log_d(...) ((void)0)
#define log_w(...) ((void)0)
#define log_e(...) ((void)0)
using uint8_t = unsigned char;
enum {
    ARDUINO_EVENT_WIFI_STA_START, ARDUINO_EVENT_WIFI_STA_STOP,
    ARDUINO_EVENT_WIFI_STA_CONNECTED, ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
    ARDUINO_EVENT_WIFI_STA_GOT_IP, ARDUINO_EVENT_WIFI_STA_LOST_IP
};
enum {
    WIFI_REASON_UNSPECIFIED = 1, WIFI_REASON_NO_AP_FOUND, WIFI_REASON_AUTH_FAIL,
    WIFI_REASON_BEACON_TIMEOUT, WIFI_REASON_HANDSHAKE_TIMEOUT,
    WIFI_REASON_AUTH_EXPIRE, WIFI_REASON_ASSOC_LEAVE, WIFI_REASON_ASSOC_FAIL
};
enum { WL_DISCONNECTED, WL_STOPPED, WL_IDLE_STATUS, WL_NO_SSID_AVAIL,
       WL_CONNECT_FAILED, WL_CONNECTION_LOST, WL_CONNECTED };
using wifi_err_reason_t = int;
constexpr int ESP_OK = 0;
struct arduino_event_t {
    int event_id;
    struct { struct { uint8_t reason; } wifi_sta_disconnected; } event_info{};
};
struct FakeSta {
    bool enabled = false;
    int status = WL_DISCONNECTED;
    int connects = 0;
    int disconnects = 0;
    void _setStatus(int value) { status = value; }
    bool getAutoReconnect() { return enabled; }
    void connect() { ++connects; }
    void disconnect() { ++disconnects; }
} sta;
FakeSta *_sta_network_if = &sta;
struct { int getSleep() { return 0; } } WiFi;
int esp_wifi_set_ps(int) { return ESP_OK; }
bool _is_staReconnectableReason(int reason) { return reason == WIFI_REASON_BEACON_TIMEOUT; }

// The test runner inserts the entire real _onStaArduinoEvent() here.
// CALLBACK

void emit(int type, uint8_t reason = 0) {
    arduino_event_t event{type};
    event.event_info.wifi_sta_disconnected.reason = reason;
    _onStaArduinoEvent(&event);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const char *scenario = argv[1];
    sta.enabled = std::strstr(scenario, "enabled") != nullptr;
    if (std::strstr(scenario, "after_ip")) {
        emit(ARDUINO_EVENT_WIFI_STA_START);
        emit(ARDUINO_EVENT_WIFI_STA_CONNECTED);
        emit(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    }
    if (std::strcmp(scenario, "voluntary_enabled") == 0) {
        emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, WIFI_REASON_ASSOC_LEAVE);
        assert(sta.connects == 0 && sta.disconnects == 0);
    } else if (std::strcmp(scenario, "disable_after_retry") == 0) {
        sta.enabled = true;
        emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, WIFI_REASON_BEACON_TIMEOUT);
        sta.enabled = false;
        emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, WIFI_REASON_BEACON_TIMEOUT);
        assert(sta.connects == 1 && sta.disconnects == 1);
    } else {
        emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, WIFI_REASON_AUTH_FAIL);
        assert(sta.connects == (sta.enabled ? 1 : 0));
        emit(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, WIFI_REASON_BEACON_TIMEOUT);
        assert(sta.connects == (sta.enabled ? 2 : 0));
        assert(sta.disconnects == sta.connects);
    }
}
