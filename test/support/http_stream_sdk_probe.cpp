// Host-only loopback probe. Builds unmodified send_all/send_chunk from the selected SDK.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "web/WebHttpSendCapture.h"
#include "sdk_shim.h"

static void require(bool value, const char *message) {
    if (!value) { fprintf(stderr, "FAIL: %s (WSA %d)\n", message, WSAGetLastError()); exit(1); }
}
static int host_errno(int wsa) {
    switch (wsa) {
        case WSAECONNRESET: return ECONNRESET;
        case WSAECONNABORTED: return ECONNABORTED;
        case WSAETIMEDOUT: return ETIMEDOUT;
        case WSAEWOULDBLOCK: return EAGAIN;
        case WSAENOTCONN: return ENOTCONN;
        default: return EIO;
    }
}
struct Probe {
    WebHttpSendCapture capture;
    SOCKET socket = INVALID_SOCKET;
    uint32_t calls = 0;
    int wsa_error = 0;
};
static int socket_send(void *handle, int, const char *data, size_t size, int flags) {
    auto &probe = *static_cast<Probe *>(handle);
    ++probe.calls;
    const int result = probe.capture.socketSend(data, size, [&]() {
        const int ret = send(probe.socket, data, static_cast<int>(size), flags);
        if (ret == SOCKET_ERROR) {
            probe.wsa_error = WSAGetLastError();
            errno = host_errno(probe.wsa_error);
        }
        return ret;
    });
    return result < 0 ? -1 : result;
}
static void sockets(SOCKET &server, SOCKET &client) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    require(listener != INVALID_SOCKET, "listener");
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Never a physical device.
    require(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "bind loopback");
    int length = sizeof(address);
    require(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0, "getsockname");
    require(listen(listener, 1) == 0, "listen");
    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int small_buffer = 4096;
    setsockopt(client, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&small_buffer), sizeof(small_buffer));
    require(connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "connect loopback");
    server = accept(listener, nullptr, nullptr);
    closesocket(listener);
    require(server != INVALID_SOCKET, "accept");
    setsockopt(server, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&small_buffer), sizeof(small_buffer));
    DWORD timeout = 250;
    setsockopt(server, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
}
static void reset_peer(SOCKET &client) {
    linger reset{1, 0};
    setsockopt(client, SOL_SOCKET, SO_LINGER, reinterpret_cast<char *>(&reset), sizeof(reset));
    closesocket(client); client = INVALID_SOCKET;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
static std::string decode(const std::string &wire) {
    auto pos = wire.find("\r\n\r\n"); require(pos != std::string::npos, "HTTP headers"); pos += 4;
    std::string body;
    while (true) {
        auto end = wire.find("\r\n", pos); require(end != std::string::npos, "chunk length");
        const size_t size = strtoul(wire.substr(pos, end - pos).c_str(), nullptr, 16);
        pos = end + 2;
        require(pos + size + 2 <= wire.size(), "whole chunk");
        body.append(wire, pos, size); pos += size;
        require(wire.compare(pos, 2, "\r\n") == 0, "chunk terminator"); pos += 2;
        if (!size) { require(pos == wire.size(), "no duplicate bytes"); return body; }
    }
}
static void run(const char *scenario) {
    SOCKET client;
    Probe probe; sockets(probe.socket, client);
    sock_db sd{}; sd.handle = &probe; sd.send_fn = socket_send;
    httpd_req_aux aux{}; aux.sd = &sd; aux.max_req_hdr_len = 1024;
    aux.status = "200 OK"; aux.content_type = "application/octet-stream";
    httpd_data hd{};
    httpd_req_t req{}; req.handle = &hd; req.aux = &aux;
    const bool full = std::string(scenario) == "complete";
    const bool slow = std::string(scenario) == "slow_reader";
    if (full || slow) {
        DWORD completion_timeout = 10000;
        setsockopt(probe.socket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<char *>(&completion_timeout), sizeof(completion_timeout));
    }
    const bool reset = std::string(scenario) == "reset";
    const bool finish_reset = std::string(scenario) == "finish_reset";
    const size_t total = slow ? 2 * 1024 * 1024 : full ? 64 * 1024 : finish_reset ? 512 : 4 * 1024 * 1024;
    std::string body(total, 'x');
    for (size_t i = 0; i < total; ++i) body[i] = static_cast<char>('a' + i % 26);
    std::string received;
    std::thread reader;
    if (full || slow) {
        reader = std::thread([&]() {
            char buffer[4096]; int count;
            while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) {
                received.append(buffer, count);
                if (slow) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    if (reset) reset_peer(client);
    size_t sent = 0;
    const auto start = std::chrono::steady_clock::now();
    bool ok = true;
    while (sent < total && ok) {
        const size_t size = std::min(size_t(4096), total - sent);
        const auto *data = reinterpret_cast<const uint8_t *>(body.data() + sent);
        ok = probe.capture.chunk(WebStreamStage::Body, data, size, [&]() {
            const int result = httpd_resp_send_chunk(&req, reinterpret_cast<const char *>(data), size);
            errno = EIO; // Exercise loss below the httpd API boundary.
            return result;
        });
        if (ok) sent += size;
    }
    if (finish_reset) reset_peer(client);
    if (ok) ok = probe.capture.chunk(WebStreamStage::Finish, nullptr, 0, [&]() {
        return httpd_resp_send_chunk(&req, nullptr, 0);
    });
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto error = probe.capture.error();
    if (full || slow) {
        require(ok, "complete and slow transfers succeed");
        if (slow) require(elapsed >= 10, "slow reader produces measured sender backpressure");
        shutdown(probe.socket, SD_SEND); reader.join();
        require(decode(received) == body, "decoded body matches exactly");
        require(!error.errno_valid && !error.terminal, "success has no stale failure");
    } else {
        require(!ok && error.errno_valid && error.sdk_error == ESP_ERR_HTTPD_RESP_SEND, "failed send evidence");
        if (!reset && !finish_reset) require(error.socket_errno == ETIMEDOUT || error.socket_errno == EAGAIN, "stalled client timeout");
        else require(error.socket_errno == ECONNRESET || error.socket_errno == ECONNABORTED, "reset errno");
        require(error.stage == (finish_reset ? WebStreamStage::Finish : WebStreamStage::Body), "failure stage");
        const auto calls = probe.calls;
        require(!probe.capture.chunk(WebStreamStage::Finish, nullptr, 0, [&]() {
            return httpd_resp_send_chunk(&req, nullptr, 0);
        }), "failed chunk cannot be finished/replayed");
        require(probe.calls == calls, "no additional send after failure");
    }
    printf("{\"case\":\"%s\",\"ok\":%s,\"sent\":%zu,\"total\":%zu,\"wire_bytes\":%u,\"partial_body_bytes\":%u,\"sdk_error\":%ld,\"errno\":%d,\"wsa_error\":%d,\"duration_ms\":%lld,\"stage\":\"%s\"}\n",
        scenario, ok ? "true" : "false", sent, total, error.wire_bytes, error.partial_body_bytes,
        static_cast<long>(error.sdk_error), error.socket_errno, probe.wsa_error, elapsed,
        web_stream_stage_text(error.stage));
    closesocket(probe.socket); if (client != INVALID_SOCKET) closesocket(client);
}
int main() {
    WSADATA data{}; require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup");
    for (const char *name : {"complete", "slow_reader", "reset", "stalled_reader", "finish_reset"}) run(name);
    WSACleanup();
}
