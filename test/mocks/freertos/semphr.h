#pragma once

#include "freertos/FreeRTOS.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

struct StaticSemaphore_t {
    std::mutex state_mutex;
    std::condition_variable changed;
    bool available = false;
    bool dynamically_allocated = false;
};

using SemaphoreHandle_t = StaticSemaphore_t *;

namespace FreeRtosSemaphoreMock {

inline std::atomic<uint32_t> &tickDurationMicroseconds() {
    static std::atomic<uint32_t> duration{1000};
    return duration;
}

inline void setTickDurationMicroseconds(uint32_t duration) {
    tickDurationMicroseconds().store(duration == 0 ? 1 : duration);
}

inline void resetTickDuration() {
    tickDurationMicroseconds().store(1000);
}

} // namespace FreeRtosSemaphoreMock

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto *semaphore = new StaticSemaphore_t();
    semaphore->available = true;
    semaphore->dynamically_allocated = true;
    return semaphore;
}

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *buffer) {
    if (!buffer) {
        return nullptr;
    }
    std::lock_guard<std::mutex> guard(buffer->state_mutex);
    buffer->available = true;
    buffer->dynamically_allocated = false;
    return buffer;
}

inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *buffer) {
    if (!buffer) {
        return nullptr;
    }
    std::lock_guard<std::mutex> guard(buffer->state_mutex);
    buffer->available = false;
    buffer->dynamically_allocated = false;
    return buffer;
}

inline int xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait_ticks) {
    if (!semaphore) {
        return 0;
    }

    std::unique_lock<std::mutex> lock(semaphore->state_mutex);
    const auto available = [semaphore]() { return semaphore->available; };
    bool acquired = available();
    if (!acquired && wait_ticks == portMAX_DELAY) {
        semaphore->changed.wait(lock, available);
        acquired = true;
    } else if (!acquired && wait_ticks != 0) {
        const uint64_t duration_us =
            static_cast<uint64_t>(wait_ticks) *
            FreeRtosSemaphoreMock::tickDurationMicroseconds().load();
        acquired = semaphore->changed.wait_for(
            lock,
            std::chrono::microseconds(duration_us),
            available);
    }

    if (!acquired) {
        return 0;
    }
    semaphore->available = false;
    return pdTRUE;
}

inline int xSemaphoreGive(SemaphoreHandle_t semaphore) {
    if (!semaphore) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> guard(semaphore->state_mutex);
        semaphore->available = true;
    }
    semaphore->changed.notify_one();
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) {
    if (semaphore && semaphore->dynamically_allocated) {
        delete semaphore;
    }
}
