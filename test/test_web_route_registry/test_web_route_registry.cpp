#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "web/WebRouteRegistry.h"
#include "web/WebServerLimits.h"
#include "web/WebTransport.h"

namespace WebTemplates {
extern const char kDashboardStylesCssPath[] = "/assets/dashboard.css";
extern const char kDashboardAppJsPath[] = "/assets/dashboard.js";
extern const char kThemeStylesCssPath[] = "/assets/theme.css";
extern const char kThemeAppJsPath[] = "/assets/theme.js";
extern const char kDacStylesCssPath[] = "/assets/dac.css";
extern const char kDacAppJsPath[] = "/assets/dac.js";
}  // namespace WebTemplates

#define DEFINE_HANDLER(name) void name() {}
DEFINE_HANDLER(wifi_handle_root)
DEFINE_HANDLER(dashboard_handle_root)
DEFINE_HANDLER(dashboard_handle_styles)
DEFINE_HANDLER(dashboard_handle_app)
DEFINE_HANDLER(wifi_handle_save)
DEFINE_HANDLER(wifi_handle_not_found)
DEFINE_HANDLER(diag_handle_root)
DEFINE_HANDLER(sfa40_debug_handle_root)
DEFINE_HANDLER(mqtt_handle_root)
DEFINE_HANDLER(mqtt_handle_save)
DEFINE_HANDLER(theme_handle_root)
DEFINE_HANDLER(theme_handle_styles)
DEFINE_HANDLER(theme_handle_app)
DEFINE_HANDLER(theme_handle_state)
DEFINE_HANDLER(theme_handle_apply)
DEFINE_HANDLER(dac_handle_root)
DEFINE_HANDLER(dac_handle_styles)
DEFINE_HANDLER(dac_handle_app)
DEFINE_HANDLER(dac_handle_state)
DEFINE_HANDLER(dac_handle_action)
DEFINE_HANDLER(dac_handle_auto)
DEFINE_HANDLER(thresholds_handle_root)
DEFINE_HANDLER(thresholds_handle_state)
DEFINE_HANDLER(thresholds_handle_update)
DEFINE_HANDLER(thresholds_handle_reset)
DEFINE_HANDLER(charts_handle_data)
DEFINE_HANDLER(daily_history_status_handle)
DEFINE_HANDLER(daily_history_csv_handle)
DEFINE_HANDLER(daily_history_current_day_csv_handle)
DEFINE_HANDLER(daily_history_clear_handle)
DEFINE_HANDLER(daily_history_clear_current_day_handle)
DEFINE_HANDLER(state_handle_data)
DEFINE_HANDLER(events_handle_data)
DEFINE_HANDLER(diag_handle_data)
DEFINE_HANDLER(sfa40_debug_handle_data)
DEFINE_HANDLER(settings_handle_update)
DEFINE_HANDLER(ota_handle_prepare)
DEFINE_HANDLER(ota_handle_update)
DEFINE_HANDLER(ota_handle_upload)
#undef DEFINE_HANDLER

// Compile the registry only in this test translation unit, where handler stubs exist.
#include "../../src/web/WebRouteRegistry.cpp"

namespace {

enum class RouteMethod : uint8_t {
    Get,
    Post,
    PostUpload,
};

struct CapturedRoute {
    RouteMethod method = RouteMethod::Get;
    String uri;
    WebHandlerFn handler = nullptr;
    WebHandlerFn upload_handler = nullptr;
};

class NullRequest final : public WebRequest {
public:
    bool hasArg(const char *) const override { return false; }
    String arg(const char *) const override { return String(); }
    String uri() const override { return String(); }
    void sendHeader(const char *, const String &, bool = false) override {}
    void send(int, const char *, const String &) override {}
    void send(int, const char *, const char *) override {}
    bool clientConnected() const override { return false; }
    void setUploadDeadlineMs(uint32_t) override {}
    void clearUploadDeadline() override {}
    void rejectUpload() override {}
    bool uploadRejected() const override { return false; }
    size_t pendingRequestBodyBytes() const override { return 0; }
    size_t drainPendingRequestBody(size_t, uint32_t) override { return 0; }
    void stopClient() override {}
    bool beginStreamResponse(int, const char *, size_t, bool = false) override { return false; }
    int32_t writeStreamChunk(const uint8_t *, size_t, int &last_error) override {
        last_error = 0;
        return 0;
    }
    bool waitUntilWritable(uint16_t, int &last_error) override {
        last_error = 0;
        return true;
    }
    void endStreamResponse() override {}
    WebUpload upload() override { return WebUpload{}; }
};

class CapturingBackend final : public WebServerBackend {
public:
    WebRequest &request() override { return request_; }

    void onGet(const char *uri, WebHandlerFn handler) override {
        capture(RouteMethod::Get, uri, handler, nullptr);
    }

    void onPost(const char *uri, WebHandlerFn handler) override {
        capture(RouteMethod::Post, uri, handler, nullptr);
    }

    void onPostUpload(const char *uri,
                      WebHandlerFn handler,
                      WebHandlerFn upload_handler) override {
        capture(RouteMethod::PostUpload, uri, handler, upload_handler);
    }

    void onNotFound(WebHandlerFn handler) override {
        not_found_handler = handler;
    }

    const char *name() const override { return "capturing"; }
    void begin() override {}
    void stop() override {}

    const CapturedRoute *find(RouteMethod method, const char *uri) const {
        for (size_t i = 0; i < route_count; ++i) {
            if (routes[i].method == method && routes[i].uri == uri) {
                return &routes[i];
            }
        }
        return nullptr;
    }

    CapturedRoute routes[64]{};
    size_t route_count = 0;
    WebHandlerFn not_found_handler = nullptr;

private:
    void capture(RouteMethod method,
                 const char *uri,
                 WebHandlerFn handler,
                 WebHandlerFn upload_handler) {
        TEST_ASSERT_LESS_THAN_MESSAGE(sizeof(routes) / sizeof(routes[0]),
                                      route_count,
                                      "test route capture capacity exceeded");
        routes[route_count++] = CapturedRoute{
            method,
            uri ? String(uri) : String(),
            handler,
            upload_handler,
        };
    }

    NullRequest request_{};
};

CapturingBackend capture_routes() {
    CapturingBackend backend;
    WebRouteRegistry::registerRoutes(backend);
    return backend;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_web_route_registry_registers_expected_route_count() {
    CapturingBackend backend = capture_routes();

    TEST_ASSERT_EQUAL_UINT32(WebRouteRegistry::kUriRouteCount, backend.route_count);
    TEST_ASSERT_NOT_NULL(backend.not_found_handler);
}

void test_web_route_registry_registers_ota_upload_endpoints() {
    CapturingBackend backend = capture_routes();

    const CapturedRoute *prepare =
        backend.find(RouteMethod::Post, WebRouteRegistry::kOtaPreparePath);
    TEST_ASSERT_NOT_NULL(prepare);
    TEST_ASSERT_TRUE(prepare->handler == ota_handle_prepare);

    const CapturedRoute *upload =
        backend.find(RouteMethod::PostUpload, WebRouteRegistry::kOtaUploadPath);
    TEST_ASSERT_NOT_NULL(upload);
    TEST_ASSERT_TRUE(upload->handler == ota_handle_update);
    TEST_ASSERT_TRUE(upload->upload_handler == ota_handle_upload);
}

void test_web_route_registry_registers_history_endpoints() {
    CapturingBackend backend = capture_routes();

    const CapturedRoute *status =
        backend.find(RouteMethod::Get, "/api/history/daily/status");
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_TRUE(status->handler == daily_history_status_handle);

    const CapturedRoute *daily_csv =
        backend.find(RouteMethod::Get, "/api/history/daily.csv");
    TEST_ASSERT_NOT_NULL(daily_csv);
    TEST_ASSERT_TRUE(daily_csv->handler == daily_history_csv_handle);

    const CapturedRoute *current_day_csv =
        backend.find(RouteMethod::Get, "/api/history/current-day.csv");
    TEST_ASSERT_NOT_NULL(current_day_csv);
    TEST_ASSERT_TRUE(current_day_csv->handler == daily_history_current_day_csv_handle);

    const CapturedRoute *clear =
        backend.find(RouteMethod::Post, "/api/history/clear");
    TEST_ASSERT_NOT_NULL(clear);
    TEST_ASSERT_TRUE(clear->handler == daily_history_clear_handle);

    const CapturedRoute *clear_current_day =
        backend.find(RouteMethod::Post, "/api/history/current-day/clear");
    TEST_ASSERT_NOT_NULL(clear_current_day);
    TEST_ASSERT_TRUE(clear_current_day->handler == daily_history_clear_current_day_handle);
}

void test_web_route_registry_has_no_duplicate_uri_method_pairs() {
    CapturingBackend backend = capture_routes();

    for (size_t i = 0; i < backend.route_count; ++i) {
        for (size_t j = i + 1; j < backend.route_count; ++j) {
            const bool duplicate =
                backend.routes[i].method == backend.routes[j].method &&
                backend.routes[i].uri == backend.routes[j].uri;
            TEST_ASSERT_FALSE_MESSAGE(duplicate, "duplicate web route registration");
        }
    }
}

void test_esp_http_server_route_capacity_covers_registered_routes() {
    TEST_ASSERT_TRUE_MESSAGE(
        WebServerLimits::kHttpServerMaxUriHandlers >= WebRouteRegistry::kUriRouteCount,
        "esp_http_server max_uri_handlers is lower than registered URI routes");
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_web_route_registry_registers_expected_route_count);
    RUN_TEST(test_web_route_registry_registers_ota_upload_endpoints);
    RUN_TEST(test_web_route_registry_registers_history_endpoints);
    RUN_TEST(test_web_route_registry_has_no_duplicate_uri_method_pairs);
    RUN_TEST(test_esp_http_server_route_capacity_covers_registered_routes);
    return UNITY_END();
}
