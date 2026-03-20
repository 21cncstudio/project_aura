#include <unity.h>

#include "web/WebWifiPage.h"

void setUp() {}
void tearDown() {}

void test_web_wifi_page_render_scan_status_json_reports_progress_flag() {
    TEST_ASSERT_EQUAL_STRING("{\"success\":true,\"scan_in_progress\":true}",
                             WebWifiPage::renderScanStatusJson(true).c_str());
    TEST_ASSERT_EQUAL_STRING("{\"success\":true,\"scan_in_progress\":false}",
                             WebWifiPage::renderScanStatusJson(false).c_str());
}

void test_web_wifi_page_captive_portal_redirect_url_falls_back_to_default_ap_ip() {
    TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/",
                             WebWifiPage::captivePortalRedirectUrl("").c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/",
                             WebWifiPage::captivePortalRedirectUrl("0.0.0.0").c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.10.1/",
                             WebWifiPage::captivePortalRedirectUrl("192.168.10.1").c_str());
}

void test_web_wifi_page_render_root_html_replaces_items_and_scan_flag() {
    const String tpl = "items={{SSID_ITEMS}};scan={{SCAN_IN_PROGRESS}}";

    WebWifiPage::RootPageData data{};
    data.ssid_items = "<li>Aura</li>";
    data.scan_in_progress = true;

    const String html = WebWifiPage::renderRootHtml(tpl, data);
    TEST_ASSERT_EQUAL_STRING("items=<li>Aura</li>;scan=1", html.c_str());
}

void test_web_wifi_page_render_save_html_builds_hostname_url_and_wait_time() {
    const String tpl = "{{HOSTNAME_DASHBOARD_URL}}|{{WAIT_SECONDS}}";

    WebWifiPage::SavePageData data{};
    data.hostname = "aura<&>";
    data.wait_seconds = 12;

    const String html = WebWifiPage::renderSaveHtml(tpl, data);
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("http://aura&lt;&amp;&gt;.local/dashboard"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("|12"));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_wifi_page_render_scan_status_json_reports_progress_flag);
    RUN_TEST(test_web_wifi_page_captive_portal_redirect_url_falls_back_to_default_ap_ip);
    RUN_TEST(test_web_wifi_page_render_root_html_replaces_items_and_scan_flag);
    RUN_TEST(test_web_wifi_page_render_save_html_builds_hostname_url_and_wait_time);
    return UNITY_END();
}
