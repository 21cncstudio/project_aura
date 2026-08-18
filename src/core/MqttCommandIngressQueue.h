// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <stddef.h>
#include <stdint.h>

// Single-producer/single-consumer queue between the ESP MQTT event task and
// the guarded application network plane. The event task only performs bounded
// copies; String construction and command parsing happen later under a
// WakePowerGuard activity lease.
class MqttCommandIngressQueue {
public:
    static constexpr size_t kQueueCapacity = 8;
    static constexpr size_t kTopicCapacity = 255;
    static constexpr size_t kPayloadCapacity = 63;

    struct Message {
        char topic[kTopicCapacity + 1] = {};
        uint8_t payload[kPayloadCapacity + 1] = {};
        uint16_t topic_length = 0;
        uint16_t payload_length = 0;
    };

    enum class PushResult : uint8_t {
        InProgress = 0,
        Queued,
        Dropped,
    };

    PushResult pushFragment(const char *topic,
                            size_t topic_length,
                            const uint8_t *payload,
                            size_t payload_length,
                            size_t total_payload_length,
                            size_t payload_offset);
    bool pop(Message &out);
    uint32_t takeDroppedCount();
    void reset();

private:
    static constexpr uint8_t kSlotCount =
        static_cast<uint8_t>(kQueueCapacity + 1);

    static uint8_t advance(uint8_t index) {
        return static_cast<uint8_t>((index + 1U) % kSlotCount);
    }
    PushResult dropCurrent(bool more_fragments_expected);

    Message slots_[kSlotCount]{};
    std::atomic<uint8_t> head_{0};
    std::atomic<uint8_t> tail_{0};
    std::atomic<uint32_t> dropped_count_{0};

    // Producer-owned assembly state. The slot is not published through head_
    // until the final fragment has been copied.
    bool assembling_ = false;
    bool dropping_current_ = false;
    uint8_t assembling_slot_ = 0;
    uint8_t assembling_next_head_ = 0;
    size_t assembling_total_payload_length_ = 0;
};
