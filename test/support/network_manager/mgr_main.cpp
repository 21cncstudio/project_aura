// Independent stand: the real AuraNetworkManager + WifiScanSession + WifiScanTransition
// + WakePowerGuard, driven the way network_plane_task drives them. Wi-Fi is a mock: this
// proves the manager wiring only, not driver or radio behaviour.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <esp_heap_caps.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>
#include "core/Logger.h"
#include "core/WakePowerGuard.h"
#include "modules/NetworkManager.h"
#include "web/WebDeferredActionsState.h"
#include <mmsystem.h>
static WebDeferredActionsState deferred;
static WebHandlerContext *web_context = nullptr;
static std::atomic<uint32_t> cleanup_end{0}, ap_off{0};

// ---- platform ----
static const auto g_t0 = std::chrono::steady_clock::now();
uint32_t millis() { return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_t0).count(); }
static std::atomic<bool> hold_home_delay{false}, home_delay_entered{false}, home_delay_left{false};
void delay(uint32_t ms) {
    if (ms==10 && hold_home_delay.load()) {
        home_delay_entered=true;
        while (hold_home_delay.load()) Sleep(1);
        home_delay_left=true;
    }
    Sleep(ms);
}
bool boot_board_cold_start = false;
MockWiFi WiFi;
MockMdns MDNS;
static void (*wifi_callback)(arduino_event_id_t, arduino_event_info_t) = nullptr;

struct Mock {
    std::mutex m;
    std::vector<std::string> ops;
    std::atomic<int> mode{0};
    std::atomic<int> status{WL_DISCONNECTED};
    std::atomic<bool> in_scan{false}, stop_seen{false}, release{false}, stop_ok{true}, start_gate{false}, worker_gate{false}, task_fail{false}, low_heap{false}, ota{false};
    std::atomic<int> auto_ms{-1}, start_err{0}, clear_delay{0}, stop_delay{0};
    std::atomic<int> scan_starts{0}, scan_stops{0}, clears{0}, during_scan{0}, in_protected{0}, begins{0}, softaps{0}, off_mode{0}, erase{0}, workers{0};
    std::atomic<int> sync_scans{0}, pinned_begins{0};
    bool connect_start_ok=true,fail_during_connect=false;
    std::atomic<int> country_first{1},country_count{1},country_error{ESP_OK},country_reads{0},current_channel{0},fail_channel{-1},clear_error{ESP_OK};
    std::atomic<bool> per_channel_records{false};
    std::atomic<int> country_policy{WIFI_COUNTRY_POLICY_MANUAL};
    std::atomic<bool> hold_stop{false}, stop_entered{false};
    std::vector<int> scan_types;
    std::vector<unsigned> channels;
    std::vector<uint32_t> channel_starts,channel_ends;
    String hostname;
    void reset() {
        std::lock_guard<std::mutex> l(m); ops.clear(); mode = 0; status = WL_DISCONNECTED; in_scan = stop_seen = release = false; stop_ok = true; start_gate = worker_gate = task_fail = low_heap = ota = false;
        country_first=country_count=1;country_error=ESP_OK;country_reads=0;current_channel=0;fail_channel=-1;clear_error=ESP_OK;per_channel_records=false;
        country_policy=WIFI_COUNTRY_POLICY_MANUAL;hold_stop=false;stop_entered=false;home_delay_left=false;scan_types.clear();
        channels.clear();channel_starts.clear();channel_ends.clear();hold_home_delay=false;home_delay_entered=false;
        auto_ms = -1; start_err = 0; clear_delay = 0; stop_delay = 0; scan_starts = scan_stops = clears = during_scan = in_protected = begins = softaps = off_mode = erase = 0;
        sync_scans = pinned_begins = 0;
        connect_start_ok=true;fail_during_connect=false;
    }
    void op(const std::string &s) { std::lock_guard<std::mutex> l(m); ops.push_back(std::to_string(millis()) + " " + s); }
} g;

static bool protected_phase() {
    const auto p = WakePowerGuard::phase(millis());
    return p == WakePowerGuard::Phase::Switching || p == WakePowerGuard::Phase::Settle ||
           p == WakePowerGuard::Phase::RenderWait || p == WakePowerGuard::Phase::FailClosed;
}
static void radio(const std::string &name) {
    g.op(name);
    if (g.in_scan.load()) { ++g.during_scan; g.op("VIOLATION radio change during live scan: " + name); }
    if (protected_phase()) { ++g.in_protected; g.op("VIOLATION radio change in protected phase: " + name); }
}
static void driver(const std::string &name) {
    g.op(name);
    if (protected_phase()) { ++g.in_protected; g.op("VIOLATION driver call in protected phase: " + name); }
}
bool MockSta::started() { return (g.mode.load() & WIFI_MODE_STA) != 0; }
bool MockSta::connect(const char *,const char *,int32_t channel,const uint8_t *bssid,bool) {
    radio("STA.connect");++g.begins;if(channel || bssid)++g.pinned_begins;
    if(g.fail_during_connect){arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);}
    return g.connect_start_ok;
}
bool MockSta::connect(const char *,wpa2_auth_method_t,const char *,const char *,const char *,const char *,const char *,const char *,int,int32_t,const uint8_t *,bool) { radio("STA.connect-ent");++g.begins;return g.connect_start_ok; }
void *MockSta::netif() { return nullptr; }
void MockWiFi::persistent(bool) {}
wifi_event_id_t MockWiFi::onEvent(void (*cb)(arduino_event_id_t, arduino_event_info_t)) { wifi_callback=cb; return 1; }
bool MockWiFi::setHostname(const char *) { return true; }
bool MockWiFi::mode(wifi_mode_t m) { radio("mode(" + std::to_string((int)m) + ")"); g.mode = m; if (m == 0) ++g.off_mode; return true; }
wifi_mode_t MockWiFi::getMode() { return (wifi_mode_t)g.mode.load(); }
bool MockWiFi::disconnect(bool off, bool erase) { radio(std::string("disconnect(") + (off ? "off" : "keep") + "," + (erase ? "erase" : "keep") + ")"); if (erase) ++g.erase; if (off) g.mode = g.mode.load() & ~WIFI_MODE_STA; return true; }
bool MockWiFi::softAP(const char *) { radio("softAP"); ++g.softaps; return true; }
bool MockWiFi::enableAP(bool on) { radio(on ? "enableAP(1)" : "enableAP(0)"); if (!on) ap_off = millis(); if (!on) g.mode = g.mode.load() & ~WIFI_MODE_AP; return true; }
void MockWiFi::setAutoReconnect(bool) {}
wl_status_t MockWiFi::status() { return (wl_status_t)g.status.load(); }
wl_status_t MockWiFi::begin(const char *, const char *, int32_t channel, const uint8_t *bssid, bool) { radio("begin"); ++g.begins; if (channel || bssid) ++g.pinned_begins; return status(); }
wl_status_t MockWiFi::begin(const char *, wpa2_auth_method_t, const char *, const char *, const char *, const char *, const char *, const char *, int, int32_t, const uint8_t *, bool) { radio("begin-ent"); ++g.begins; return WL_DISCONNECTED; }
int MockWiFi::scanNetworks(bool, bool) { ++g.sync_scans; return 0; }
void MockWiFi::scanDelete() {}
uint32_t heap_caps_get_free_size(uint32_t) { return g.low_heap.load() ? 1000 : 200000; }
uint32_t heap_caps_get_largest_free_block(uint32_t) { return g.low_heap.load() ? 1000 : 100000; }
esp_err_t esp_netif_set_hostname(esp_netif_t *, const char *h) { g.hostname = h; return ESP_OK; }
esp_err_t esp_netif_get_hostname(esp_netif_t *, const char **h) { *h = g.hostname.c_str(); return ESP_OK; }

esp_err_t esp_wifi_get_country(wifi_country_t *country) {
    driver("get_country");++g.country_reads;country->schan=g.country_first;country->nchan=g.country_count;country->policy=static_cast<wifi_country_policy_t>(g.country_policy.load());return g.country_error;
}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block) {
    driver(block ? "scan_start(block)" : "scan_start(async)");
    if (g.mode.load() == 0) return ESP_ERR_WIFI_NOT_INIT;
    while (g.start_gate.load()) Sleep(1);
    g.current_channel=config->channel;
    { std::lock_guard<std::mutex> l(g.m);g.channels.push_back(config->channel);g.scan_types.push_back(config->scan_type);g.channel_starts.push_back(millis()); }
    ++g.scan_starts; g.stop_seen = false; g.release = false; g.in_scan = true;
    const uint32_t t = millis();
    while (!g.release.load() && !g.stop_seen.load()) {
        const int a = g.auto_ms.load();
        if (a >= 0 && (int)(millis() - t) >= a) break;
        Sleep(1);
    }
    g.in_scan = false; g.op("scan_end");
    { std::lock_guard<std::mutex> l(g.m);g.channel_ends.push_back(millis()); }
    return config->channel==g.fail_channel ? ESP_FAIL : g.start_err.load();
}
esp_err_t esp_wifi_scan_stop() {
    driver("scan_stop"); ++g.scan_stops;
    // Barrier is deliberately inside the SDK stop call.
    g.stop_entered=true;while(g.hold_stop.load())Sleep(1);
    if (!g.stop_ok.load()) return ESP_FAIL;
    if (g.in_scan.load()) g.stop_seen = true;   // A stop before the driver scan has no effect.
    if (g.stop_delay.load()) { Sleep(g.stop_delay.load()); driver("scan_stop_exit"); }
    return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list() { driver("clear_ap_list"); ++g.clears; if (g.clear_delay.load()) { Sleep(g.clear_delay.load()); driver("clear_ap_list_exit"); } cleanup_end = millis(); return g.clear_error; }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *n) { driver("ap_num"); *n = 2; return ESP_OK; }
static std::atomic<int> g_read{0};
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *r) {
    driver("ap_rec"); const int i = g_read++ % 2;
    if (g.per_channel_records) {
        if (i==0) strcpy((char*)r->ssid,"shared");
        else snprintf((char*)r->ssid,sizeof(r->ssid),"channel-%d",g.current_channel.load());
        r->rssi=-80+g.current_channel.load();
    } else { snprintf((char *)r->ssid, sizeof(r->ssid), "net%d", i); r->rssi = (int8_t)(-40 - i); }
    r->authmode = 3; return ESP_OK;
}
extern "C" BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *, uint32_t, void *arg, UBaseType_t, TaskHandle_t *, BaseType_t) {
    if (g.task_fail.load()) return pdFAIL;
    ++g.workers;
    std::thread([fn, arg] { while (g.worker_gate.load()) Sleep(1); fn(arg); --g.workers; }).detach();
    return pdPASS;
}
extern "C" void vTaskDelete(TaskHandle_t) {}
std::vector<std::string> g_warns;
void Logger::log(Level level, const char *tag, const char *fmt, ...) {
    if (level > Warn) return;
    char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    std::lock_guard<std::mutex> l(g.m); g_warns.push_back(std::string(tag) + ": " + b);
}
std::unique_ptr<WebServerBackend> createDefaultWebServerBackend(uint16_t) { return std::unique_ptr<WebServerBackend>(new WebServerBackend()); }
void WebHandlersInit(WebHandlerContext *ctx) { web_context = ctx; }
bool WebHandlersIsOtaBusy() { return g.ota.load(); }
void WebHandlersCancelWifiSave() { deferred.cancelWifiSave(); }
void WebHandlersPollDeferred() {
    if (!web_context) return;
    const auto due = deferred.pollDue(millis(), web_context->wifi_prepare_sta);
    if (due.wifi_start_sta && web_context->wifi_start_sta) web_context->wifi_start_sta();
}

// ---- harness ----
static int g_fail = 0, g_pass = 0;
static void expect(bool ok, const char *what) { printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (ok) ++g_pass; else ++g_fail; fflush(stdout); }
static AuraNetworkManager *nm = nullptr;
static StorageManager *st = nullptr;

// Same order as network_plane_task.
static void tick() {
    nm->serviceWakeQuiescence();
    WakePowerGuard::Activity a = WakePowerGuard::tryAcquireActivity(millis());
    if (a) nm->poll();
    Sleep(2);
}
template <class F> static void command(F f) {   // NetworkCommandQueue::processAll under admission
    WakePowerGuard::Activity a = WakePowerGuard::tryAcquireActivity(millis());
    if (!a) { printf("  command had no admission\n"); return; }
    f();
}
template <class P> static bool until(P p, uint32_t ms) {
    const uint32_t t = millis();
    while (millis() - t < ms) { if (p()) return true; tick(); }
    return p();
}
static void run_for(uint32_t ms) { const uint32_t t = millis(); while (millis() - t < ms) tick(); }
static bool full_wake(uint32_t max_ms, uint32_t *waited = nullptr) {
    const uint32_t t = millis();
    WakePowerGuard::request(t);
    while (true) {
        tick();
        const uint32_t now = millis();
        if (WakePowerGuard::phase(now) == WakePowerGuard::Phase::Idle) WakePowerGuard::request(now);
        if (WakePowerGuard::evaluateSwitch(now, 100, 500).ready) break;
        if (now - t > max_ms) { WakePowerGuard::cancel(); return false; }
    }
    if (waited) *waited = millis() - t;
    if (!WakePowerGuard::beginSwitch()) { WakePowerGuard::cancel(); return false; }
    run_for(100); WakePowerGuard::beginSettle(millis(), 100); run_for(110);
    WakePowerGuard::beginRenderWait(millis()); run_for(100); WakePowerGuard::completeRenderWait();
    return true;
}
// Wake owner on its own thread; the caller keeps servicing the network loop.
static bool threaded_wake(uint32_t max_ms, uint32_t *waited) {
    std::atomic<bool> done{false}; bool ok = false; uint32_t w = 0;
    std::thread owner([&] {
        const uint32_t t = millis();
        WakePowerGuard::request(t);
        while (true) {
            const uint32_t now = millis();
            if (WakePowerGuard::phase(now) == WakePowerGuard::Phase::Idle) WakePowerGuard::request(now);
            if (WakePowerGuard::evaluateSwitch(now, 100, 500).ready) break;
            if (now - t > max_ms) { WakePowerGuard::cancel(); done = true; return; }
            Sleep(1);
        }
        w = millis() - t;
        if (!WakePowerGuard::beginSwitch()) { WakePowerGuard::cancel(); done = true; return; }
        Sleep(300); WakePowerGuard::beginSettle(millis(), 100); Sleep(110);
        WakePowerGuard::beginRenderWait(millis()); Sleep(100); WakePowerGuard::completeRenderWait();
        ok = true; done = true;
    });
    while (!done.load()) tick();
    owner.join();
    if (waited) *waited = w;
    return ok;
}
static void boot(bool enabled, const char *ssid) {
    LabDns::reset();
    deferred.reset(); web_context = nullptr; cleanup_end = 0; ap_off = 0;
    g.reset(); g_warns.clear(); WakePowerGuard::resetForTest();
    st = new StorageManager(); st->stored.enabled = enabled; st->stored.ssid = ssid; st->stored.pass = ssid[0] ? "pw" : "";
    nm = new AuraNetworkManager(); nm->begin(*st);
}
static void finish() {
    deferred.reset();
    g.release = true; g.stop_ok = true; g.auto_ms = 0;
    const uint32_t t = millis();
    uint32_t idle_since = millis();
    while (millis() - t < 4000) {   // A restart after a cancelled scan is legitimate work.
        tick(); g.release = true;
        if (g.workers.load() > 0) idle_since = millis();
        if (millis() - idle_since >= 120) break;
    }
    expect(g.during_scan.load() == 0, "no radio reconfiguration while a driver scan was live");
    expect(g.in_protected.load() == 0, "no driver or radio call in Switching/Settle/RenderWait");
    expect(g.workers.load() == 0, "worker finished");
    if (g_fail) { std::lock_guard<std::mutex> l(g.m); for (auto &s : g.ops) printf("    %s\n", s.c_str()); for (auto &s : g_warns) printf("    W %s\n", s.c_str()); }
    delete nm; nm = nullptr; delete st; st = nullptr;
}
static Config::WifiSettings saved(const char *ssid) { Config::WifiSettings s; s.ssid = ssid; s.pass = "pw"; s.enabled = true; return s; }
static bool scan_live() { return g.in_scan.load(); }
#define S(name) printf("\n== %s ==\n", name)

static void a1() { S("A1 AP boot: scan result is published");
    boot(true, ""); g.auto_ms = 50; until([] { return !nm->scanInProgress() && nm->scanOptions().length() > 0; }, 1500);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG && nm->scanOptions().indexOf("net0") >= 0, "AP state, scan rows published");
    finish(); }
static void a2() { S("A2 OFF during scan");
    boot(true, ""); until(scan_live, 500); const int off = g.off_mode;
    command([] { nm->setEnabled(false); });
    expect(g.off_mode == off && nm->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG, "OFF does not touch the radio synchronously");
    until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 800);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF && g.off_mode == off + 1 && g.erase == 0 && g.softaps == 1 && g.begins == 0, "radio off once after worker exit; no erase, no restart");
    finish(); }
static void a3() { S("A3 forget during scan, Wi-Fi stays enabled");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->clearCredentials(); });
    expect(g.erase == 0, "erase deferred until scan ended");
    until([] { return g.softaps >= 2; }, 800); run_for(30);
    expect(g.erase == 1 && g.softaps == 2 && g.begins == 0 && st->clears == 1, "erase once, AP restarted, stored credentials cleared");
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG && g.scan_starts == 2, "AP config state and a new scan");
    finish(); }
static void a4() { S("A4 save network during AP scan (main setup path)");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); });
    expect(g.begins == 0, "STA does not start under a live scan");
    until([] { return g.begins > 0; }, 1200); run_for(30);
    expect(g.begins == 1 && nm->state() == AuraNetworkManager::WIFI_STATE_STA_CONNECTING && g.softaps == 1, "one STA begin, no AP restart");
    finish(); }
static void a5() { S("A5 OFF then ON, no credentials");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->setEnabled(false); nm->setEnabled(true); });
    until([] { return g.softaps >= 2; }, 800); run_for(30);
    expect(g.off_mode == 1 && g.softaps == 2 && nm->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG, "shutdown first, then AP");
    finish(); }
static void a6() { S("A6 OFF then ON, with credentials");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->setEnabled(false); nm->setEnabled(true); });
    until([] { return g.begins > 0; }, 1200); run_for(30);
    expect(g.off_mode >= 1 && g.begins == 1 && nm->state() == AuraNetworkManager::WIFI_STATE_STA_CONNECTING, "shutdown first, then STA");
    finish(); }
static void a7() { S("A7 forget, OFF, ON: erase survives");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->clearCredentials(); nm->setEnabled(false); nm->setEnabled(true); });
    until([] { return g.softaps >= 2; }, 800); run_for(30);
    expect(g.erase == 1 && g.softaps == 2, "erase kept, AP started after it");
    finish(); }
static void a8() { S("A8 STA pending then OFF");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); nm->setEnabled(false); });
    until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 800); run_for(50);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF && g.begins == 0 && g.softaps == 1, "OFF wins, no STA and no AP");
    finish(); }
static void a9() { S("A9 stop fails, driver then ends by itself");
    boot(true, ""); until(scan_live, 500); g.stop_ok = false;
    command([] { nm->setEnabled(false); });
    run_for(1300);
    expect(g.off_mode == 0 && nm->state() == AuraNetworkManager::WIFI_STATE_AP_CONFIG && g.scan_stops >= 2, "stop retried, radio untouched");
    g.release = true; until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 500);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF && g.off_mode == 1, "OFF runs after the natural end");
    finish(); }
static void a10() { S("A10 stop fails and driver hangs: OFF stays pending");
    boot(true, ""); until(scan_live, 500); g.stop_ok = false;
    command([] { nm->setEnabled(false); });
    run_for(3000);
    expect(g.off_mode == 0 && scan_live(), "radio not reconfigured under a hung driver");
    uint32_t w = 0; const bool woke = full_wake(1500, &w);
    expect(!woke, "guard stays closed while the worker holds its lease");
    g.stop_ok = true; until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 2500);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF, "recovers when stop works again");
    finish(); }
static void a11() { S("A11 wake during AP scan: stop, protected phases, restart");
    boot(true, ""); until(scan_live, 500); uint32_t w = 0;
    expect(full_wake(1000, &w), "wake reached the switch"); printf("  wake wait %u ms\n", w);
    expect(g.scan_starts == 1, "no scan restart inside the wake");
    expect(until([] { return g.scan_starts == 2; }, 500), "scan restarted after admission reopened");
    g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    expect(nm->scanOptions().indexOf("net0") >= 0, "second scan published rows");
    finish(); }
static void a12() { S("A12 wake right after scan end, before results are polled");
    boot(true, ""); until(scan_live, 500);
    WakePowerGuard::request(millis()); g.release = true;
    while (g.workers.load() > 0) { Sleep(1); WakePowerGuard::request(millis()); }
    WakePowerGuard::cancel(); WakePowerGuard::request(millis());
    uint32_t w = 0; expect(full_wake(1000, &w), "wake completed");
    printf("  options after wake: '%s' starts=%d\n", nm->scanOptions().substring(0, 20).c_str(), g.scan_starts.load());
    expect(until([] { return g.scan_starts == 2; }, 500), "finished but unread results are dropped, scan restarts");
    finish(); }
static void a13() { S("A13 wake while worker has not reached its lease");
    boot(true, ""); until(scan_live, 500); g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    g.worker_gate = true; command([] { nm->startScan(); });
    uint32_t w = 0; WakePowerGuard::request(millis()); tick(); g.worker_gate = false;
    expect(full_wake(1000, &w), "wake completed");
    expect(until([] { return g.scan_starts == 2; }, 600), "scan restarted after wake");
    finish(); }
static void a14() { S("A14 OTA start stops the scan, no restart after it");
    boot(true, ""); until(scan_live, 500); g.ota = true; run_for(100);
    expect(!scan_live() && g.scan_starts == 1, "OTA stopped the scan");
    g.ota = false; run_for(120); expect(g.scan_starts == 1, "no automatic scan after OTA");
    finish(); }
static void a15() { S("A15 wake cancel with OTA busy: restart waits for OTA end");
    boot(true, ""); until(scan_live, 500); g.ota = true; uint32_t w = 0;
    WakePowerGuard::request(millis()); expect(full_wake(1000, &w), "wake completed");
    run_for(80); const int starts = g.scan_starts;
    g.ota = false; run_for(120); printf("  starts before=%d after=%d\n", starts, g.scan_starts.load());
    expect(starts == 1, "no scan during OTA");
    expect(g.scan_starts == 2, "scan interrupted by wake restarts once OTA ends");
    finish(); }
static void a16() { S("A16 scan request during cancellation restarts once");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->stopScan(); nm->startScan(); });
    until([] { return g.scan_starts == 2; }, 600); run_for(50);
    expect(g.scan_starts == 2, "exactly one restart");
    finish(); }
static void a17() { S("A17 scan request during a pending STA leaves no stale restart");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); nm->startScan(); });
    until([] { return g.begins > 0; }, 1200); run_for(150);
    expect(g.scan_starts == 1 && g.begins == 1, "no scan after leaving AP");
    finish(); }
static void a18() { S("A18 worker task allocation failure");
    boot(true, ""); until(scan_live, 500); g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    g.task_fail = true; command([] { nm->startScan(); }); run_for(50);
    expect(!nm->scanInProgress(), "not stuck in progress"); g.task_fail = false;
    finish(); }
static void a19() { S("A19 driver scan error");
    boot(true, ""); until(scan_live, 500); g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    expect(nm->scanOptions().length() > 0, "first scan published rows");
    command([] { nm->startScan(); }); until(scan_live, 300); g.start_err = ESP_FAIL; g.release = true;
    until([] { return !nm->scanInProgress(); }, 500); run_for(30);
    expect(nm->scanOptions().isEmpty() && g.scan_starts == 2, "failed rescan clears the list, no retry loop"); g.start_err = 0;
    finish(); }
static void a20() { S("A20 low heap on the deferred STA start");
    boot(true, ""); until(scan_live, 500); g.low_heap = true;
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); });
    until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 800);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF && g.begins == 0, "deferred start reports low heap");
    g.low_heap = false; until([] { return g.begins > 0; }, 1500);
    expect(g.begins == 1, "retry path starts STA");
    finish(); }
static void a21() { S("A21 stop before driver entry: retry reaches the driver");
    boot(true, ""); until(scan_live, 500); g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    g.start_gate = true; command([] { nm->startScan(); }); Sleep(50);
    command([] { nm->setEnabled(false); }); Sleep(20); g.start_gate = false;
    const uint32_t t = millis(); until([] { return nm->state() == AuraNetworkManager::WIFI_STATE_OFF; }, 2500);
    printf("  OFF applied after %u ms\n", millis() - t);
    expect(nm->state() == AuraNetworkManager::WIFI_STATE_OFF, "OFF eventually applies");
    finish(); }
static void a22() { S("A22 wake while a radio transition is pending");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); });
    uint32_t w = 0; expect(full_wake(1000, &w), "wake completed");
    expect(until([] { return g.begins == 1; }, 800), "STA starts after admission reopens");
    finish(); }
static void a23() { S("A23 scan deadline: driver hangs until stop");
    boot(true, ""); until(scan_live, 500); const uint32_t t = millis();
    until([] { return !nm->scanInProgress(); }, 11000);
    printf("  ended after %u ms stops=%d\n", millis() - t, g.scan_stops.load());
    expect(!nm->scanInProgress() && g.scan_stops >= 1 && g.scan_starts == 1, "10 s deadline stops it; no retry");
    finish(); }
static void a24() { S("A24 scan request with Wi-Fi off");
    boot(false, ""); command([] { nm->startScan(); }); run_for(60);
    expect(!nm->scanInProgress() && g.during_scan == 0, "fails cleanly");
    finish(); }
static void a25() { S("A25 repeated wake during AP scans does not lose the list");
    boot(true, ""); g.auto_ms = 300; until([] { return !nm->scanInProgress() && nm->scanOptions().length() > 0; }, 1500);
    const String first = nm->scanOptions(); command([] { nm->startScan(); });
    until(scan_live, 300); uint32_t w = 0; expect(full_wake(1000, &w), "wake completed");
    expect(nm->scanOptions() == first, "previous list stays during restart");
    finish(); }

static void a26() { S("A26 service never calls the driver without a live lease");
    boot(true, ""); until(scan_live, 500); g.release = true; until([] { return !nm->scanInProgress(); }, 500);
    g.worker_gate = true; command([] { nm->startScan(); });
    const int stops = g.scan_stops; const uint32_t t = millis(); WakePowerGuard::request(t);
    while (!WakePowerGuard::evaluateSwitch(millis(), 100, 500).ready && millis() - t < 800) Sleep(1);
    expect(WakePowerGuard::beginSwitch(), "switch began while the worker had no lease yet");
    tick(); tick();
    expect(g.scan_stops == stops, "no scan_stop in Switching from the outside-admission service");
    WakePowerGuard::cancel(); g.worker_gate = false;
    finish(); }
static void a27() { S("A27 lease covers driver result cleanup");
    boot(true, ""); until(scan_live, 500); g.clear_delay = 350; uint32_t w = 0;
    expect(threaded_wake(1500, &w), "wake completed"); printf("  wake wait %u ms with 350 ms cleanup\n", w);
    expect(w >= 350, "wake waited for the cleanup");
    finish(); }
static void a28() { S("A28 mutex keeps the lease alive through a slow stop");
    boot(true, ""); until(scan_live, 500); g.stop_delay = 350; uint32_t w = 0;
    expect(threaded_wake(1500, &w), "wake completed"); printf("  wake wait %u ms with 350 ms stop\n", w);
    expect(w >= 350, "wake waited for the stop call to return");
    finish(); }
static void a29() { S("A29 scan request during pending STA, then back to AP: no stale extra scan");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); nm->connectSta(); nm->startScan(); });
    until([] { return g.begins > 0; }, 1200); run_for(30);
    const int starts = g.scan_starts; g.auto_ms = 40;
    command([] { nm->startApOnDemand(); }); run_for(300);
    printf("  scans after return to AP: %d\n", g.scan_starts.load() - starts);
    expect(g.scan_starts == starts + 1, "exactly the scan started by AP entry");
    finish(); }


static void a30() { S("A30 save grace begins after worker cleanup, not stop acknowledgement");
    boot(true, ""); expect(until(scan_live, 500), "scan entered driver");
    g.clear_delay = 350;
    command([] { nm->applySavedWiFiSettings(saved("home")); });
    deferred.scheduleWifiStartSta(200);
    expect(until([] { return g.begins == 1; }, 1500), "saved STA eventually starts");
    printf("  cleanup=%u ap_off=%u grace=%u\n",cleanup_end.load(),ap_off.load(),ap_off.load()-cleanup_end.load());
    expect(cleanup_end && ap_off && ap_off.load()-cleanup_end.load() >= 200,
           "AP survives at least 200 ms after actual worker cleanup");
    finish(); }
static void a31() { S("A31 failed stop cannot spend the save grace");
    boot(true, ""); until(scan_live, 500); g.stop_ok = false;
    command([] { nm->applySavedWiFiSettings(saved("home")); });
    deferred.scheduleWifiStartSta(200); run_for(450);
    expect(g.begins == 0 && !ap_off && scan_live(), "AP stays up while stop fails beyond the grace");
    g.stop_ok = true;
    expect(until([] { return g.begins == 1; }, 1500), "retry eventually completes the transition");
    expect(ap_off.load()-cleanup_end.load() >= 200, "retry success gets a fresh full grace");
    finish(); }
static void a32() { S("A32 Rescan during save grace restarts the grace");
    boot(true, ""); until(scan_live, 500);
    command([] { nm->applySavedWiFiSettings(saved("home")); });
    deferred.scheduleWifiStartSta(200);
    expect(until([] { return !nm->scanInProgress(); }, 500), "first scan stops");
    run_for(60); g.stop_seen = false; g.release = false; g.clear_delay = 100;
    command([] { nm->startScan(); });
    // Hold the owner between commands: let the worker actually enter the
    // driver before the next poll can cancel it (deterministic overlap).
    const uint32_t entered = millis();
    while (!scan_live() && millis()-entered < 500) Sleep(1);
    expect(scan_live(), "second scan entered driver");
    expect(until([] { return g.begins == 1; }, 1500), "STA starts after second scan");
    expect(ap_off.load()-cleanup_end.load() >= 200, "second worker completion restarts grace");
    finish(); }
static void a33() { S("A33 no active scan still preserves save grace");
    boot(true, ""); until(scan_live, 500); g.release = true;
    expect(until([] { return !nm->scanInProgress(); }, 500), "scan already completed");
    command([] { nm->applySavedWiFiSettings(saved("home")); });
    const uint32_t start = millis(); deferred.scheduleWifiStartSta(200);
    run_for(100); expect(!ap_off, "AP remains available during grace");
    expect(until([] { return g.begins == 1; }, 600), "STA starts without another scan");
    expect(ap_off.load()-start >= 200, "no-scan save has full grace too");
    finish(); }

static void a34() { S("A34 captive DNS is scoped to AP and closes before STA");
    boot(true, "");expect(LabDns::live==1,"AP starts DNS");
    g.release=true;until([]{return !nm->scanInProgress();},500);run_for(20);
    expect(LabDns::polls>0,"AP polls DNS");
    command([]{nm->applySavedWiFiSettings(saved("home"));nm->connectSta();});
    until([]{return g.begins>0;},1000);
    expect(LabDns::live==0,"DNS closed on STA transition");
    const int polls=LabDns::polls;run_for(50);expect(LabDns::polls==polls,"STA does not poll DNS");finish(); }
static void a35() { S("A35 DNS follows OFF and wake admission");
    boot(true, "");g.release=true;until([]{return !nm->scanInProgress();},500);
    WakePowerGuard::request(millis());const int polls=LabDns::polls;
    const uint32_t start=millis();while(!WakePowerGuard::evaluateSwitch(millis(),100,500).ready && millis()-start<700)tick();
    expect(WakePowerGuard::beginSwitch(),"DNS does not hold an independent wake lease");run_for(50);
    expect(LabDns::polls==polls,"DNS polling stops during wake protection");WakePowerGuard::cancel();
    command([]{nm->setEnabled(false);});until([]{return nm->state()==AuraNetworkManager::WIFI_STATE_OFF;},1000);
    expect(LabDns::live==0,"OFF closes DNS");finish(); }

static void a36() { S("A36 failed DNS start retries without spinning");
    boot(false, "");LabDns::fail_start=true;
    command([]{nm->setEnabled(true);});
    expect(until([]{return LabDns::starts>0;},1000),"AP attempts DNS start");
    const int starts=LabDns::starts;g.release=true;run_for(100);
    expect(LabDns::live==0 && LabDns::starts==starts,"failed start is rate limited");
    LabDns::fail_start=false;
    expect(until([]{return LabDns::live==1;},5500),"later poll recovers DNS");
    expect(LabDns::starts==starts+1,"exactly one recovery attempt");finish(); }

// The ownership cases above use a one-channel regulatory fixture. These cases
// exercise the real worker across a full multi-channel scan without a manager
// restart racing the assertions. Driver results remain deterministic shims.
static WifiScanSession::Result wait_scan(WifiScanSession &scan,int &count) {
    const uint32_t start=millis();
    do { auto r=scan.poll(count);if(r!=WifiScanSession::Result::Pending)return r;Sleep(1); }
    while(millis()-start<2500);
    scan.cancel();return WifiScanSession::Result::Pending;
}
static void a37() { S("A37 SoftAP scan preserves home airtime and regulatory channels");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_first=3;g.country_count=6;g.per_channel_records=true;
    WifiScanSession scan;int count=0;scan.start();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Ready,"multi-channel scan completes");
    bool order=false,gaps=true;
    { std::lock_guard<std::mutex> l(g.m);order=g.channels==std::vector<unsigned>({3,4,5,6,7,8});
      for(size_t i=1;i<g.channel_starts.size();++i)gaps &= g.channel_starts[i]-g.channel_ends[i-1]>=120; }
    expect(order,"each allowed channel scanned once, with no out-of-country channels");
    expect(gaps && g.scan_starts==6,"at least 120 ms home airtime between channels");
    expect(g.clears==6,"every channel's SDK list is released");
    bool strongest=false,first=false,last=false;
    for(int i=0;i<count;++i){auto *r=scan.record(i);if(!r)continue;
      if(!strcmp(r->ssid,"shared"))strongest=r->rssi==-72;
      first |= !strcmp(r->ssid,"channel-3");last |= !strcmp(r->ssid,"channel-8");}
    expect(count==7 && strongest && first && last,"results aggregate all channels and deduplicate by strongest RSSI");
    expect(WakePowerGuard::activeOperations()==0,"completed scan releases its lease");scan.releaseResults();finish(); }
static void a38() { S("A38 wake cancels during home airtime without starting another channel");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=6;hold_home_delay=true;
    WifiScanSession scan;int count=0;scan.start();const uint32_t t=millis();
    while(!home_delay_entered && millis()-t<1000)Sleep(1);
    expect(home_delay_entered && g.scan_starts==1,"worker is between channel scans");
    expect(WakePowerGuard::activeOperations()==1,"home dwell retains the worker lease");
    WakePowerGuard::request(millis());scan.serviceWakeQuiescence();
    expect(WakePowerGuard::activeOperations()==1,"cancellation retains lease until worker exits home dwell");
    hold_home_delay=false;
    expect(wait_scan(scan,count)==WifiScanSession::Result::Cancelled,"home-wait cancellation completes");
    expect(g.scan_starts==1 && g.clears==1,"cancelled worker cannot enter the next channel");
    expect(WakePowerGuard::activeOperations()==0,"home cancellation releases the wake lease");
    WakePowerGuard::cancel();finish(); }
static void a39() { S("A39 a later channel failure discards partial results");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=6;g.fail_channel=3;
    WifiScanSession scan;int count=0;scan.start();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Failed,"later SDK failure is reported");
    expect(g.scan_starts==3 && g.clears==3,"failure stops further scans and cleans the failed channel");
    expect(count==0 && scan.record(0)==nullptr,"partial list is not published as a complete scan");finish(); }
static void a40() { S("A40 invalid or unavailable country never falls back to an unbounded AP scan");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;
    for(int variant=0;variant<4;++variant){g.country_error=variant==0?ESP_FAIL:ESP_OK;g.country_first=variant==1?0:1;g.country_count=variant==2?0:(variant==3?15:1);
      WifiScanSession scan;int count=0;scan.start();expect(wait_scan(scan,count)==WifiScanSession::Result::Failed,"invalid regulatory input fails closed");}
    expect(g.scan_starts==0 && g.clears==0 && WakePowerGuard::activeOperations()==0,"no scan or leaked lease after country error");finish(); }
static void a41() { S("A41 STA-only scans keep SDK all-channel behavior");
    boot(false,"");g.mode=WIFI_MODE_STA;g.auto_ms=0;g.country_count=14;
    WifiScanSession scan;int count=0;scan.start();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Ready,"STA scan succeeds");
    expect(g.scan_starts==1 && g.current_channel==0 && g.country_reads==0,"STA delegates all channels to the SDK");scan.releaseResults();finish(); }
static void a42() { S("A42 failed result cleanup forbids another channel");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=6;g.clear_error=ESP_FAIL;
    WifiScanSession scan;int count=0;scan.start();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Failed,"cleanup failure fails the scan");
    expect(g.scan_starts==1 && g.clears==1 && WakePowerGuard::activeOperations()==0,"no further SDK list after cleanup failure");finish(); }

static void a43() { S("A43 AUTO discovers passive channels outside the active country range");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=11;g.country_policy=WIFI_COUNTRY_POLICY_AUTO;g.per_channel_records=true;
    WifiScanSession scan;int count=0;scan.start();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Ready,"AUTO scan completes");
    bool modes=true;{std::lock_guard<std::mutex> l(g.m);
      modes=g.channels.size()==14 && g.scan_types.size()==14;
      for(size_t i=0;modes && i<14;++i)modes &= g.channels[i]==i+1 && g.scan_types[i]==(i<11?WIFI_SCAN_TYPE_ACTIVE:WIFI_SCAN_TYPE_PASSIVE);}
    expect(modes,"world-safe AUTO probes 1-11 and only listens on 12-14");
    bool ch13=false;for(int i=0;i<count;++i)ch13 |= !strcmp(scan.record(i)->ssid,"channel-13");
    expect(ch13 && count==15,"channel 13 AP is included in aggregated results");
    expect(g.clears==14 && WakePowerGuard::activeOperations()==0,"all passive lists and the worker lease are released");scan.releaseResults();finish(); }
static void a44() { S("A44 final lease release waits for stop during home dwell");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=6;hold_home_delay=true;
    WifiScanSession scan;int count=0;scan.start();uint32_t t=millis();
    while(!home_delay_entered && millis()-t<1000)Sleep(1);
    expect(home_delay_entered,"worker is past cleanup and inside home dwell");
    g.hold_stop=true;WakePowerGuard::request(millis());
    std::thread owner([&]{scan.serviceWakeQuiescence();});t=millis();
    while(!g.stop_entered && millis()-t<1000)Sleep(1);
    expect(g.stop_entered,"network owner holds worker mutex inside stop");
    hold_home_delay=false;t=millis();while(!home_delay_left && millis()-t<1000)Sleep(1);
    Sleep(250);
    expect(home_delay_left && WakePowerGuard::activeOperations()==1,"final release cannot bypass the stop mutex after home dwell");
    g.hold_stop=false;owner.join();
    expect(wait_scan(scan,count)==WifiScanSession::Result::Cancelled,"worker completes after stop unlocks");
    expect(g.scan_starts==1 && WakePowerGuard::activeOperations()==0,"no next channel or leaked lease");WakePowerGuard::cancel();finish(); }

static void a45() { S("A45 AUTO lower passive range returns to active dwell");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_first=3;g.country_count=6;g.country_policy=WIFI_COUNTRY_POLICY_AUTO;
    WifiScanSession scan;int count=0;scan.start();expect(wait_scan(scan,count)==WifiScanSession::Result::Ready,"AUTO non-default range completes");
    bool correct=true;{std::lock_guard<std::mutex> l(g.m);correct=g.channels.size()==14;
      for(size_t i=0;correct && i<14;++i)correct &= g.scan_types[i]==((i>=2 && i<8)?WIFI_SCAN_TYPE_ACTIVE:WIFI_SCAN_TYPE_PASSIVE);}
    expect(correct,"both sides of configured range are passive; configured range stays active");scan.releaseResults();finish(); }
static void a46() { S("A46 MANUAL channel 13 remains active and does not scan 14");
    boot(false,"");g.mode=WIFI_MODE_APSTA;g.auto_ms=0;g.country_count=13;
    WifiScanSession scan;int count=0;scan.start();expect(wait_scan(scan,count)==WifiScanSession::Result::Ready,"MANUAL 1-13 completes");
    bool correct=true;{std::lock_guard<std::mutex> l(g.m);correct=g.channels.size()==13 && g.channels.back()==13;
      for(auto mode:g.scan_types)correct &= mode==WIFI_SCAN_TYPE_ACTIVE;}
    expect(correct,"MANUAL probes only its configured 13 channels");scan.releaseResults();finish(); }

static void a47() { S("A47 cold boot delegates AP selection to the driver");
    boot_board_cold_start=true;boot(true,"home");boot_board_cold_start=false;
    expect(until([]{return g.begins==1;},7000),"cold warmup leads to a first connection");
    expect(g.sync_scans==0 && g.pinned_begins==0,"no synchronous pre-scan or BSSID/channel pinning");
    expect(g.off_mode==1,"cold warmup keeps its existing single radio reset");
    g.status=WL_CONNECTED;tick();
    expect(nm->isConnected(),"first connection can complete normally");finish(); }

static void a48() { S("A48 terminal authentication failure uses bounded owner retries");
    boot(true,"home");expect(until([]{return g.begins==1;},4000),"STA attempt starts");
    // The real Arduino callback status is independently exercised by the overlay test.
    for(unsigned attempt=0;attempt<=Config::WIFI_CONNECT_MAX_RETRIES;++attempt) {
        const int before=g.begins;g.status=WL_DISCONNECTED;
        arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;
        wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
        expect(nm->state()==AuraNetworkManager::WIFI_STATE_OFF,"terminal failure ends attempt on next poll");
        expect(g.begins==before,"no immediate busy-loop retry");
        if(attempt<Config::WIFI_CONNECT_MAX_RETRIES) {
            expect(until([&]{return g.begins==before+1;},1000),"owner retries after its scheduled delay");
            expect(nm->retryCount()==attempt+1,"retry budget advances once");
        } else {
            run_for(600);expect(g.begins==before,"exhausted budget enters background backoff");
        }
    }
    command([]{nm->setEnabled(false);});tick();
    expect(g.erase==0,"authentication failures never erase stored credentials");finish(); }

static void a49() { S("A49 stale Arduino failure cannot cancel a fresh attempt");
    boot(true,"home");expect(until([]{return g.begins==1;},4000),"first attempt starts");
    g.status=WL_CONNECT_FAILED;arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;
    wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
    expect(until([]{return g.begins==2;},1000),"second attempt starts after failure event");
    run_for(500);
    expect(g.status==WL_CONNECT_FAILED && g.begins==2 && nm->state()==AuraNetworkManager::WIFI_STATE_STA_CONNECTING,"old status alone cannot reject the second attempt");
    g.status=WL_CONNECTED;tick();expect(nm->isConnected() && nm->retryCount()==0,"second attempt completes and resets budget");finish(); }

static void a50() { S("A50 synchronous connect rejection and early failure callback are retained");
    for(int variant=0;variant<2;++variant) {
        boot(true,"home");g.connect_start_ok=variant!=0;g.fail_during_connect=variant==1;
        expect(until([]{return g.begins==1;},4000),"connect API invoked");tick();
        expect(nm->state()==AuraNetworkManager::WIFI_STATE_OFF && nm->retryCount()==1,"both immediate rejection and callback during connect enter bounded retry");
        command([]{nm->setEnabled(false);});tick();finish();
    }
}

static void a51() { S("A51 startup retry is quiet only before first success and within 30 seconds");
    boot(true,"home");expect(until([]{return g.begins==1;},4000),"initial attempt starts");
    expect(nm->isStartupStaRetryQuiet(millis()),"startup attempt is in informational grace");
    expect(!nm->isStartupStaRetryQuiet(millis()+30000),"grace has a finite 30-second deadline");
    const size_t warnings=g_warns.size();arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;
    wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
    expect(g_warns.size()==warnings,"initial auth retry produces no warning");
    expect(until([]{return g.begins==2;},1000),"quiet logging does not postpone retry");
    g.status=WL_CONNECTED;tick();expect(!nm->isStartupStaRetryQuiet(millis()),"first successful connection ends grace");
    wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);
    expect(g_warns.size()==warnings+1,"failure after success remains a warning");finish(); }

static void a52() { S("A52 exhausted startup attempts remain visible and manual reconnect is not quiet");
    boot(true,"home");expect(until([]{return g.begins==1;},4000),"initial attempt starts");
    arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;
    for(unsigned attempt=0;attempt<Config::WIFI_CONNECT_MAX_RETRIES;++attempt){
        const int before=g.begins;wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
        expect(until([&]{return g.begins==before+1;},1000),"bounded retry still runs during grace");
    }
    expect(!nm->isStartupStaRetryQuiet(millis()),"last allowed attempt has no grace");
    const size_t warnings=g_warns.size();wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
    expect(g_warns.size()>=warnings+2,"terminal auth failure and exhausted budget both warn");
    command([]{nm->connectSta();});
    expect(!nm->isStartupStaRetryQuiet(millis()),"manual connect cannot rearm startup grace");finish(); }

static void a53() { S("A53 explicit reconnect cancels unexpired startup grace");
    boot(true,"home");expect(until([]{return g.begins==1;},4000),"initial attempt starts");
    expect(nm->isStartupStaRetryQuiet(millis()),"grace initially armed");
    command([]{nm->connectSta();});expect(!nm->isStartupStaRetryQuiet(millis()),"explicit reconnect cancels grace immediately");
    const size_t warnings=g_warns.size();arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_AUTH_FAIL;
    wifi_callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);tick();
    expect(g_warns.size()>=warnings+2,"manual connection failure is visible inside original 30-second window");finish(); }

int main(int argc, char **argv) {
    timeBeginPeriod(1);
    void (*all[])() = {a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42,a43,a44,a45,a46,a47,a48,a49,a50,a51,a52,a53};
    const int n = sizeof(all) / sizeof(all[0]);
    for (int i = 0; i < n; ++i) { char b[8]; snprintf(b, sizeof(b), "a%d", i + 1); if (argc < 2 || !strcmp(argv[1], b)) all[i](); }
    printf("\npassed=%d failed=%d\n", g_pass, g_fail); fflush(stdout);
    ExitProcess(g_fail ? 1 : 0);
}

extern "C" char *utoa(unsigned v, char *b, int) { sprintf(b, "%u", v); return b; }
extern "C" char *ultoa(unsigned long v, char *b, int) { sprintf(b, "%lu", v); return b; }
extern "C" char *dtostrf(double v, signed char w, unsigned char p, char *b) { sprintf(b, "%*.*f", w, p, v); return b; }
