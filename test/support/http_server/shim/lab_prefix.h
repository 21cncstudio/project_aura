// Host lab prefix: maps the POSIX/lwIP socket surface used by the real
// esp_http_server sources and production transport onto Winsock loopback.
// This is NOT lwIP: timing, buffering and reset behaviour are host-specific.
#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#undef interface
#undef IN
#undef OUT
#undef ERROR

#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif

#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 0x7777
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifdef __cplusplus
extern "C" {
#endif
int sh_socket(int domain, int type, int protocol);
int sh_bind(int fd, const struct sockaddr *addr, socklen_t len);
int sh_listen(int fd, int backlog);
int sh_accept(int fd, struct sockaddr *addr, socklen_t *len);
int sh_close(int fd);
int sh_shutdown(int fd, int how);
int sh_send(int fd, const void *buf, size_t n, int flags);
int sh_recv(int fd, void *buf, size_t n, int flags);
int sh_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t tolen);
int sh_setsockopt(int fd, int level, int opt, const void *val, socklen_t len);
int sh_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv);
void *lab_realloc(void *ptr, size_t size);
#ifdef __cplusplus
}
#endif

#ifndef __cplusplus
#define socket(a, b, c) sh_socket(a, b, c)
#define bind(a, b, c) sh_bind(a, b, c)
#define listen(a, b) sh_listen(a, b)
#define accept(a, b, c) sh_accept(a, b, c)
#define close(a) sh_close(a)
#define send(a, b, c, d) sh_send(a, b, c, d)
#define recv(a, b, c, d) sh_recv(a, b, c, d)
#define sendto(a, b, c, d, e, f) sh_sendto(a, b, c, d, e, f)
#define setsockopt(a, b, c, d, e) sh_setsockopt(a, b, c, d, e)
#define select(a, b, c, d, e) sh_select(a, b, c, d, e)
#else
inline int shutdown(int fd, int how) { return sh_shutdown(fd, how); }
inline int send(int fd, const char *b, size_t n, int f) { return sh_send(fd, b, n, f); }
inline int recv(int fd, char *b, size_t n, int f) { return sh_recv(fd, b, n, f); }
inline int recv(int fd, char *b, int n, int f) { return sh_recv(fd, b, (size_t)n, f); }
inline int setsockopt(int fd, int l, int o, const void *v, socklen_t n) { return sh_setsockopt(fd, l, o, v, n); }
#endif

#include <string.h>
#ifndef LAB_HAVE_STRLCPY
#define LAB_HAVE_STRLCPY
static inline size_t lab_strlcpy(char *d, const char *s, size_t n) {
    size_t l = strlen(s);
    if (n) { size_t c = l >= n ? n - 1 : l; memcpy(d, s, c); d[c] = 0; }
    return l;
}
#define strlcpy lab_strlcpy
#endif
#ifndef __cplusplus
#define F_GETFD 1
#define fcntl(a, b) (0)
#endif
#define ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE(x)
#define ESP_COMPILER_DIAGNOSTIC_POP(x)
#ifndef __cplusplus
#define inet_aton(s, a) inet_pton(AF_INET, (s), (a))
#endif
