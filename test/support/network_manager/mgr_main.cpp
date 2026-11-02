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
void delay(uint32_t ms) { Sleep(ms); }
bool boot_board_cold_start = false;
MockWiFi WiFi;
MockMdns MDNS;

struct Mock {
    std::mutex m;
    std::vector<std::string> ops;
    std::atomic<int> mode{0};
    std::atomic<int> status{WL_DISCONNECTED};
    std::atomic<bool> in_scan{false}, stop_seen{false}, release{false}, stop_ok{true}, start_gate{false}, worker_gate{false}, task_fail{false}, low_heap{false}, ota{false};
    std::atomic<int> auto_ms{-1}, start_err{0}, clear_delay{0}, stop_delay{0};
    std::atomic<int> scan_starts{0}, scan_stops{0}, clears{0}, during_scan{0}, in_protected{0}, begins{0}, softaps{0}, off_mode{0}, erase{0}, workers{0};
    String hostname;
    void reset() {
        std::lock_guard<std::mutex> l(m); ops.clear(); mode = 0; status = WL_DISCONNECTED; in_scan = stop_seen = release = false; stop_ok = true; start_gate = worker_gate = task_fail = low_heap = ota = false;
        auto_ms = -1; start_err = 0; clear_delay = 0; stop_delay = 0; scan_starts = scan_stops = clears = during_scan = in_protected = begins = softaps = off_mode = erase = 0;
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
void *MockSta::netif() { return nullptr; }
void MockWiFi::persistent(bool) {}
wifi_event_id_t MockWiFi::onEvent(void (*)(arduino_event_id_t, arduino_event_info_t)) { return 1; }
bool MockWiFi::setHostname(const char *) { return true; }
bool MockWiFi::mode(wifi_mode_t m) { radio("mode(" + std::to_string((int)m) + ")"); g.mode = m; if (m == 0) ++g.off_mode; return true; }
wifi_mode_t MockWiFi::getMode() { return (wifi_mode_t)g.mode.load(); }
bool MockWiFi::disconnect(bool off, bool erase) { radio(std::string("disconnect(") + (off ? "off" : "keep") + "," + (erase ? "erase" : "keep") + ")"); if (erase) ++g.erase; if (off) g.mode = g.mode.load() & ~WIFI_MODE_STA; return true; }
bool MockWiFi::softAP(const char *) { radio("softAP"); ++g.softaps; return true; }
bool MockWiFi::enableAP(bool on) { radio(on ? "enableAP(1)" : "enableAP(0)"); if (!on) ap_off = millis(); if (!on) g.mode = g.mode.load() & ~WIFI_MODE_AP; return true; }
void MockWiFi::setAutoReconnect(bool) {}
wl_status_t MockWiFi::status() { return (wl_status_t)g.status.load(); }
wl_status_t MockWiFi::begin(const char *, const char *, int32_t, const uint8_t *, bool) { radio("begin"); ++g.begins; return WL_DISCONNECTED; }
wl_status_t MockWiFi::begin(const char *, wpa2_auth_method_t, const char *, const char *, const char *, const char *, const char *, const char *, int, int32_t, const uint8_t *, bool) { radio("begin-ent"); ++g.begins; return WL_DISCONNECTED; }
int MockWiFi::scanNetworks(bool, bool) { return 0; }
void MockWiFi::scanDelete() {}
uint32_t heap_caps_get_free_size(uint32_t) { return g.low_heap.load() ? 1000 : 200000; }
uint32_t heap_caps_get_largest_free_block(uint32_t) { return g.low_heap.load() ? 1000 : 100000; }
esp_err_t esp_netif_set_hostname(esp_netif_t *, const char *h) { g.hostname = h; return ESP_OK; }
esp_err_t esp_netif_get_hostname(esp_netif_t *, const char **h) { *h = g.hostname.c_str(); return ESP_OK; }

esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool block) {
    driver(block ? "scan_start(block)" : "scan_start(async)");
    if (g.mode.load() == 0) return ESP_ERR_WIFI_NOT_INIT;
    while (g.start_gate.load()) Sleep(1);
    ++g.scan_starts; g.stop_seen = false; g.release = false; g.in_scan = true;
    const uint32_t t = millis();
    while (!g.release.load() && !g.stop_seen.load()) {
        const int a = g.auto_ms.load();
        if (a >= 0 && (int)(millis() - t) >= a) break;
        Sleep(1);
    }
    g.in_scan = false; g.op("scan_end");
    return g.start_err.load();
}
esp_err_t esp_wifi_scan_stop() {
    driver("scan_stop"); ++g.scan_stops;
    if (!g.stop_ok.load()) return ESP_FAIL;
    if (g.in_scan.load()) g.stop_seen = true;   // A stop before the driver scan has no effect.
    if (g.stop_delay.load()) { Sleep(g.stop_delay.load()); driver("scan_stop_exit"); }
    return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list() { driver("clear_ap_list"); ++g.clears; if (g.clear_delay.load()) { Sleep(g.clear_delay.load()); driver("clear_ap_list_exit"); } cleanup_end = millis(); return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *n) { driver("ap_num"); *n = 2; return ESP_OK; }
static std::atomic<int> g_read{0};
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *r) {
    driver("ap_rec"); const int i = g_read++ % 2;
    snprintf((char *)r->ssid, sizeof(r->ssid), "net%d", i); r->rssi = (int8_t)(-40 - i); r->authmode = 3; return ESP_OK;
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

int main(int argc, char **argv) {
    timeBeginPeriod(1);
    void (*all[])() = {a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33};
    const int n = sizeof(all) / sizeof(all[0]);
    for (int i = 0; i < n; ++i) { char b[8]; snprintf(b, sizeof(b), "a%d", i + 1); if (argc < 2 || !strcmp(argv[1], b)) all[i](); }
    printf("\npassed=%d failed=%d\n", g_pass, g_fail); fflush(stdout);
    ExitProcess(g_fail ? 1 : 0);
}

extern "C" char *utoa(unsigned v, char *b, int) { sprintf(b, "%u", v); return b; }
extern "C" char *ultoa(unsigned long v, char *b, int) { sprintf(b, "%lu", v); return b; }
extern "C" char *dtostrf(double v, signed char w, unsigned char p, char *b) { sprintf(b, "%*.*f", w, p, v); return b; }
