// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/CaptiveDnsServer.h"
#include "core/CaptiveDnsReply.h"
#include "core/Logger.h"
#include <lwip/sockets.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

bool CaptiveDnsServer::start(uint32_t address_network_order) {
    if (running() && address_ == address_network_order) return true;
    stop();
    if (!address_network_order) return false;
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) { LOGW("WiFi", "captive DNS socket failed: %d", errno); return false; }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(53);
    local.sin_addr.s_addr = address_network_order; // Never listen on the STA IP.
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        bind(fd, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) < 0) {
        const int error = errno;
        close(fd);
        LOGW("WiFi", "captive DNS setup failed: %d", error);
        return false;
    }
    socket_ = fd;
    address_ = address_network_order;
    return true;
}

void CaptiveDnsServer::stop() {
    if (socket_ >= 0) close(socket_);
    socket_ = -1;
    address_ = 0;
}

void CaptiveDnsServer::poll() {
    // A DNS flood must not monopolize the network task or delay display wake.
    for (unsigned i = 0; socket_ >= 0 && i < 2; ++i) {
        sockaddr_in peer{};
        socklen_t peer_size = sizeof(peer);
        const int received = recvfrom(socket_, packet_, sizeof(packet_), 0,
                                      reinterpret_cast<sockaddr *>(&peer), &peer_size);
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                const int error = errno;
                stop();
                LOGW("WiFi", "captive DNS receive failed: %d", error);
            }
            break;
        }
        if (peer.sin_family != AF_INET || peer_size < sizeof(peer)) continue;
        const size_t size = CaptiveDnsReply::build(packet_, received, packet_, 512,
                                                 reinterpret_cast<const uint8_t *>(&address_));
        if (size) {
            // Nonblocking UDP: a full TX queue drops this answer. Client retry
            // is preferable to retaining a lease while waiting for socket space.
            (void)sendto(socket_, packet_, size, 0,
                         reinterpret_cast<const sockaddr *>(&peer), peer_size);
        }
    }
}
