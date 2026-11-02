#pragma once
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
extern int g_lab_esp_log;
#ifdef __cplusplus
}
#endif
#define LAB_LOG_(lvl, tag, ...) do { if (g_lab_esp_log) { fprintf(stderr, "[%s %s] ", lvl, (const char *)(tag)); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)
#define ESP_LOGE(tag, ...) LAB_LOG_("E", tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) LAB_LOG_("W", tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) ((void)0)
#define ESP_LOGD(tag, ...) ((void)0)
#define ESP_LOGV(tag, ...) ((void)0)
#define ESP_LOG_BUFFER_HEX_LEVEL(...) ((void)0)
