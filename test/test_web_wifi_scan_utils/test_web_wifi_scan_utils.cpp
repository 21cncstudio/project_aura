#include <unity.h>

#include "web/WebWifiScanUtils.h"

void setUp() {}
void tearDown() {}

void test_web_wifi_scan_utils_keeps_best_duplicate_and_caps_list() {
    WebWifiScanUtils::WifiScanRow rows[2];
    size_t count = 0;

    WebWifiScanUtils::addOrReplaceBestNetwork(rows, count, 2, "Alpha", -70, false);
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, count, 2, "Alpha", -55, true);
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, count, 2, "Beta", -60, false);
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, count, 2, "Gamma", -80, true);
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, count, 2, "Delta", -40, true);

    TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(count));
    TEST_ASSERT_EQUAL_STRING("Alpha", rows[0].ssid.c_str());
    TEST_ASSERT_EQUAL_INT(-55, rows[0].rssi);
    TEST_ASSERT_TRUE(rows[0].open);

    TEST_ASSERT_EQUAL_STRING("Delta", rows[1].ssid.c_str());
    TEST_ASSERT_EQUAL_INT(-40, rows[1].rssi);
    TEST_ASSERT_TRUE(rows[1].open);
}

void test_web_wifi_scan_utils_sort_orders_by_strongest_rssi() {
    WebWifiScanUtils::WifiScanRow rows[3] = {
        {"Gamma", -75, 50, true},
        {"Alpha", -40, 100, false},
        {"Beta", -60, 80, false},
    };

    WebWifiScanUtils::sortNetworksByRssiDesc(rows, 3);

    TEST_ASSERT_EQUAL_STRING("Alpha", rows[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Beta", rows[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Gamma", rows[2].ssid.c_str());
}

void test_web_wifi_scan_utils_render_network_items_html_escapes_ssid() {
    WebWifiScanUtils::WifiScanRow rows[1] = {
        {"Cafe<&>\"'", -51, 98, false},
    };

    const String html = WebWifiScanUtils::renderNetworkItemsHtml(rows, 1);
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("network-item"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("Secure | -51 dBm"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("98%"));
    TEST_ASSERT_NOT_EQUAL(String::npos, html.find("Cafe&lt;&amp;&gt;&quot;&#39;"));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_wifi_scan_utils_keeps_best_duplicate_and_caps_list);
    RUN_TEST(test_web_wifi_scan_utils_sort_orders_by_strongest_rssi);
    RUN_TEST(test_web_wifi_scan_utils_render_network_items_html_escapes_ssid);
    return UNITY_END();
}
