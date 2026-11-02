// HTTP/AP review lab. Real: esp_http_server 6.1, http_parser, Arduino WString,
// WebTransportEspHttpServer, WebResponseUtils, WebStreamWriter, WebStreamState,
// WebPortalHandlers, WebWifiPage, WebDashboardPage, WebWifiScanUtils,
// WebRouteRegistry, WakePowerGuard. Fake: OS, sockets (Winsock loopback), Wi-Fi.
// A PASS here says nothing about radio, lwIP or ESP32 scheduling.
#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <mmsystem.h>

#include "core/ConnectivityRuntime.h"
#include "core/Logger.h"
#include "core/WakePowerGuard.h"
#include "core/Watchdog.h"
#include "core/WifiPowerSaveGuard.h"
#include "core/WifiScanSession.h"
#include "core/WifiScanTransition.h"
#include <esp_wifi.h>
#include "web/WebContext.h"
#include "web/WebHandlers.h"
#include "web/WebPortalHandlers.h"
#include "web/WebResponseUtils.h"
#include "web/WebStreamState.h"
#include "web/WebTemplates.h"
#include "web/WebTextUtils.h"
#include "web/WebTransport.h"
#include "web/WebWifiPage.h"
#include "web/WebWifiScanUtils.h"
#include "lab.h"

LabWiFi WiFi;
static std::atomic<int> g_ps_mode{WIFI_PS_NONE};
static std::atomic<bool> g_ps_restore_entered{false}, g_ps_hold_restore{false};
static std::atomic<bool> g_ps_fail_get{false}, g_ps_fail_set{false};
esp_err_t esp_wifi_get_ps(wifi_ps_type_t *mode) {
    if (g_ps_fail_get) return ESP_FAIL;
    *mode = static_cast<wifi_ps_type_t>(g_ps_mode.load()); return ESP_OK;
}
esp_err_t esp_wifi_set_ps(wifi_ps_type_t mode) {
    if (g_ps_fail_set) return ESP_FAIL;
    if (mode == WIFI_PS_MIN_MODEM && g_ps_hold_restore) {
        g_ps_restore_entered = true;
        while (g_ps_hold_restore) Sleep(1);
    }
    g_ps_mode = mode; return ESP_OK;
}


// Real production worker; only the driver call is controlled. No Arduino
// event is ever delivered, so successful tests also exercise dropped SCAN_DONE.
static std::atomic<int> g_scan_starts{0}, g_scan_stops{0}, g_scan_clears{0};
static std::atomic<bool> g_scan_complete{false}, g_scan_blocking{false};
static std::atomic<int> g_scan_stop_error{ESP_OK}, g_scan_start_error{ESP_OK};
static std::vector<wifi_ap_record_t> g_scan_records;
static size_t g_scan_read = 0;
static std::atomic<bool> g_scan_start_gate{false};
static std::atomic<int> g_scan_protected_calls{0};
// Test barriers hold a driver call open without changing production code.
static std::atomic<bool> g_scan_hold_cleanup{false}, g_scan_cleanup_entered{false};
static std::atomic<bool> g_scan_hold_stop{false}, g_scan_stop_entered{false};
static void check_scan_guard() {
    const auto p=WakePowerGuard::phase(millis());
    if (p==WakePowerGuard::Phase::Switching || p==WakePowerGuard::Phase::Settle ||
        p==WakePowerGuard::Phase::RenderWait || p==WakePowerGuard::Phase::FailClosed)
        ++g_scan_protected_calls;
}
int LabWiFi::scanNetworks(bool) { return WIFI_SCAN_FAILED; }
int LabWiFi::scanComplete() { return WIFI_SCAN_FAILED; }
void LabWiFi::scanDelete() {}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool block) {
    check_scan_guard();
    g_scan_blocking = block; ++g_scan_starts;
    if(g_scan_start_gate.load()) {
        while(g_scan_start_gate.load()) Sleep(1);
        // A stop issued before the driver actually began has no effect on
        // this scan. The owner must retry, not pretend completion occurred.
        g_scan_complete=false;
    }
    while (!g_scan_complete.load()) Sleep(1);
    g_scan_read = 0;
    return g_scan_start_error.load();
}
esp_err_t esp_wifi_scan_stop() {
    check_scan_guard(); ++g_scan_stops;
    const int error = g_scan_stop_error.load();
    if (error == ESP_OK) g_scan_complete = true;
    g_scan_stop_entered = true;
    while (g_scan_hold_stop.load()) Sleep(1);
    check_scan_guard();
    return error;
}
esp_err_t esp_wifi_clear_ap_list() {
    check_scan_guard(); ++g_scan_clears;
    g_scan_cleanup_entered = true;
    while (g_scan_hold_cleanup.load()) Sleep(1);
    check_scan_guard();
    return ESP_OK;
}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count) { check_scan_guard(); *count = static_cast<uint16_t>(g_scan_records.size()); return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *record) {
    check_scan_guard();
    if (g_scan_read >= g_scan_records.size()) return ESP_FAIL;
    *record = g_scan_records[g_scan_read++]; return ESP_OK;
}


// ---- Fake network plane -------------------------------------------------
static std::mutex g_snapshot_lock;
static ConnectivityRuntimeSnapshot g_snapshot;
static ConnectivityRuntime g_connectivity;
static std::atomic<bool> g_scan_requested{false};
static std::atomic<int> g_scan_commands{0};
static std::atomic<bool> g_network_thread_run{true};

ConnectivityRuntime::ConnectivityRuntime() {}
ConnectivityRuntimeSnapshot ConnectivityRuntime::snapshot() const {
    std::lock_guard<std::mutex> g(g_snapshot_lock);
    return g_snapshot;
}
ConnectivityStatusSnapshot ConnectivityRuntime::statusSnapshot() const {
    std::lock_guard<std::mutex> g(g_snapshot_lock);
    ConnectivityStatusSnapshot s{};
    s.valid = true;
    s.wifi_ap_mode = g_snapshot.wifi_ap_mode;
    return s;
}
static void set_mode(bool ap, const char *ap_ip = "192.168.4.1") {
    std::lock_guard<std::mutex> g(g_snapshot_lock);
    g_snapshot = {};
    g_snapshot.wifi_enabled = true;
    g_snapshot.wifi_ap_mode = ap;
    g_snapshot.wifi_connected = !ap;
    g_snapshot.ap_ip = ap ? ap_ip : "";
    g_snapshot.wifi_scan_options = "";
}
static void set_scan_in_progress(bool v) {
    std::lock_guard<std::mutex> g(g_snapshot_lock);
    g_snapshot.wifi_scan_in_progress = v;
}

// The real command is enqueued and consumed by the network task within ~10 ms.
static bool lab_wifi_start_scan() {
    g_scan_commands.fetch_add(1);
    g_scan_requested.store(true);
    return true;
}
static void network_thread() {
    while (g_network_thread_run.load()) {
        if (g_scan_requested.exchange(false)) set_scan_in_progress(true);
        Sleep(10);
    }
}

// ---- Same StreamContext wiring as WebHandlersSupport.cpp --------------------
static WebStreamState g_stream_state;
static WebHandlerContext g_ctx;
static std::unique_ptr<WebServerBackend> g_backend;

static uint32_t ctx_now(void *) { return millis(); }
static uint32_t ctx_sta(void *) { return 0; }
static void run_delay(void *, uint16_t ms) { delay(ms); }
static void run_kick(void *) { Watchdog::kick(); }
static void capture_env(void *, WebStreamEnvironment &out) {
    out.memory_valid = true;
    const ConnectivityStatusSnapshot n = g_connectivity.statusSnapshot();
    out.network_valid = n.valid;
    out.ap_mode = n.wifi_ap_mode;
}
static WebResponseUtils::StreamContext response_context() {
    static const WebStreamRuntime runtime = {nullptr, ctx_now, run_delay, run_kick, nullptr, 0};
    WebResponseUtils::StreamContext c;
    c.stream_state = &g_stream_state;
    c.stream_runtime = &runtime;
    c.slow_write_warn_ms = 200;
    c.nowMs = ctx_now;
    c.wifiStaConnectedElapsedMs = ctx_sta;
    c.captureFailureEnvironment = capture_env;
    return c;
}

// ---- Logger capture ----------------------------------------------------------
static std::mutex g_log_lock;
static std::vector<std::string> g_logs;
static bool g_echo_requests = false;
void Logger::log(Level level, const char *tag, const char *fmt, ...) {
    char line[256];
    va_list a;
    va_start(a, fmt);
    vsnprintf(line, sizeof(line), fmt, a);
    va_end(a);
    std::lock_guard<std::mutex> g(g_log_lock);
    g_logs.push_back(std::string(tag) + ": " + line);
    if (g_echo_requests) { printf("    [fw-log] %s: %s\n", tag, line); fflush(stdout); }
}
static std::vector<std::string> take_logs() {
    std::lock_guard<std::mutex> g(g_log_lock);
    auto copy = g_logs;
    g_logs.clear();
    return copy;
}

// ---- Accept/close bookkeeping --------------------------------------------------
static std::mutex g_fd_lock;
static std::vector<std::pair<char, int>> g_fd_events;
void lab_note_accept(int fd) { std::lock_guard<std::mutex> g(g_fd_lock); g_fd_events.push_back({'A', fd}); }
void lab_note_close(int fd) {
    int expected = fd;
    g_lab_stall_fd.compare_exchange_strong(expected, -1);
    std::lock_guard<std::mutex> g(g_fd_lock);
    g_fd_events.push_back({'C', fd});
}
static std::atomic<uint32_t> g_stall_override_ms{0};
uint32_t lab_stall_ms(uint32_t configured_ms) {
    const uint32_t o = g_stall_override_ms.load();
    return o ? o : configured_ms;
}

// ---- Handlers wired exactly like WebHandlers.cpp for the in-scope routes ----
#define LOG_REQ(name) do { if (g_echo_requests) { \
    WebRequest &r = *g_ctx.server; \
    printf("    [req] %-22s uri=%s\n", name, r.uri().c_str()); fflush(stdout); } } while (0)
void wifi_handle_root() { LOG_REQ("wifi_handle_root"); WebPortalHandlers::handleWifiRoot(g_ctx, response_context()); }
void dashboard_handle_root() { LOG_REQ("dashboard_handle_root"); WebPortalHandlers::handleDashboardRoot(g_ctx, response_context()); }
void dashboard_handle_styles() { LOG_REQ("dashboard_handle_styles"); WebPortalHandlers::handleDashboardStyles(g_ctx, response_context()); }
void dashboard_handle_app() { LOG_REQ("dashboard_handle_app"); WebPortalHandlers::handleDashboardApp(g_ctx, response_context()); }
void wifi_handle_not_found() { LOG_REQ("wifi_handle_not_found"); WebPortalHandlers::handleWifiNotFound(g_ctx); }
#define OTHER(name) void name() { g_ctx.server->send(501, "text/plain", "lab stub"); }
OTHER(wifi_handle_save) OTHER(diag_handle_root) OTHER(sfa40_debug_handle_root) OTHER(mqtt_handle_root)
OTHER(mqtt_handle_save) OTHER(theme_handle_root) OTHER(theme_handle_styles) OTHER(theme_handle_app)
OTHER(theme_handle_state) OTHER(theme_handle_apply) OTHER(dac_handle_root) OTHER(dac_handle_styles)
OTHER(dac_handle_app) OTHER(dac_handle_state) OTHER(dac_handle_action) OTHER(dac_handle_auto)
OTHER(thresholds_handle_root) OTHER(thresholds_handle_state) OTHER(thresholds_handle_update)
OTHER(thresholds_handle_reset) OTHER(charts_handle_data) OTHER(daily_history_status_handle)
OTHER(daily_history_reconnect_handle) OTHER(daily_history_csv_handle)
OTHER(daily_history_current_day_csv_handle) OTHER(daily_history_clear_handle)
OTHER(daily_history_clear_current_day_handle) OTHER(state_handle_data) OTHER(events_handle_data)
OTHER(diag_handle_data) OTHER(sfa40_debug_handle_data) OTHER(settings_handle_update)
OTHER(ota_handle_prepare) OTHER(ota_handle_update) OTHER(ota_handle_upload)
#include "web/WebRouteRegistry.cpp"

// Test-only routes exercise the production transport and stream helper.
static void test_plain_failure() { g_lab_fail_next_send = 1; g_ctx.server->send(200, "application/json", "{}"); }
static void test_finish_failure() { g_lab_fail_next_send = 1; WebResponseUtils::sendHtmlStream(*g_ctx.server, "", response_context()); }
static void test_setup_failure() {
    g_ctx.server->sendHeader("X-Invalid", "value\r\ninjected: yes");
    WebResponseUtils::sendHtmlStream(*g_ctx.server, "never sent", response_context());
}
static void test_stop_twice() {
    g_ctx.server->stopClient(); g_ctx.server->stopClient();
    g_ctx.server->send(200, "text/plain", "must not be sent");
    WebResponseUtils::sendHtmlStream(*g_ctx.server, "must not be sent", response_context());
}
static void test_large_stream() {
    static const String body = [] { String text; text.reserve(60 * 1024); for (int i=0;i<60*1024;++i) text += 'x'; return text; }();
    WebResponseUtils::sendHtmlStream(*g_ctx.server, body, response_context());
}

// ---- Raw test client --------------------------------------------------------
static constexpr uint16_t kPort = 18080;

static SOCKET cl_connect(int rcvbuf = 0) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (rcvbuf) ::setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&rcvbuf), sizeof(rcvbuf));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(kPort);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0) {
        printf("connect failed %d\n", WSAGetLastError());
        exit(2);
    }
    return s;
}
static void cl_send(SOCKET s, const std::string &req) { ::send(s, req.c_str(), static_cast<int>(req.size()), 0); }
static std::string get_req(const char *method, const char *path, const char *extra = "") {
    return std::string(method) + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: lab/1\r\n" + extra + "\r\n";
}
static void cl_rst(SOCKET s) {
    linger l{1, 0};
    ::setsockopt(s, SOL_SOCKET, SO_LINGER, reinterpret_cast<char *>(&l), sizeof(l));
    ::closesocket(s);
}

struct Reply {
    int status = 0;
    std::string headers;
    std::string raw_body;
    bool complete = false;
    bool closed = false;
    bool timed_out = false;
    int wsa = 0;
    uint32_t ms = 0;
};
static bool chunked_complete(const std::string &b) {
    return b.size() >= 5 && b.compare(b.size() - 5, 5, "0\r\n\r\n") == 0;
}
static std::string dechunk(const std::string &b) {
    std::string out;
    size_t p = 0;
    while (p < b.size()) {
        const size_t e = b.find("\r\n", p);
        if (e == std::string::npos) break;
        const size_t n = strtoul(b.substr(p, e - p).c_str(), nullptr, 16);
        if (!n) break;
        out += b.substr(e + 2, n);
        p = e + 2 + n + 2;
    }
    return out;
}
static std::string header_value(const std::string &h, const char *name) {
    std::string lo = h, key = std::string("\r\n") + name + ":";
    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    const size_t p = lo.find(key);
    if (p == std::string::npos) return "";
    size_t b = p + key.size();
    while (b < h.size() && h[b] == ' ') ++b;
    return h.substr(b, h.find("\r\n", b) - b);
}
static Reply cl_read(SOCKET s, uint32_t timeout_ms, bool head = false) {
    Reply r;
    const uint32_t start = millis();
    std::string buf;
    DWORD to = timeout_ms;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&to), sizeof(to));
    char tmp[4096];
    size_t hdr_end = std::string::npos;
    while (true) {
        const int n = ::recv(s, tmp, static_cast<int>(sizeof(tmp)), 0);
        if (n == 0) { r.closed = true; break; }
        if (n < 0) {
            r.wsa = WSAGetLastError();
            if (r.wsa == WSAETIMEDOUT) r.timed_out = true; else r.closed = true;
            break;
        }
        buf.append(tmp, n);
        if (hdr_end == std::string::npos) {
            hdr_end = buf.find("\r\n\r\n");
            if (hdr_end == std::string::npos) continue;
            r.headers = buf.substr(0, hdr_end + 2);
            sscanf(r.headers.c_str(), "HTTP/1.1 %d", &r.status);
        }
        r.raw_body = buf.substr(hdr_end + 4);
        const std::string cl = header_value(r.headers, "Content-Length");
        if (head && !cl.empty()) { r.complete = true; break; }
        if (!cl.empty() && r.raw_body.size() >= strtoul(cl.c_str(), nullptr, 10)) { r.complete = true; break; }
        if (header_value(r.headers, "Transfer-Encoding") == "chunked" && chunked_complete(r.raw_body)) {
            r.complete = true;
            break;
        }
    }
    r.ms = millis() - start;
    return r;
}
static std::string body_of(const Reply &r) {
    return header_value(r.headers, "Transfer-Encoding") == "chunked" ? dechunk(r.raw_body) : r.raw_body;
}
static Reply one_shot(const char *method, const char *path, uint32_t timeout_ms = 8000) {
    SOCKET s = cl_connect();
    cl_send(s, get_req(method, path));
    Reply r = cl_read(s, timeout_ms);
    ::closesocket(s);
    Sleep(20);
    return r;
}

static int g_fail = 0;
static void expect(bool ok, const char *label) {
    printf("  %s %s\n", ok ? "[PASS]" : "[FAIL]", label);
    fflush(stdout);
    if (!ok) ++g_fail;
}
static bool logs_contain(const std::vector<std::string> &l, const char *needle) {
    for (const auto &e : l) if (e.find(needle) != std::string::npos) return true;
    return false;
}
static void print_logs(const std::vector<std::string> &l) {
    for (const auto &e : l) printf("    log: %s\n", e.c_str());
}

// ---- Scenarios --------------------------------------------------------------
static void route_row(const char *m, const char *p) {
    Reply r = one_shot(m, p);
    const std::string b = body_of(r);
    const char *kind = b.find("WiFi Configuration") != std::string::npos ? "WIFI-PAGE"
        : (b.size() > 2 && static_cast<uint8_t>(b[0]) == 0x1f) ? "DASHBOARD-GZIP"
        : b.find("scan_in_progress") != std::string::npos ? "SCAN-JSON" : "other";
    printf("  %-5s %-28s -> %3d %-14s body=%-6u ce=%-5s loc=%s %s\n", m, p, r.status, kind,
           static_cast<unsigned>(b.size()), header_value(r.headers, "Content-Encoding").c_str(),
           header_value(r.headers, "Location").c_str(), r.complete ? "" : (r.closed ? "(closed)" : "(timeout)"));
}
static void s1_routes() {
    printf("\n== S1 route matrix ==\n");
    const char *rows[][2] = {{"GET", "/"}, {"GET", "/?x=1"}, {"GET", "/?"}, {"GET", "/dashboard"}, {"GET", "/wifi"},
                             {"GET", "/wifi?scan_status=1"}, {"GET", "/favicon.ico"}, {"GET", "/generate_204"},
                             {"GET", "/hotspot-detect.html"}, {"GET", "/connecttest.txt"}, {"HEAD", "/"},
                             {"HEAD", "/wifi"}, {"HEAD", "/nope"}, {"OPTIONS", "/"}, {"POST", "/nope"}};
    printf(" AP mode:\n");
    set_mode(true);
    for (auto &r : rows) route_row(r[0], r[1]);
    printf(" STA connected mode:\n");
    set_mode(false);
    for (auto &r : rows) route_row(r[0], r[1]);
    set_mode(true);
    Reply query = one_shot("GET", "/?x=1");
    expect(query.status == 200 && body_of(query).find("WiFi Configuration") != std::string::npos,
           "AP root query routes to WiFi setup");
    Reply icon = one_shot("GET", "/favicon.ico?x=1");
    expect(icon.status == 204 && body_of(icon).empty() && header_value(icon.headers, "Location").empty(),
           "favicon never redirects to the HTML page");
    Reply head = one_shot("HEAD", "/unknown");
    expect(head.status == 302 && head.raw_body.empty(), "unknown HEAD has no response body");
    for (const char *path : {"/", "/wifi"}) {
        Reply rejected = one_shot("HEAD", path);
        expect(rejected.status == 405 && rejected.raw_body.empty() &&
               header_value(rejected.headers, "Content-Length") == "0",
               "unsupported HEAD has an empty 405 response");
    }
    take_logs();
}

static void s2_scan_stale() {
    printf("\n== S2 /wifi?scan=1 renders pre-scan snapshot ==\n");
    set_mode(true);
    g_scan_commands = 0;
    Reply r = one_shot("GET", "/wifi?scan=1");
    const std::string b = body_of(r);
    const bool zero = b.find("var scanInProgress = 0;") != std::string::npos;
    const bool empty_list = b.find("No networks found") != std::string::npos;
    printf("  scan commands issued=%d, response has scanInProgress=0: %d, 'No networks found' shown: %d\n",
           g_scan_commands.load(), zero, empty_list);
    expect(g_scan_commands.load() == 1 && !zero && !empty_list, "accepted scan renders progress and starts polling");
    Sleep(40);
    Reply c = one_shot("GET", "/wifi");
    expect(body_of(c).find("var scanInProgress = 1;") != std::string::npos,
           "CONTROL: next plain /wifi sees scanInProgress=1");
    set_scan_in_progress(false);
}

static void set_small_server_buffers() { g_lab_server_sndbuf = 2048; }

static void s3_early_close() {
    printf("\n== S3 client aborts Wi-Fi page after first segment ==\n");
    set_mode(true);
    set_small_server_buffers();
    take_logs();
    SOCKET a = cl_connect(1024);
    cl_send(a, get_req("GET", "/"));
    char tmp[1000];
    ::recv(a, tmp, static_cast<int>(sizeof(tmp)), 0);
    cl_rst(a);
    Sleep(300);
    auto l = take_logs();
    print_logs(l);
    expect(logs_contain(l, "HTTP interrupted"), "early RST is reported as HTTP interrupted");
    SOCKET b = cl_connect();
    cl_send(b, get_req("GET", "/"));
    Reply r = cl_read(b, 5000);
    ::closesocket(b);
    Sleep(30);
    expect(r.complete && take_logs().empty(), "CONTROL: full read completes with no failure log");
    g_lab_server_sndbuf = 0;
}

// Fails a stream on one session, with another connection already waiting in the
// listen backlog when the handler returns.
static void s5_double_close(bool with_other_session) {
    printf("\n== S5 stale second close, other_session=%d ==\n", with_other_session);
    set_mode(true);
    set_small_server_buffers();
    Sleep(50);
    take_logs();
    SOCKET k = INVALID_SOCKET;
    if (with_other_session) {
        k = cl_connect();
        cl_send(k, get_req("GET", "/wifi?scan_status=1"));
        Reply kr = cl_read(k, 3000);
        printf("  keep-alive session K: status=%d\n", kr.status);
    }
    // Stream to A blocks on a stalled peer; the victim N connects meanwhile.
    SOCKET a = cl_connect(1024);
    cl_send(a, get_req("GET", "/"));
    Sleep(60);
    SOCKET n = cl_connect();
    cl_send(n, get_req("GET", "/wifi?scan_status=1"));
    Sleep(60);
    cl_rst(a);                      // fails A's stream while N waits in backlog
    Reply r = cl_read(n, 4000);
    printf("  victim N: status=%d complete=%d closed=%d wsa=%d after %u ms\n", r.status, r.complete, r.closed, r.wsa, r.ms);
    print_logs(take_logs());
    ::closesocket(n);
    if (k != INVALID_SOCKET) ::closesocket(k);
    Sleep(150);
    g_lab_server_sndbuf = 0;
    expect(r.status == 200 && r.complete, "next client survives a failed stream");
}

static void s4_stall_blocks_all(bool stalled, bool idle_session = false) {
    printf("== S4 stalled client vs second client, stalled=%d idle_keepalive=%d ==\n", stalled, idle_session);
    set_mode(true);
    take_logs();
    SOCKET idle = INVALID_SOCKET;
    if (idle_session) {
        idle = cl_connect();
        cl_send(idle, get_req("GET", "/wifi?scan_status=1"));
        cl_read(idle, 3000);
    }
    g_stall_override_ms = 0;  // Use the actual production send deadline.
    SOCKET a = cl_connect(1024);
    if (stalled) {
        Sleep(30);
        std::lock_guard<std::mutex> g(g_fd_lock);
        for (auto it = g_fd_events.rbegin(); it != g_fd_events.rend(); ++it) {
            if (it->first == 'A') { g_lab_stall_fd = it->second; break; }
        }
    }
    cl_send(a, get_req("GET", "/"));
    Sleep(100);
    const uint32_t t0 = millis();
    SOCKET b = cl_connect();
    cl_send(b, get_req("GET", "/wifi?scan_status=1"));
    Reply r = cl_read(b, 15000);
    const uint32_t waited = millis() - t0;
    printf("  second client: status=%d waited=%u ms\n", r.status, waited);
    if (!stalled) {
        char tmp[8192];
        DWORD to = 500;
        ::setsockopt(a, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&to), sizeof(to));
        while (::recv(a, tmp, static_cast<int>(sizeof(tmp)), 0) > 0) {}
    }
    g_lab_stall_fd = -1;
    g_stall_override_ms = 0;
    ::closesocket(a);
    ::closesocket(b);
    if (idle != INVALID_SOCKET) ::closesocket(idle);
    Sleep(100);
    auto l = take_logs();
    print_logs(l);
    if (stalled) expect(waited >= 4000 && waited < 6500 && r.status == 200 && r.complete,
                        "stalled client is bounded and the following client receives 200");
    else expect(waited < 1500 && r.status == 200, "healthy first client leaves the next request responsive");
}

static void s6_alloc_failure() {
    printf("\n== S6 real Arduino String under allocation failure ==\n");
    set_mode(true);
    take_logs();
    WebTransferSnapshot before = g_stream_state.snapshot(millis());
    g_lab_alloc_fail_min = 4096;
    g_lab_alloc_failed = 0;
    Reply r = one_shot("GET", "/");
    g_lab_alloc_fail_min = 0;
    WebTransferSnapshot after = g_stream_state.snapshot(millis());
    const std::string b = body_of(r);
    printf("  failed allocations=%ld status=%d body=%u ok_delta=%lu abort_delta=%lu\n", g_lab_alloc_failed.load(), r.status,
           static_cast<unsigned>(b.size()), static_cast<unsigned long>(after.stats.ok_count - before.stats.ok_count),
           static_cast<unsigned long>(after.stats.abort_count - before.stats.abort_count));
    auto l = take_logs();
    print_logs(l);
    expect(r.status == 503 && !b.empty() && !l.empty() && after.stats.ok_count == before.stats.ok_count,
           "failed page allocation returns an explicit error without counting a successful stream");

    // Truncation: template and first copy succeed, reserve() of the rebuilt page fails.
    g_lab_alloc_fail_min = 16384;
    g_lab_alloc_skip_big = 2;
    g_lab_alloc_failed = 0;
    r = one_shot("GET", "/");
    g_lab_alloc_fail_min = 0;
    g_lab_alloc_skip_big = 0;
    const std::string t = body_of(r);
    l = take_logs();
    printf("  partial: failed allocations=%ld status=%d body=%u hasBody=%d hasScript=%d endsHtml=%d\n",
           g_lab_alloc_failed.load(), r.status, static_cast<unsigned>(t.size()),
           t.find("<body>") != std::string::npos, t.find("function setupNetworkItems") != std::string::npos,
           t.find("</html>") != std::string::npos);
    print_logs(l);
    expect(r.status == 503 || (r.status == 200 && t.find("function setupNetworkItems") != std::string::npos && t.find("</html>") != std::string::npos),
           "allocation pressure must not produce a successful truncated page");
    Reply c = one_shot("GET", "/");
    expect(c.status == 200 && body_of(c).find("</html>") != std::string::npos, "CONTROL: normal render complete");
    printf("  control body bytes=%u\n", static_cast<unsigned>(body_of(c).size()));
}

static void s7_fail_closed() {
    printf("\n== S7 WakePowerGuard admission ==\n");
    set_mode(true);
    Reply ok = one_shot("GET", "/wifi?scan_status=1");
    expect(ok.status == 200, "CONTROL: Idle phase serves request");

    WakePowerGuard::request(millis());
    uint32_t t0 = millis();
    Reply pq = one_shot("GET", "/wifi?scan_status=1", 6000);
    printf("  PreQuiet (owner lost): status=%d after %u ms\n", pq.status, millis() - t0);
    expect(pq.status == 200 && millis() - t0 >= 1800, "CONTROL: PreQuiet failsafe reopens after ~2 s");

    WakePowerGuard::latchFailClosed();
    t0 = millis();
    SOCKET s = cl_connect();
    cl_send(s, get_req("GET", "/wifi?scan_status=1"));
    Reply held = cl_read(s, 3300);
    expect(held.status == 0 && held.timed_out && !held.closed,
           "FailClosed neither sends nor closes the HTTP socket without admission");
    expect(WakePowerGuard::phase(millis()) == WakePowerGuard::Phase::FailClosed,
           "HTTP must not reopen hardware admission");
    WakePowerGuard::cancel(); // Only the simulated hardware owner recovers.
    Reply delayed = cl_read(s, 2500);
    expect(delayed.status == 503 && delayed.complete,
           "expired request receives 503 only after hardware admission is restored");
    Reply recovered = one_shot("GET", "/wifi?scan_status=1");
    expect(recovered.status == 200, "HTTP recovers after the owner releases the guard");
    ::closesocket(s);
    Sleep(50);
}

static void s8_ssid() {
    printf("\n== S8 non-ASCII SSID through real scan rendering ==\n");
    WebWifiScanUtils::WifiScanRow rows[2];
    size_t n = 0;
    const String utf8 = "\xD0\x94\xD0\xBE\xD0\xBC-WiFi";  // Дом-WiFi
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, n, 2, utf8, -40, false, false);
    WebWifiScanUtils::addOrReplaceBestNetwork(rows, n, 2, String("HomeNet"), -50, false, false);
    const String html = WebWifiScanUtils::renderNetworkItemsHtml(rows, n);
    const std::string h = html.c_str();
    const size_t p = h.find("data-ssid=\"");
    printf("  first data-ssid attribute: %s\n", h.substr(p, 28).c_str());
    expect(h.find(std::string("data-ssid=\"") + utf8.c_str() + "\"") != std::string::npos,
           "UTF-8 SSID preserved in the submitted attribute");
    expect(h.find("data-ssid=\"HomeNet\"") != std::string::npos, "CONTROL: ASCII SSID preserved");
}

static void s10_power_save() {
    printf("\n== S10 power-save lease interleaving ==\n");
    g_ps_mode = WIFI_PS_MIN_MODEM;
    WifiPowerSaveGuard a, b;
    expect(a.suspend() && g_ps_mode == WIFI_PS_NONE, "first lease disables PS");
    g_ps_hold_restore = true;
    std::thread restore([&] { a.restore(); });
    const uint32_t start = millis();
    while (!g_ps_restore_entered && millis() - start < 1000) Sleep(1);
    std::atomic<bool> suspended{false};
    std::thread suspend([&] { suspended = b.suspend(); });
    Sleep(30);
    const bool serialized = !suspended;
    g_ps_hold_restore = false;
    restore.join(); suspend.join();
    expect(serialized && suspended && g_ps_mode == WIFI_PS_NONE,
           "restore cannot overwrite a concurrent active lease");
    expect(WifiPowerSaveGuard::disableByDefault(), "baseline can change under an active lease");
    b.restore();
    expect(g_ps_mode == WIFI_PS_NONE, "last lease honors the new NONE baseline");
    g_ps_fail_get = true;
    expect(!a.suspend() && !a.isActive(), "get failure cannot acquire a lease");
    g_ps_fail_get = false;
    g_ps_mode = WIFI_PS_MIN_MODEM; g_ps_fail_set = true;
    expect(!a.suspend() && !a.isActive(), "set failure cannot acquire a lease");
    g_ps_fail_set = false;
    expect(a.suspend() && b.suspend(), "nested leases acquire");
    a.restore(); a.restore();
    expect(g_ps_mode == WIFI_PS_NONE && b.isActive(), "duplicate release keeps the remaining lease active");
    b.restore();
    expect(g_ps_mode == WIFI_PS_MIN_MODEM, "final release restores the saved mode");
    g_ps_mode = WIFI_PS_NONE;
}

static void s9_error_lifecycle() {
    printf("\n== S9 transport error lifecycle ==\n");
    set_mode(true);
    for (const char *path : {"/test/plain-fail", "/test/finish-fail", "/test/setup-fail", "/test/stop-twice"}) {
        Reply failed = one_shot("GET", path, 2000);
        expect(failed.status == 0 && failed.closed && !failed.timed_out, "terminal error closes the affected session without another response");
        Reply next = one_shot("GET", "/wifi?scan_status=1");
        expect(next.status == 200 && next.complete, "next request survives setup/body/finish/explicit-stop failures");
    }
    SOCKET keep = cl_connect();
    for (int i=0;i<2;++i) {
        cl_send(keep, get_req("GET", "/wifi?scan_status=1"));
        Reply r = cl_read(keep, 2000);
        expect(r.status == 200 && r.complete, "successful keep-alive requests reset transport state");
    }
    ::closesocket(keep);
}

static void s11_scan_lifecycle() {
    printf("\n== S11 scan completion without Arduino events ==\n");
    WifiScanSession scan;
    int count = 0;
    auto wait_done = [&]() {
        const uint32_t start = millis();
        WifiScanSession::Result result;
        do { result = scan.poll(count); if (result != WifiScanSession::Result::Pending) return result; Sleep(1); }
        while (millis()-start < 3000);
        return WifiScanSession::Result::Pending;
    };
    auto reset_driver = [&]() { g_scan_complete = false; g_scan_start_error = ESP_OK; g_scan_stop_error = ESP_OK; g_scan_records.clear(); };
    auto wait_started = [&](int previous) { const uint32_t t=millis(); while(g_scan_starts==previous && millis()-t<1000) Sleep(1); };
    reset_driver();
    wifi_ap_record_t row{}; strcpy(reinterpret_cast<char *>(row.ssid), "Home"); row.rssi=-60; row.authmode=WIFI_AUTH_WPA2_PSK;
    g_scan_records.push_back(row); row.rssi=-40; g_scan_records.push_back(row);
    strcpy(reinterpret_cast<char *>(row.ssid), "Guest"); row.rssi=-70; row.authmode=WIFI_AUTH_OPEN; g_scan_records.push_back(row);
    int starts=g_scan_starts;
    expect(scan.start()==WIFI_SCAN_RUNNING, "scan starts without blocking the network owner"); wait_started(starts);
    expect(g_scan_blocking, "worker uses SDK blocking completion, not Arduino events");
    expect(scan.poll(count)==WifiScanSession::Result::Pending && scan.record(0)==nullptr, "live worker results remain private");
    g_scan_complete=true;
    expect(wait_done()==WifiScanSession::Result::Ready && count==2, "dropped SCAN_DONE cannot strand scan or subsequent STA transition");
    expect(scan.record(0) && scan.record(0)->rssi==-40 && scan.record(1)->auth_mode==WIFI_AUTH_OPEN,
           "worker preserves strongest duplicate and network security");
    scan.releaseResults();
    expect(!scan.active() && g_scan_clears>0, "completed worker releases its driver records and slot");

    reset_driver(); starts=g_scan_starts; scan.start(); wait_started(starts);
    const int deletes=g_scan_clears;
    g_scan_stop_error=ESP_FAIL;
    expect(!scan.cancel() && scan.active(), "failed stop cannot release live worker storage");
    const int held_starts=g_scan_starts;
    scan.start(); scan.releaseResults();
    expect(scan.active() && g_scan_starts==held_starts && g_scan_clears==deletes, "cancelled live worker cannot be replaced or freed");
    int stops=g_scan_stops; scan.poll(count);
    expect(g_scan_stops==stops, "failed stop retries are rate limited");
    g_scan_stop_error=ESP_OK; Sleep(1010);
    expect(wait_done()==WifiScanSession::Result::Cancelled && !scan.active(), "retry completes cancellation without any callback");

    reset_driver(); starts=g_scan_starts; scan.start(); wait_started(starts);
    expect(scan.poll(count)==WifiScanSession::Result::Pending, "previous completion cannot finish the next worker");
    g_scan_start_error=ESP_ERR_TIMEOUT; g_scan_complete=true;
    expect(wait_done()==WifiScanSession::Result::Failed && !scan.active(), "SDK scan timeout releases ownership without an event");

    reset_driver();
    for (int i=0;i<20;++i) {
        wifi_ap_record_t many{}; snprintf(reinterpret_cast<char *>(many.ssid),sizeof(many.ssid),"Network-%02d",i);
        many.rssi=-90+i; g_scan_records.push_back(many);
    }
    starts=g_scan_starts; scan.start(); wait_started(starts); g_scan_complete=true;
    const auto many_result=wait_done();
    bool strongest=many_result==WifiScanSession::Result::Ready && count==15;
    for(int i=0;i<count;++i) strongest=strongest && scan.record(i) && scan.record(i)->rssi>=-85;
    expect(strongest,"bounded worker storage retains the fifteen strongest unique networks");
    scan.releaseResults();

    reset_driver(); starts=g_scan_starts; scan.start(); wait_started(starts);
    const uint32_t deadline_start=millis();
    Sleep(10010);
    expect(wait_done()==WifiScanSession::Result::Cancelled && !scan.active() && millis()-deadline_start<11500,
           "scan deadline cancels a stalled SDK operation without waiting for SCAN_DONE");

    reset_driver(); g_lab_fail_scan_task=true;
    expect(scan.start()==WIFI_SCAN_FAILED && !scan.active(), "worker allocation failure leaves no pending scan");
    starts=g_scan_starts; scan.start(); scan.cancel(); g_scan_complete=true;
    expect(wait_done()==WifiScanSession::Result::Cancelled && !scan.active(), "cancellation racing worker startup does not leave a pending scan");

    reset_driver(); WakePowerGuard::latchFailClosed(); starts=g_scan_starts; scan.start();
    expect(wait_done()==WifiScanSession::Result::Failed && g_scan_starts==starts, "scan worker cannot enter the driver during hardware quiet window");
    WakePowerGuard::cancel();
}

static void s12_slow_reader() {
    printf("\n== S12 healthy slow reader ==\n");
    set_mode(true);
    g_lab_server_sndbuf = 2048;
    SOCKET s = cl_connect(2048);
    cl_send(s, get_req("GET", "/test/large"));
    std::string wire;
    DWORD timeout = 7000;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
    const uint32_t start = millis();
    bool complete = false;
    while (millis() - start < 25000) {
        char chunk[1024];
        int n = ::recv(s, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (n <= 0) break;
        wire.append(chunk, n);
        const size_t end = wire.find("\r\n\r\n");
        if (end != std::string::npos && chunked_complete(wire.substr(end + 4))) { complete = true; break; }
        Sleep(100); // Transfer lasts longer than one write budget, with progress.
    }
    const size_t end = wire.find("\r\n\r\n");
    const std::string body = end == std::string::npos ? "" : dechunk(wire.substr(end + 4));
    printf("  slow reader: complete=%d bytes=%u elapsed=%u wsa=%d\n", complete, static_cast<unsigned>(body.size()), millis()-start, WSAGetLastError());
    print_logs(take_logs());
    expect(complete && body == std::string(60*1024, 'x') && millis() - start > 5000,
           "slow progressing reader receives an exact complete body beyond five seconds");
    ::closesocket(s); g_lab_server_sndbuf = 0;
    Reply next = one_shot("GET", "/wifi?scan_status=1");
    expect(next.status == 200 && next.complete, "next client works after slow successful response");
}

static void s13_partial_write_deadline() {
    printf("\n== S13 partial raw sends share one deadline ==\n");
    set_mode(true);
    g_lab_partial_send_limit = 1;
    g_lab_send_delay_ms = 100;
    const uint32_t start = millis();
    Reply failed = one_shot("GET", "/wifi?scan_status=1", 7000);
    const uint32_t elapsed = millis() - start;
    g_lab_partial_send_limit = 0; g_lab_send_delay_ms = 0;
    printf("  partial headers: closed=%d elapsed=%u status=%d\n", failed.closed, elapsed, failed.status);
    expect(failed.closed && !failed.timed_out && elapsed >= 4000 && elapsed < 6500,
           "partial send_all iterations cannot restart the five-second response budget");
    Reply next = one_shot("GET", "/wifi?scan_status=1");
    expect(next.status == 200 && next.complete, "next client survives a partial ordinary-response timeout");
}

static void s14_restart_server() {
    printf("\n== S14 borrowed backend survives repeated SDK stop/restart ==\n");
    set_mode(true);
    for (int i = 0; i < 3; ++i) {
        g_backend->stop();
        g_backend->stop();
        g_backend->begin();
        Reply reply = one_shot("GET", "/wifi?scan_status=1");
        expect(reply.status == 200 && reply.complete,
               "same backend owns routes and request after stop/restart");
    }
}

static void s15_save_page_allocations() {
    printf("\n== S15 save page rejects every allocation failure ==\n");
    WebWifiPage::SavePageData data{};
    data.hostname = "aura-kitchen";
    for (int skip = 0; skip < 2; ++skip) {
        String html;
        g_lab_alloc_fail_min = 1; g_lab_alloc_skip_big = skip;
        g_lab_alloc_failed = 0;
        const bool success = WebWifiPage::renderSaveHtml(WebTemplates::kWifiSavePage, data, html);
        g_lab_alloc_fail_min = 0;
        expect(!success && html.isEmpty() && g_lab_alloc_failed > 0,
               "save page allocation failure cannot return partial success");
    }
    String html;
    expect(WebWifiPage::renderSaveHtml(WebTemplates::kWifiSavePage, data, html) &&
           html.indexOf("http://aura-kitchen.local/dashboard") >= 0 && html.indexOf("</html>") >= 0,
           "save page succeeds completely once allocations recover");
}

// Same scenarios compiled against 77bfb4b0 and the working tree.
static std::atomic<uint32_t> g_x_pending{0}, g_x_drained{0}, g_x_received{0};
static int g_x_reject_after = 0;
static void x_upload_chunk() {
    WebUpload u = g_ctx.server->upload();
    if (u.status == WebUploadStatus::Write) {
        g_x_received += u.currentSize;
        if (g_x_reject_after >= 0 && g_x_received >= static_cast<uint32_t>(g_x_reject_after)) g_ctx.server->rejectUpload();
    }
}
// Line-for-line send_upload_json_response() from WebOtaHandlers.cpp.
static void x_upload_final() {
    WebRequest &server = *g_ctx.server;
    WebResponseUtils::sendNoStoreHeaders(server);
    const size_t pending = server.pendingRequestBodyBytes();
    g_x_pending = static_cast<uint32_t>(pending);
    if (pending > 0) g_x_drained = static_cast<uint32_t>(server.drainPendingRequestBody(32UL * 1024UL, 1500));
    server.sendHeader("Connection", "close");
    if (g_x_reject_after < 0) server.send(200, "application/json", "{\"success\":true}");
    else server.send(400, "application/json", "{\"success\":false,\"error\":\"Image is not for this device\"}");
    server.stopClient();
}
static void x_upload_preflight() { g_ctx.server->rejectUpload(); }
static void x_echo_post() { g_ctx.server->send(200, "text/plain", "post-ok"); }
static void x_large_plain() {
    static const String body = [] { String t; t.reserve(60 * 1024); for (int i = 0; i < 60 * 1024; ++i) t += 'y'; return t; }();
    g_ctx.server->send(200, "text/plain", body.c_str());
}
static void x_form() { g_ctx.server->send(200, "text/html", R"XHTML(<!doctype html><title>u</title><pre id="o">pending</pre><script>
const blob=new Blob([new Uint8Array(3*1024*1024)]);
const f=new FormData(); f.append('ota_size',String(blob.size)); f.append('firmware',blob,'fw.bin');
const x=new XMLHttpRequest(); x.open('POST','/test/upload',true); x.timeout=0;
x.onload=()=>{document.getElementById('o').textContent='ONLOAD status='+x.status+' body='+x.responseText;};
x.onerror=()=>{document.getElementById('o').textContent='ONERROR status='+x.status;};
x.send(f);
</script>)XHTML"); }
static void x_register() {
    g_backend->onGet("/test/form", x_form);
    g_backend->onPostUpload("/test/upload", x_upload_final, x_upload_chunk);
    g_backend->onPost("/test/echo", x_echo_post);
    g_backend->onPostUpload("/test/preflight", x_upload_final, x_upload_chunk, x_upload_preflight);
}

struct UploadOutcome { int status = 0; bool complete = false; std::string body; int recv_wsa = 0; int send_wsa = 0; size_t sent = 0; uint32_t ms = 0; };
static UploadOutcome x_upload_once(const char *path, bool multipart, size_t file_bytes) {
    SOCKET s = cl_connect();
    const std::string boundary = "----labboundary", pre = multipart
        ? "--" + boundary + "\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"fw.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n" : "";
    const std::string post = multipart ? "\r\n--" + boundary + "--\r\n" : "";
    std::string hdr = std::string("POST ") + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: "
        + (multipart ? "multipart/form-data; boundary=" + boundary : std::string("application/octet-stream"))
        + "\r\nContent-Length: " + std::to_string(pre.size() + file_bytes + post.size()) + "\r\n\r\n";
    cl_send(s, hdr + pre);
    UploadOutcome o;
    const uint32_t start = millis();
    std::thread tx([&] {
        std::string block(1436, 'F');
        size_t left = file_bytes;
        while (left) {
            const int n = static_cast<int>(std::min(left, block.size()));
            const int r = ::send(s, block.c_str(), n, 0);
            if (r == SOCKET_ERROR) { o.send_wsa = WSAGetLastError(); return; }
            left -= r; o.sent += r;
        }
        ::send(s, post.c_str(), static_cast<int>(post.size()), 0);
    });
    Reply r = cl_read(s, 20000);
    o.ms = millis() - start;
    tx.join();
    o.status = r.status; o.complete = r.complete; o.body = body_of(r); o.recv_wsa = r.wsa;
    ::closesocket(s);
    Sleep(30);
    return o;
}
static void x1_upload_reject() {
    printf("\n== X1 rejected upload: production response path, 3 MiB body ==\n");
    set_mode(false);
    int delivered = 0;
    const int rounds = 12;
    for (int i = 0; i < rounds; ++i) {
        UploadOutcome o = x_upload_once("/test/upload", true, 3 * 1024 * 1024);
        if (o.status == 400 && o.complete && o.body == "{\"success\":false,\"error\":\"Image is not for this device\"}") ++delivered;
        printf("  round %2d: status=%d complete=%d sent=%u recv_wsa=%d send_wsa=%d ms=%u pending=%u drained=%u\n", i, o.status, o.complete,
               static_cast<unsigned>(o.sent), o.recv_wsa, o.send_wsa, o.ms, g_x_pending.load(), g_x_drained.load());
    }
    printf("  RESULT error response delivered %d/%d\n", delivered, rounds);
    expect(delivered == rounds, "all rejected uploads deliver their complete error response");
}
static void x2_oversize_post() {
    printf("\n== X2 oversize POST body (limit 64 KiB): 413 ==\n");
    set_mode(false);
    int delivered = 0;
    const int rounds = 12;
    for (int i = 0; i < rounds; ++i) {
        UploadOutcome o = x_upload_once("/test/echo", false, 256 * 1024);
        if (o.status == 413 && o.complete) ++delivered;
        printf("  round %2d: status=%d complete=%d sent=%u recv_wsa=%d send_wsa=%d ms=%u\n", i, o.status, o.complete,
               static_cast<unsigned>(o.sent), o.recv_wsa, o.send_wsa, o.ms);
    }
    printf("  RESULT 413 delivered %d/%d\n", delivered, rounds);
    expect(delivered == rounds, "all oversized POST requests deliver their complete 413");
}

static void s16_unread_body_errors() {
    x1_upload_reject();
    x2_oversize_post();
    expect(one_shot("GET", "/dashboard").status == 200, "next request survives repeated rejected bodies");
}

static void s17_unfinished_body() {
    printf("\n== S17 stalled sender does not monopolize rejection cleanup ==\n");
    set_mode(false);
    SOCKET s = cl_connect();
    const uint32_t start = millis();
    cl_send(s, "POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/octet-stream\r\nContent-Length: 999999999\r\n\r\nF");
    Reply rejected = cl_read(s, 4000);
    printf("  rejection status=%d elapsed=%u complete=%d\n", rejected.status, millis()-start, rejected.complete);
    expect(rejected.status == 413 && rejected.complete, "silent sender receives rejection without sending the declared body");
    // Keep its read side open: the server must bound its own cleanup, not wait
    // for this peer or a huge Content-Length before servicing the next client.
    Reply next = one_shot("GET", "/wifi?scan_status=1", 5000);
    expect(next.status == 404 && millis()-start < 3500, "silent sender cleanup has an absolute deadline");
    ::closesocket(s);
    s=cl_connect();
    cl_send(s, "POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\n\r\nF");
    std::atomic<bool> stop{false};
    std::thread tx([&] { while(!stop) { if (::send(s,"F",1,0)<0) break; Sleep(10); } });
    const uint32_t trickle_start=millis();
    Reply trickle=cl_read(s,3000);
    Reply following=one_shot("GET","/dashboard",4000);
    stop=true; tx.join(); ::closesocket(s);
    expect(trickle.status==413 && trickle.complete && following.status==200 && millis()-trickle_start<3500,
           "continuous trickle cannot extend the progress deadline");
}

static void s18_rejection_stages() {
    printf("\n== S18 preflight/mid-upload/malformed/405 and completed upload ==\n");
    set_mode(false);
    g_x_received=0; g_x_reject_after=0;
    auto before=x_upload_once("/test/preflight", true, 1024*1024);
    expect(before.status==400 && before.complete && g_x_received==0, "preflight rejection delivers JSON without invoking the upload writer");
    g_x_received=0; g_x_reject_after=64*1024;
    auto middle=x_upload_once("/test/upload", true, 1024*1024);
    expect(middle.status==400 && middle.complete && g_x_received>=64*1024 && g_x_received<1024*1024,
           "rejection after partial consumption preserves the full error response");
    auto malformed=x_upload_once("/test/upload", false, 256*1024);
    expect(malformed.status==400 && malformed.complete, "malformed upload type also preserves its error response");
    auto method=x_upload_once("/wifi", false, 256*1024);
    expect(method.status==405 && method.complete, "unsupported method with a large body receives 405 before close");
    g_x_received=0; g_x_reject_after=-1;
    auto success=x_upload_once("/test/upload", true, 1024*1024);
    expect(success.status==200 && success.complete && success.body=="{\"success\":true}" && g_x_received==1024*1024,
           "completed upload still consumes every byte and delivers success");
    g_x_reject_after=0;
    expect(one_shot("GET", "/dashboard").status==200, "next client survives every upload outcome");
}


// Model a browser which finishes its upload before reading an early response.
static void s19_sequential_slow_upload() {
    printf("\n== S19 sequential slow sender preserves rejection ==\n");
    set_mode(false);
    SOCKET s=cl_connect();
    const size_t total=1024*1024;
    cl_send(s,"POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(total)+"\r\n\r\n");
    std::string block(4096,'F');
    size_t sent=0;
    const uint32_t start=millis();
    while(sent<total) {
        const int n=::send(s,block.data(),static_cast<int>(block.size()),0);
        if(n<=0) break;
        sent+=static_cast<size_t>(n);
        Sleep(31);
    }
    Reply r=cl_read(s,4000);
    printf("  sent=%u elapsed=%u status=%d complete=%d\n",unsigned(sent),millis()-start,r.status,r.complete);
    expect(sent==total && millis()-start>6000 && r.status==413 && r.complete,
           "1 MiB at 128 KiB/s receives complete 413 after finishing its send");
    ::closesocket(s);
    expect(one_shot("GET","/dashboard").status==200,"next client survives slow rejected upload");
}

static void s20_close_limits() {
    printf("\n== S20 rejected bodies have time and byte limits ==\n");
    set_mode(false);
    for (bool paced : {false,true}) {
        SOCKET s=cl_connect();
        int sndbuf=8192;
        setsockopt(s,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<const char*>(&sndbuf),sizeof(sndbuf));
        DWORD timeout=2000;
        setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
        cl_send(s,"POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\n\r\nF");
        // Read the complete early response, then keep sending past its FIN.
        Reply r=cl_read(s,2000);
        const uint32_t start=millis();
        std::string block(32768,'F'); size_t sent=0;
        while(millis()-start<94000 && sent<64*1024*1024) {
            const int n=::send(s,block.data(),static_cast<int>(block.size()),0);
            if(n<=0) break;
            sent+=static_cast<size_t>(n);
            if(paced) Sleep(500);
        }
        Reply next=one_shot("GET","/dashboard",4000);
        const uint32_t elapsed=millis()-start;
        printf("  paced=%d sent=%u elapsed=%u\n",paced,unsigned(sent),elapsed);
        if(paced) expect(r.status==413 && r.complete && elapsed>=88000 && elapsed<93500 && next.status==200,
                         "useful continuous progress cannot extend the 90 s total drain limit");
        else expect(r.status==413 && r.complete && sent<12*1024*1024 && elapsed<5000 && next.status==200,
                    "fast endless sender is cut off at the byte bound, not its Content-Length");
        ::closesocket(s);
    }
}


static void s21_half_close() {
    printf("\n== S21 response FIN is visible while receive side remains open ==\n");
    set_mode(false);
    SOCKET s=cl_connect();
    cl_send(s,"POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3145728\r\n\r\n"+std::string(65536,'F'));
    Reply r=cl_read(s,3000);
    DWORD timeout=700;
    setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    char c; const int fin=::recv(s,&c,1,0);
    size_t sent=0; std::string block(4096,'F');
    for (int i=0;i<64;++i) {
        const int n=::send(s,block.data(),static_cast<int>(block.size()),0);
        if(n<=0) break;
        sent+=static_cast<size_t>(n); Sleep(5);
    }
    printf("  status=%d complete=%d recv=%d sent_after_fin=%u\n",r.status,r.complete,fin,unsigned(sent));
    expect(r.status==413 && r.complete && fin==0 && sent==256*1024,
           "half-close exposes FIN before body completion and still accepts client upload");
    ::closesocket(s);
    expect(one_shot("GET","/dashboard").status==200,"next client survives half-close cleanup");
}
static void s22_failed_send_skips_drain() {
    printf("\n== S22 failed rejection send must skip unread-body cleanup ==\n");
    set_mode(false);
    SOCKET s=cl_connect(); g_lab_fail_next_send=1;
    const uint32_t start=millis();
    cl_send(s,"POST /test/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\n\r\nF");
    Reply failed=cl_read(s,3000), next=one_shot("GET","/dashboard",3000);
    const uint32_t elapsed=millis()-start;
    printf("  status=%d closed=%d next=%d elapsed=%u\n",failed.status,failed.closed,next.status,elapsed);
    expect(failed.status==0 && failed.closed && next.status==200 && elapsed<700,
           "failed response bypasses drain and releases HTTP without a progress timeout");
    ::closesocket(s);
}
static SOCKET open_rejected_upload(size_t total, std::string &post) {
    SOCKET s=cl_connect();
    const std::string boundary="----wake-test";
    const std::string pre="--"+boundary+"\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"fw.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n";
    post="\r\n--"+boundary+"--\r\n";
    cl_send(s,"POST /test/upload HTTP/1.1\r\nHost: localhost\r\nContent-Type: multipart/form-data; boundary="+boundary+
              "\r\nContent-Length: "+std::to_string(pre.size()+total+post.size())+"\r\n\r\n"+pre);
    return s;
}
static size_t paced_send(SOCKET s,size_t total,uint32_t kib_per_sec) {
    std::string block(4096,'F'); size_t sent=0; const uint32_t start=millis();
    while(sent<total) {
        const int n=::send(s,block.data(),static_cast<int>(std::min(block.size(),total-sent)),0);
        if(n<=0) break;
        sent+=static_cast<size_t>(n);
        const uint32_t due=static_cast<uint32_t>(sent*1000ULL/(kib_per_sec*1024ULL));
        const uint32_t elapsed=millis()-start;
        if(due>elapsed) Sleep(due-elapsed);
    }
    return sent;
}
static bool wait_wake_ready(uint32_t timeout_ms, WifiScanSession *scan=nullptr) {
    const uint32_t start=millis();
    while(millis()-start<timeout_ms) {
        if(scan) scan->serviceWakeQuiescence();
        if(WakePowerGuard::evaluateSwitch(millis(),100,500).ready) return true;
        Sleep(1);
    }
    return false;
}
static void s23_wake_during_http_drain() {
    printf("\n== S23 wake pauses a live upload drain without losing its reply ==\n");
    set_mode(false); g_x_pending=0; g_x_reject_after=0; g_x_received=0;
    std::string post; SOCKET s=open_rejected_upload(1024*1024,post);
    size_t sent=0;
    std::thread tx([&]{sent=paced_send(s,1024*1024,128); cl_send(s,post);});
    const uint32_t wait=millis();
    while(g_x_pending==0 && millis()-wait<2000) Sleep(1);
    Sleep(350); // Final JSON was sent; teardown is draining this live body.
    g_lab_guard_fd=g_lab_last_accepted.load(); g_lab_guard_io=0;
    const uint32_t start=millis(); WakePowerGuard::request(start);
    const bool ready=wait_wake_ready(700); const uint32_t wake_ms=millis()-start;
    expect(ready && WakePowerGuard::beginSwitch(),"upload drain releases its lease for wake within 700 ms");
    if(ready) {
        Sleep(100); WakePowerGuard::beginSettle(millis(),100); Sleep(110);
        expect(WakePowerGuard::beginRenderWait(millis()),"wake remains owner-controlled through settle");
        Sleep(1700); // Longer than the drain's 1500 ms progress timeout.
        expect(WakePowerGuard::activeOperations()==0 && g_lab_guard_io==0,
               "no socket send/recv/shutdown/close in Switching, Settle or RenderWait");
        expect(WakePowerGuard::completeRenderWait(),"only the owner reopens admission");
    } else WakePowerGuard::cancel();
    tx.join(); Reply r=cl_read(s,3000); ::closesocket(s); g_lab_guard_fd=-1;
    printf("  wake_ms=%u sent=%u status=%d complete=%d protected_io=%d\n",wake_ms,unsigned(sent),r.status,r.complete,g_lab_guard_io.load());
    expect(sent==1024*1024 && r.status==400 && r.complete && body_of(r).find("Image is not for this device")!=std::string::npos,
           "drain resumes after a long protected pause and sequential sender receives complete rejection");
    expect(one_shot("GET","/dashboard").status==200,"next client survives paused teardown");
}
static void s24_slow_current_image() {
    printf("\n== S24 current-size rejected image at 104 and 80 KiB/s ==\n");
    set_mode(false); g_x_reject_after=0;
    const size_t total=3390144;
    for(uint32_t rate:{104U,80U}) {
        std::string post; SOCKET s=open_rejected_upload(total,post);
        const uint32_t start=millis(); const size_t sent=paced_send(s,total,rate); cl_send(s,post);
        Reply r=cl_read(s,4000); ::closesocket(s);
        printf("  rate=%u KiB/s total=%u sent=%u elapsed=%u status=%d complete=%d\n",rate,unsigned(total),unsigned(sent),millis()-start,r.status,r.complete);
        expect(sent==total && r.status==400 && r.complete && body_of(r).find("Image is not for this device")!=std::string::npos,
               "sequential slow sender receives complete JSON without retrying POST");
        expect(one_shot("GET","/dashboard").status==200,"next client survives slow current-size image");
    }
}
static void s25_wake_during_scan() {
    printf("\n== S25 scan quiesces before wake and keeps its lease on stop failure ==\n");
    WifiScanSession scan; int count=0;
    auto begin=[&] {
        g_scan_complete=false;g_scan_start_error=ESP_OK;g_scan_stop_error=ESP_OK;g_scan_records.clear();
        const int starts=g_scan_starts;scan.start();const uint32_t t=millis();
        while(g_scan_starts==starts && millis()-t<1000) Sleep(1);
        expect(g_scan_starts>starts && WakePowerGuard::activeOperations()>0,"blocking scan owns a lease before driver entry");
    };
    auto finish=[&] {
        const uint32_t t=millis();WifiScanSession::Result result;
        do {result=scan.poll(count);Sleep(1);} while(result==WifiScanSession::Result::Pending && millis()-t<2000);
        return result;
    };
    begin(); const int stops=g_scan_stops; const uint32_t t=millis();WakePowerGuard::request(t);
    const bool ready=wait_wake_ready(700,&scan);
    printf("  scan wake_ms=%u stops=%d\n",millis()-t,g_scan_stops.load()-stops);
    expect(ready && g_scan_stops>stops && WakePowerGuard::beginSwitch(),"network owner stops the existing scan before protected switch");
    const int after_stop=g_scan_stops; scan.serviceWakeQuiescence();
    expect(g_scan_protected_calls==0,"scan API calls never enter Switching/Settle/RenderWait/FailClosed");
    expect(g_scan_stops==after_stop && WakePowerGuard::activeOperations()==0,"quiescence cannot call SDK stop after worker lease release");
    WakePowerGuard::cancel();
    expect(finish()==WifiScanSession::Result::Cancelled && !scan.active(),"cancelled scan finishes without publishing stale results");
    begin();g_scan_stop_error=ESP_FAIL;WakePowerGuard::request(millis());
    expect(!wait_wake_ready(350,&scan) && WakePowerGuard::activeOperations()>0,
           "failed SDK stop cannot force open the hardware guard or release worker ownership");
    const auto errors=take_logs();
    expect(std::count_if(errors.begin(),errors.end(),[](const std::string &s){return s.find("scan stop failed")!=std::string::npos;})==1,
           "wake retries do not flood the log with the same driver failure");
    g_scan_stop_error=ESP_OK;
    expect(wait_wake_ready(700,&scan),"stop retry proceeds while admission remains closed");
    WakePowerGuard::cancel();expect(finish()==WifiScanSession::Result::Cancelled,"failed stop recovers without a lost event");
    begin();g_scan_complete=true;
    expect(finish()==WifiScanSession::Result::Ready,"new scan after wake has independent completion");scan.releaseResults();
}
static void s26_radio_transitions_wait_for_scan() {
    printf("\n== S26 radio destinations wait for live worker; latest intent retains erase ==\n");
    WifiScanSession scan; WifiScanTransition transitions; int count=0;
    g_scan_complete=false;g_scan_stop_error=ESP_FAIL;g_scan_start_error=ESP_OK;g_scan_records.clear();
    const int starts=g_scan_starts;scan.start();const uint32_t start=millis();
    while(g_scan_starts==starts && millis()-start<1000) Sleep(1);
    transitions.shutdown(true);scan.cancel();transitions.start(WifiScanTransition::Start::Ap);
    expect(!transitions.take(scan.active()) && transitions.pending(),"forget + AP cannot reconfigure radio while scan worker is alive");
    transitions.start(WifiScanTransition::Start::Sta);transitions.shutdown(false);
    expect(!transitions.take(scan.active()),"later OFF still waits for worker and cancels pending start");
    g_scan_stop_error=ESP_OK;scan.cancel();
    while(scan.poll(count)==WifiScanSession::Result::Pending && millis()-start<2000) Sleep(1);
    auto off=transitions.take(scan.active());
    expect(off.shutdown && off.erase_credentials && off.start==WifiScanTransition::Start::None,
           "OFF keeps the earlier credential erase and cannot resurrect AP/STA");
    expect(!transitions.take(false),"consumed radio transition cannot execute twice");
    transitions.shutdown(true);transitions.start(WifiScanTransition::Start::Ap);
    auto ap=transitions.take(false);
    expect(ap.shutdown && ap.erase_credentials && ap.start==WifiScanTransition::Start::Ap,
           "forget followed by enable applies shutdown/erase before the latest AP start");
}


static void s27_scan_start_stop_race() {
    printf("\n== S27 stop before SDK start must retry without releasing live worker ==\n");
    WifiScanSession scan;int count=0;g_scan_complete=false;g_scan_start_error=ESP_OK;g_scan_stop_error=ESP_OK;
    g_scan_start_gate=true; const int starts=g_scan_starts;scan.start();const uint32_t start=millis();
    while(g_scan_starts==starts && millis()-start<1000) Sleep(1);
    WakePowerGuard::request(millis());scan.serviceWakeQuiescence();
    expect(WakePowerGuard::activeOperations()>0 && scan.active() && scan.poll(count)==WifiScanSession::Result::Pending,
           "successful early stop cannot release a worker still entering SDK start");
    const int stops=g_scan_stops; g_scan_start_gate=false;
    expect(wait_wake_ready(700,&scan) && g_scan_stops>stops,
           "owner retries stop after driver start while normal network admission is closed");
    WakePowerGuard::cancel();
    const uint32_t until=millis();
    while(scan.poll(count)==WifiScanSession::Result::Pending && millis()-until<1000) Sleep(1);
    expect(!scan.active() && g_scan_protected_calls==0,"startup race terminates without stale worker or protected driver calls");
}
static void s28_idle_session_close_guard() {
    printf("\n== S28 SDK closes an idle session only after wake admission ==\n");
    set_mode(false);SOCKET s=cl_connect();cl_send(s,get_req("GET","/dashboard"));Reply r=cl_read(s,3000);
    const int fd=g_lab_last_accepted.load();
    auto closes=[&]{std::lock_guard<std::mutex> lock(g_fd_lock);return std::count(g_fd_events.begin(),g_fd_events.end(),std::make_pair('C',fd));};
    Sleep(100);const auto before=closes();WakePowerGuard::request(millis());
    const bool ready=wait_wake_ready(700);expect(ready && WakePowerGuard::beginSwitch(),"completed keepalive request leaves wake ready");
    ::closesocket(s);Sleep(250);
    expect(r.status==200 && closes()==before,"SDK session close waits outside the protected hardware phase");
    WakePowerGuard::cancel();
    const uint32_t resumed=millis();
    while(closes()==before && millis()-resumed<1000) Sleep(1);
    // Count this lifetime before opening another connection: Windows can
    // reuse the same numeric SOCKET, which must not count as a double close.
    const auto after=closes();
    Reply next=one_shot("GET","/dashboard",3000);
    printf("  closed_before=%u closed_after=%u next=%d\n",unsigned(before),unsigned(after),next.status);
    expect(next.status==200 && after==before+1,"deferred SDK close occurs exactly once after admission");
}

// Permanent ports of independent A26-A28 and strengthened Z10 (2026-11-01).
// Driver barriers make the ownership assertions independent of a chosen SDK
// delay. The existing test runner supplies a process timeout for broken code.
template<class Predicate>
static bool wait_for_test(Predicate done, uint32_t timeout_ms = 1000) {
    const uint32_t start = millis();
    while (!done() && millis() - start < timeout_ms) Sleep(1);
    return done();
}

static void reset_scan_driver() {
    g_scan_complete = false;
    g_scan_start_error = g_scan_stop_error = ESP_OK;
    g_scan_records.clear();
    g_scan_protected_calls = 0;
    g_scan_cleanup_entered = g_scan_stop_entered = false;
}

static bool start_test_scan(WifiScanSession &scan) {
    reset_scan_driver();
    const int before = g_scan_starts;
    return scan.start() == WIFI_SCAN_RUNNING &&
           wait_for_test([&] { return g_scan_starts > before; }) &&
           WakePowerGuard::activeOperations() > 0;
}

static bool finish_test_scan(WifiScanSession &scan) {
    g_scan_complete = true;
    int count = 0;
    const bool finished = wait_for_test([&] {
        return scan.poll(count) != WifiScanSession::Result::Pending;
    });
    scan.releaseResults();
    return finished && !scan.active() && WakePowerGuard::activeOperations() == 0;
}

static void s29_scan_stop_requires_live_lease() {
    printf("\n== S29 stop never enters SDK for a worker without a lease ==\n");
    WifiScanSession scan;
    reset_scan_driver();
    g_lab_hold_scan_task = true;
    expect(scan.start() == WIFI_SCAN_RUNNING &&
           wait_for_test([] { return g_lab_scan_task_waiting.load(); }),
           "scan task is scheduled but has not acquired its activity lease");
    WakePowerGuard::request(millis());
    const bool switched = wait_wake_ready(700) && WakePowerGuard::beginSwitch();
    expect(switched, "wake can switch before the deferred worker starts");
    const int stops = g_scan_stops;
    scan.serviceWakeQuiescence();
    expect(g_scan_stops == stops && g_scan_protected_calls == 0,
           "outside-admission service cannot call SDK stop without a live worker lease");
    WakePowerGuard::cancel();
    g_lab_hold_scan_task = false;
    expect(finish_test_scan(scan), "deferred cancellation leaves no worker or lease behind");
}

static void s30_scan_lease_covers_cleanup() {
    printf("\n== S30 driver result cleanup retains the worker lease ==\n");
    WifiScanSession scan;
    expect(start_test_scan(scan), "worker entered the driver with its lease");
    g_scan_hold_cleanup = true;
    g_scan_complete = true;
    expect(wait_for_test([] { return g_scan_cleanup_entered.load(); }),
           "completed scan is held inside driver cleanup");
    WakePowerGuard::request(millis());
    // No service call here: stop would wait on the mutex held by cleanup.
    const bool prematurely_ready = wait_wake_ready(250);
    expect(!prematurely_ready && WakePowerGuard::activeOperations() > 0,
           "wake cannot become ready until driver cleanup returns");
    g_scan_hold_cleanup = false;
    expect(wait_wake_ready(700) && WakePowerGuard::beginSwitch(),
           "wake becomes ready after cleanup releases the lease");
    expect(g_scan_protected_calls == 0, "driver cleanup never overlaps a protected phase");
    WakePowerGuard::cancel();
    expect(finish_test_scan(scan), "cleanup completion releases worker storage and lease");
}

static void s31_scan_stop_serializes_release() {
    printf("\n== S31 slow stop prevents concurrent cleanup and lease release ==\n");
    WifiScanSession scan;
    expect(start_test_scan(scan), "worker entered the driver with its lease");
    const int clears = g_scan_clears;
    g_scan_hold_stop = true;
    WakePowerGuard::request(millis());
    // This thread is the sole network owner. The test thread acts as the
    // wake owner; it does not concurrently poll or mutate WifiScanSession.
    std::thread network_owner([&] { scan.serviceWakeQuiescence(); });
    expect(wait_for_test([] { return g_scan_stop_entered.load(); }),
           "stop has completed the scan but is held inside the driver call");
    const bool prematurely_ready = wait_wake_ready(250);
    expect(!prematurely_ready && WakePowerGuard::activeOperations() > 0 && g_scan_clears == clears,
           "stop mutex prevents cleanup and lease release while SDK stop is still running");
    g_scan_hold_stop = false;
    network_owner.join();
    expect(wait_wake_ready(700) && WakePowerGuard::beginSwitch(),
           "wake proceeds once stop and cleanup have both returned");
    expect(g_scan_protected_calls == 0, "stop and cleanup never overlap a protected phase");
    WakePowerGuard::cancel();
    expect(finish_test_scan(scan), "cancelled worker finishes without leaking its lease");
}

static void s32_http_reacquires_between_wakes() {
    printf("\n== S32 rejected upload reacquires admission between two wakes ==\n");
    set_mode(false); g_x_pending = 0; g_x_reject_after = 0; g_x_received = 0;
    g_lab_half_closed_fd = -1;
    constexpr size_t total = 1024 * 1024;
    std::string post;
    SOCKET s = open_rejected_upload(total, post);
    size_t sent = 0;
    std::thread tx([&] { sent = paced_send(s, total, 128); cl_send(s, post); });
    expect(wait_for_test([] { return g_lab_half_closed_fd.load() >= 0; }, 2000),
           "rejection has entered half-close and unread-body drain");
    g_lab_guard_fd = g_lab_half_closed_fd.load(); g_lab_guard_io = 0;
    for (int cycle = 1; cycle <= 2; ++cycle) {
        // A full JSON alone cannot detect the missing reacquisition: it was
        // already sent before the first wake. Check live ownership explicitly.
        const bool has_lease = wait_for_test([] { return WakePowerGuard::activeOperations() > 0; }, 350);
        expect(has_lease, cycle == 1 ? "drain owns admission before the first wake" :
                                     "drain reacquires its activity lease between the two wakes");
        const uint32_t start = millis();
        WakePowerGuard::request(start);
        const bool switched = wait_wake_ready(700) && WakePowerGuard::beginSwitch();
        printf("  cycle=%d wake_ms=%u lease_before=%d\n", cycle, millis() - start, has_lease);
        expect(switched, "live drain yields its lease for this wake");
        if (switched) {
            Sleep(50); WakePowerGuard::beginSettle(millis(), 50); Sleep(60);
            const bool render = WakePowerGuard::beginRenderWait(millis());
            Sleep(100);
            expect(render && WakePowerGuard::activeOperations() == 0 && g_lab_guard_io == 0,
                   "protected phases contain no activity lease or socket operation");
            expect(WakePowerGuard::completeRenderWait(), "wake owner reopens admission");
        } else {
            WakePowerGuard::cancel();
        }
    }
    tx.join();
    Reply r = cl_read(s, 3000);
    ::closesocket(s); g_lab_guard_fd = -1;
    expect(sent == total && r.status == 400 && r.complete &&
           body_of(r).find("Image is not for this device") != std::string::npos,
           "both wake cycles preserve the complete JSON rejection");
    expect(one_shot("GET", "/dashboard").status == 200, "next client survives two drain pauses");
}

static int serve_mode(uint32_t seconds) {
    set_mode(true, "127.0.0.1:18080");
    g_lab_server_sndbuf = 5760;  // CONFIG_LWIP_TCP_SND_BUF_DEFAULT
    g_echo_requests = true;
    printf("serving AP-mode pages on http://127.0.0.1:%u/ for %u s\n", kPort, seconds);
    fflush(stdout);
    const uint32_t until = millis() + seconds * 1000;
    while (static_cast<int32_t>(millis() - until) < 0) Sleep(100);
    WebTransferSnapshot s = g_stream_state.snapshot(millis());
    printf("stream stats ok=%lu abort=%lu failures=%u\n", static_cast<unsigned long>(s.stats.ok_count),
           static_cast<unsigned long>(s.stats.abort_count), s.failure_count);
    return 0;
}

int main(int argc, char **argv) {
    timeBeginPeriod(1);
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
    setvbuf(stdout, nullptr, _IONBF, 0);
    g_ctx.server = nullptr;
    g_backend = createDefaultWebServerBackend(kPort);
    g_ctx.server = &g_backend->request();
    g_ctx.connectivity_runtime = &g_connectivity;
    g_ctx.wifi_start_scan = lab_wifi_start_scan;
    WebRouteRegistry::registerRoutes(*g_backend);
    g_backend->onGet("/test/plain-fail", test_plain_failure);
    g_backend->onGet("/test/finish-fail", test_finish_failure);
    g_backend->onGet("/test/setup-fail", test_setup_failure);
    g_backend->onGet("/test/stop-twice", test_stop_twice);
    g_backend->onGet("/test/large", test_large_stream);
    x_register();
    g_backend->begin();
    Sleep(150);
    std::thread net(network_thread);

    if (argc > 3) g_lab_send_delay_ms = atoi(argv[3]);
    if (argc > 2 && !strcmp(argv[1], "serve")) {
        serve_mode(static_cast<uint32_t>(atoi(argv[2])));
        g_network_thread_run = false; net.join(); g_backend->stop();
        return 0;
    }

    const std::string only = argc > 1 ? argv[1] : "";
    auto want = [&](const char *n) { return only.empty() || only == n; };
    if (want("s1")) s1_routes();
    if (want("s2")) s2_scan_stale();
    if (want("s3")) s3_early_close();
    if (want("s5")) { s5_double_close(true); s5_double_close(false); }
    if (want("s4")) { s4_stall_blocks_all(false); s4_stall_blocks_all(true); s4_stall_blocks_all(true, true); }
    if (want("s6")) s6_alloc_failure();
    if (want("s7")) s7_fail_closed();
    if (want("s8")) s8_ssid();
    if (want("s9")) s9_error_lifecycle();
    if (want("s10")) s10_power_save();
    if (want("s11")) s11_scan_lifecycle();
    if (want("s12")) s12_slow_reader();
    if (want("s13")) s13_partial_write_deadline();
    if (want("s14")) s14_restart_server();
    if (want("s15")) s15_save_page_allocations();
    if (want("s16")) s16_unread_body_errors();
    if (want("s17")) s17_unfinished_body();
    if (want("s18")) s18_rejection_stages();
    if (want("s19")) s19_sequential_slow_upload();
    if (want("s20")) s20_close_limits();
    if (want("s21")) s21_half_close();
    if (want("s22")) s22_failed_send_skips_drain();
    if (want("s23")) s23_wake_during_http_drain();
    if (want("s24")) s24_slow_current_image();
    if (want("s25")) s25_wake_during_scan();
    if (want("s26")) s26_radio_transitions_wait_for_scan();
    if (want("s27")) s27_scan_start_stop_race();
    if (want("s28")) s28_idle_session_close_guard();
    if (want("s29")) s29_scan_stop_requires_live_lease();
    if (want("s30")) s30_scan_lease_covers_cleanup();
    if (want("s31")) s31_scan_stop_serializes_release();
    if (want("s32")) s32_http_reacquires_between_wakes();
    printf("\nunexpected=%d\n", g_fail);
    fflush(stdout);
    g_network_thread_run = false;
    net.join();
    g_backend->stop();
    g_backend.reset();
    WSACleanup();
    timeEndPeriod(1);
    return g_fail ? 1 : 0;
}
