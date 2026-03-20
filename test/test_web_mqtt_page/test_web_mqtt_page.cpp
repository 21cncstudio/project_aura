#include <unity.h>

#include "web/WebMqttPage.h"

void setUp() {}
void tearDown() {}

void test_web_mqtt_page_root_access_matches_wifi_and_screen_state() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebMqttPage::RootAccess::NotFound),
                          static_cast<int>(WebMqttPage::rootAccess(false, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebMqttPage::RootAccess::Locked),
                          static_cast<int>(WebMqttPage::rootAccess(true, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebMqttPage::RootAccess::Ready),
                          static_cast<int>(WebMqttPage::rootAccess(true, true)));
}

void test_web_mqtt_page_status_for_matches_connectivity_states() {
    WebMqttPage::PageData data;

    WebMqttPage::StatusView status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("Disabled", status.text);
    TEST_ASSERT_EQUAL_STRING("status-disconnected", status.css_class);

    data.mqtt_enabled = true;
    status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("No WiFi", status.text);

    data.wifi_enabled = true;
    data.wifi_connected = true;
    status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("Connecting", status.text);

    data.mqtt_retry_stage = 1;
    status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("Retrying", status.text);

    data.mqtt_retry_stage = 2;
    status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("Delayed retry", status.text);

    data.mqtt_connected = true;
    status = WebMqttPage::statusFor(data);
    TEST_ASSERT_EQUAL_STRING("Connected", status.text);
    TEST_ASSERT_EQUAL_STRING("status-connected", status.css_class);
}

void test_web_mqtt_page_render_html_replaces_and_escapes_placeholders() {
    const String tpl =
        "{{STATUS}}|{{STATUS_CLASS}}|{{DEVICE_ID}}|{{DEVICE_IP}}|{{MQTT_HOST}}|{{MQTT_PORT}}|"
        "{{MQTT_USER}}|{{MQTT_PASS}}|{{MQTT_NAME}}|{{MQTT_TOPIC}}|{{ANONYMOUS_CHECKED}}|"
        "{{DISCOVERY_CHECKED}}";

    WebMqttPage::PageData data;
    data.mqtt_enabled = true;
    data.wifi_enabled = true;
    data.wifi_connected = true;
    data.mqtt_connected = true;
    data.device_id = "id<&>";
    data.device_ip = "192.168.1.5";
    data.host = "host.local";
    data.port = 1883;
    data.user = "user";
    data.pass = "p<&>w";
    data.device_name = "Aura \"Screen\"";
    data.base_topic = "aura/main";
    data.anonymous = true;
    data.discovery = false;

    const String html = WebMqttPage::renderHtml(tpl, data);
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("Connected|status-connected"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("id&lt;&amp;&gt;"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("p&lt;&amp;&gt;w"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("Aura &quot;Screen&quot;"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("1883"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("|checked|"));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_mqtt_page_root_access_matches_wifi_and_screen_state);
    RUN_TEST(test_web_mqtt_page_status_for_matches_connectivity_states);
    RUN_TEST(test_web_mqtt_page_render_html_replaces_and_escapes_placeholders);
    return UNITY_END();
}
