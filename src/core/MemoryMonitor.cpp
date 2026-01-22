#include "core/MemoryMonitor.h"

#include <string.h>
#include "core/Logger.h"

void MemoryMonitor::begin(uint32_t interval_ms) {
    interval_ms_ = interval_ms;
    last_log_ms_ = millis();
}

void MemoryMonitor::poll(uint32_t now_ms) {
    if (!enabled_ || interval_ms_ == 0) {
        return;
    }
    if (now_ms - last_log_ms_ >= interval_ms_) {
        last_log_ms_ = now_ms;
        logNow("periodic");
    }
}

void MemoryMonitor::logNow(const char *reason) {
    if (!enabled_) {
        return;
    }
    uint32_t heap_free = ESP.getFreeHeap();
    uint32_t heap_min = ESP.getMinFreeHeap();
    uint32_t heap_max = ESP.getMaxAllocHeap();
    uint32_t psram_free = ESP.getFreePsram();
    uint32_t psram_min = ESP.getMinFreePsram();
    uint32_t psram_max = ESP.getMaxAllocPsram();
    const char *tag = "Mem";
    const char *reason_text = (reason && reason[0] != '\0') ? reason : "manual";

    Logger::Level level = Logger::Info;
    if (reason && strcmp(reason, "periodic") == 0) {
        level = Logger::Debug;
    }

    if (psram_free == 0 && psram_min == 0 && psram_max == 0) {
        Logger::log(level, tag,
                    "%s heap free=%u min=%u max=%u",
                    reason_text, heap_free, heap_min, heap_max);
    } else {
        Logger::log(level, tag,
                    "%s heap free=%u min=%u max=%u psram free=%u min=%u max=%u",
                    reason_text,
                    heap_free, heap_min, heap_max,
                    psram_free, psram_min, psram_max);
    }
}
