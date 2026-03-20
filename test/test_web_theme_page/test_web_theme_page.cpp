#include <unity.h>

#include "web/WebThemePage.h"

void setUp() {}
void tearDown() {}

void test_web_theme_page_root_access_requires_wifi_first() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebThemePage::RootAccess::WifiRequired),
                          static_cast<int>(WebThemePage::rootAccess(false, true, true)));
}

void test_web_theme_page_root_access_requires_both_theme_screens() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebThemePage::RootAccess::Locked),
                          static_cast<int>(WebThemePage::rootAccess(true, false, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebThemePage::RootAccess::Locked),
                          static_cast<int>(WebThemePage::rootAccess(true, true, false)));
}

void test_web_theme_page_root_access_allows_ready_state() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebThemePage::RootAccess::Ready),
                          static_cast<int>(WebThemePage::rootAccess(true, true, true)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_theme_page_root_access_requires_wifi_first);
    RUN_TEST(test_web_theme_page_root_access_requires_both_theme_screens);
    RUN_TEST(test_web_theme_page_root_access_allows_ready_state);
    return UNITY_END();
}
