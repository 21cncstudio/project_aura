#include <unity.h>

#include "web/WebDashboardPage.h"

void setUp() {}
void tearDown() {}

void test_web_dashboard_page_decide_root_action_preserves_ap_root_portal() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebDashboardPage::RootAction::WifiPortal),
                          static_cast<int>(WebDashboardPage::decideRootAction(true, false, "/")));
}

void test_web_dashboard_page_decide_root_action_blocks_sta_dashboard_without_wifi() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebDashboardPage::RootAction::NotFound),
                          static_cast<int>(WebDashboardPage::decideRootAction(false, false,
                                                                             "/dashboard")));
}

void test_web_dashboard_page_decide_root_action_serves_dashboard_when_allowed() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebDashboardPage::RootAction::Dashboard),
                          static_cast<int>(WebDashboardPage::decideRootAction(false, true,
                                                                             "/dashboard")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebDashboardPage::RootAction::Dashboard),
                          static_cast<int>(WebDashboardPage::decideRootAction(true, false,
                                                                             "/dashboard")));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_dashboard_page_decide_root_action_preserves_ap_root_portal);
    RUN_TEST(test_web_dashboard_page_decide_root_action_blocks_sta_dashboard_without_wifi);
    RUN_TEST(test_web_dashboard_page_decide_root_action_serves_dashboard_when_allowed);
    return UNITY_END();
}
