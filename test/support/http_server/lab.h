#pragma once
#include <atomic>
#include <stdint.h>
extern std::atomic<long> g_lab_alloc_fail_min;
extern std::atomic<long> g_lab_alloc_skip_big;
extern std::atomic<long> g_lab_alloc_failed;
extern std::atomic<int> g_lab_server_sndbuf;
extern std::atomic<int> g_lab_stall_fd;
extern std::atomic<int> g_lab_send_delay_ms;
void lab_note_accept(int fd);
void lab_note_close(int fd);
uint32_t lab_stall_ms(uint32_t configured_ms);
extern std::atomic<int> g_lab_fail_next_send;
extern std::atomic<int> g_lab_fail_after_bytes;
extern std::atomic<int> g_lab_partial_send_limit;

extern std::atomic<bool> g_lab_fail_scan_task;
extern std::atomic<bool> g_lab_hold_scan_task, g_lab_scan_task_waiting;
extern std::atomic<int> g_lab_half_closed_fd;

extern std::atomic<int> g_lab_last_accepted, g_lab_guard_fd, g_lab_guard_io;
extern std::atomic<int> g_lab_guard_recv_bytes;
void lab_check_guard_io(int fd);
