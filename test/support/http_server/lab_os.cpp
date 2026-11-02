// OS shims for the host lab. Sockets bind only to 127.0.0.1.
#include <Arduino.h>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <mmsystem.h>
#include "lab.h"
#include "core/WakePowerGuard.h"

extern "C" { int g_lab_esp_log = 1; }
std::atomic<int> g_lab_last_accepted{-1}, g_lab_guard_fd{-1}, g_lab_guard_io{0};
void lab_check_guard_io(int fd) {
    if (fd != g_lab_guard_fd) return;
    const auto p=WakePowerGuard::phase(millis());
    if (p == WakePowerGuard::Phase::Switching || p == WakePowerGuard::Phase::Settle ||
        p == WakePowerGuard::Phase::RenderWait || p == WakePowerGuard::Phase::FailClosed)
        ++g_lab_guard_io;
}

std::atomic<long> g_lab_alloc_fail_min{0};      // >0: fail realloc() of at least this many bytes
std::atomic<long> g_lab_alloc_skip_big{0};      // big allocations to allow before failing
std::atomic<long> g_lab_alloc_failed{0};
std::atomic<int> g_lab_server_sndbuf{0};
std::atomic<int> g_lab_stall_fd{-1};            // emulate a peer that stopped ACKing
std::atomic<int> g_lab_fail_next_send{0};
std::atomic<int> g_lab_fail_after_bytes{-1};
std::atomic<bool> g_lab_fail_scan_task{false};
std::atomic<bool> g_lab_hold_scan_task{false}, g_lab_scan_task_waiting{false};
std::atomic<int> g_lab_half_closed_fd{-1};
std::atomic<int> g_lab_partial_send_limit{0};
std::atomic<int> g_lab_send_delay_ms{0};        // emulate a slow radio link per send()

extern "C" void *lab_realloc(void *ptr, size_t size) {
    const long min = g_lab_alloc_fail_min.load();
    if (min > 0 && static_cast<long>(size) >= min) {
        if (g_lab_alloc_skip_big.load() > 0) {
            g_lab_alloc_skip_big.fetch_sub(1);
        } else {
            g_lab_alloc_failed.fetch_add(1);
            return nullptr;
        }
    }
    return realloc(ptr, size);
}

static void set_errno_from_wsa() {
    switch (WSAGetLastError()) {
        case WSAECONNRESET: errno = ECONNRESET; break;
        case WSAECONNABORTED: errno = ECONNABORTED; break;
        case WSAESHUTDOWN: errno = EPIPE; break;
        case WSAENOTCONN: errno = ENOTCONN; break;
        case WSAETIMEDOUT:   // lwIP reports SO_SNDTIMEO/SO_RCVTIMEO expiry as EAGAIN.
        case WSAEWOULDBLOCK: errno = EAGAIN; break;
        case WSAENOTSOCK: errno = ENOTSOCK; break;
        case WSAEINVAL: errno = EINVAL; break;
        case WSAEINTR: errno = EINTR; break;
        default: errno = EIO; break;
    }
}

extern "C" {
int sh_socket(int d, int t, int p) {
    const SOCKET s = ::socket(d, t, p);
    if (s == INVALID_SOCKET) { set_errno_from_wsa(); return -1; }
    return static_cast<int>(s);
}
int sh_bind(int fd, const struct sockaddr *addr, socklen_t len) {
    sockaddr_in a = *reinterpret_cast<const sockaddr_in *>(addr);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // Never expose the lab beyond loopback.
    const int r = ::bind(fd, reinterpret_cast<sockaddr *>(&a), len);
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return 0;
}
int sh_listen(int fd, int backlog) {
    const int r = ::listen(fd, backlog);
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return 0;
}
int sh_accept(int fd, struct sockaddr *addr, socklen_t *len) {
    const SOCKET s = ::accept(fd, addr, len);
    if (s == INVALID_SOCKET) { set_errno_from_wsa(); return -1; }
    const int sndbuf = g_lab_server_sndbuf.load();
    if (sndbuf > 0) {
        ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char *>(&sndbuf), sizeof(sndbuf));
    }
    g_lab_last_accepted = static_cast<int>(s);
    lab_note_accept(static_cast<int>(s));
    return static_cast<int>(s);
}
int sh_shutdown(int fd, int how) {
    lab_check_guard_io(fd);
    const int result=::shutdown(static_cast<SOCKET>(fd),how);
    if (result==SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    if (how == SD_SEND) g_lab_half_closed_fd = fd;
    return result;
}
int sh_close(int fd) {
    lab_check_guard_io(fd);
    lab_note_close(fd);
    return ::closesocket(fd) == SOCKET_ERROR ? -1 : 0;
}
int sh_send(int fd, const void *buf, size_t n, int flags) {
    lab_check_guard_io(fd);
    const int left=g_lab_fail_after_bytes.load();
    if(left==0){g_lab_fail_after_bytes=-1;errno=EIO;return -1;}
    if(left>0 && n>static_cast<size_t>(left))n=left;
    if (g_lab_fail_next_send.exchange(0)) { errno = EIO; return -1; }
    if (g_lab_stall_fd.load() == fd) {
        // Peer gone silently: on lwIP send() returns EAGAIN after SO_SNDTIMEO (30 s).
        DWORD timeout_ms = 0;
        int optlen = sizeof(timeout_ms);
        ::getsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeout_ms), &optlen);
        const uint32_t until = millis() + lab_stall_ms(timeout_ms);
        while (g_lab_stall_fd.load() == fd && static_cast<int32_t>(millis() - until) < 0) Sleep(5);
        if (g_lab_stall_fd.load() == fd) { errno = EAGAIN; return -1; }
    }
    if (const int d = g_lab_send_delay_ms.load()) Sleep(d);
    const int cap = g_lab_partial_send_limit.load();
    if (cap > 0 && n > static_cast<size_t>(cap)) n = static_cast<size_t>(cap);
    const int r = ::send(static_cast<SOCKET>(fd), static_cast<const char *>(buf), static_cast<int>(n), flags);
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    if(left>0 && r>0)g_lab_fail_after_bytes-=r;
    return r;
}
int sh_recv(int fd, void *buf, size_t n, int flags) {
    lab_check_guard_io(fd);
    const int r = ::recv(static_cast<SOCKET>(fd), static_cast<char *>(buf), static_cast<int>(n), flags);
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return r;
}
int sh_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t tolen) {
    const int r = ::sendto(fd, static_cast<const char *>(buf), static_cast<int>(n), flags, to, tolen);
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return r;
}
int sh_setsockopt(int fd, int level, int opt, const void *val, socklen_t len) {
    int r;
    if (level == SOL_SOCKET && (opt == SO_RCVTIMEO || opt == SO_SNDTIMEO)) {
        const timeval *tv = static_cast<const timeval *>(val);
        const DWORD ms = static_cast<DWORD>(tv->tv_sec * 1000 + tv->tv_usec / 1000);
        r = ::setsockopt(static_cast<SOCKET>(fd), level, opt, reinterpret_cast<const char *>(&ms), sizeof(ms));
    } else {
        r = ::setsockopt(static_cast<SOCKET>(fd), level, opt, static_cast<const char *>(val), len);
    }
    if (r == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return 0;
}
int sh_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv) {
    const int ret = ::select(n, r, w, e, tv);
    if (ret == SOCKET_ERROR) { set_errno_from_wsa(); return -1; }
    return ret;
}

BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn, const char *, uint32_t, void *arg, UBaseType_t,
                                           TaskHandle_t *out, BaseType_t, uint32_t) {
    std::thread t(fn, arg);
    if (out) *out = reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(1));
    t.detach();
    return pdPASS;
}
void vTaskDeleteWithCaps(TaskHandle_t) {}
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                                  UBaseType_t prio, TaskHandle_t *out, BaseType_t core) {
    if (g_lab_fail_scan_task.exchange(false)) return pdFAIL;
    // Gate only scan workers; the SDK HTTP task uses WithCaps directly.
    std::thread t([fn, arg] {
        g_lab_scan_task_waiting = true;
        while (g_lab_hold_scan_task.load()) Sleep(1);
        g_lab_scan_task_waiting = false;
        fn(arg);
    });
    if (out) *out = reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(1));
    t.detach();
    return pdPASS;
}
void vTaskDelete(TaskHandle_t) {}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(1)); }
void vTaskDelay(TickType_t t) { Sleep(t); }

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t init) {
    return CreateSemaphoreA(nullptr, static_cast<LONG>(init), static_cast<LONG>(max), nullptr);
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *) {
    return CreateSemaphoreA(nullptr, 1, 1, nullptr);
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
    return WaitForSingleObject(s, t == portMAX_DELAY ? INFINITE : t) == WAIT_OBJECT_0 ? pdTRUE : pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { return ReleaseSemaphore(s, 1, nullptr) ? pdTRUE : pdFALSE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { CloseHandle(s); }
}  // extern "C"

uint32_t millis() { return static_cast<uint32_t>(GetTickCount64()); }
void delay(uint32_t ms) { Sleep(ms); }

// Numeric helpers that newlib provides on ESP32 but MinGW does not export.
extern "C" {
char *utoa(unsigned v, char *s, int r) { _ultoa_s(v, s, 34, r); return s; }
char *ultoa(unsigned long v, char *s, int r) { _ultoa_s(v, s, 34, r); return s; }
char *dtostrf(double v, signed char w, unsigned char p, char *s) { sprintf(s, "%*.*f", w, p, v); return s; }
}
