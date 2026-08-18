// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/MqttCommandIngressQueue.h"

#include <string.h>

MqttCommandIngressQueue::PushResult
MqttCommandIngressQueue::dropCurrent(bool more_fragments_expected) {
    assembling_ = false;
    dropping_current_ = more_fragments_expected;
    dropped_count_.fetch_add(1, std::memory_order_relaxed);
    return PushResult::Dropped;
}

MqttCommandIngressQueue::PushResult
MqttCommandIngressQueue::pushFragment(const char *topic,
                                      size_t topic_length,
                                      const uint8_t *payload,
                                      size_t payload_length,
                                      size_t total_payload_length,
                                      size_t payload_offset) {
    const bool fragment_bounds_valid =
        payload_offset <= total_payload_length &&
        payload_length <= total_payload_length - payload_offset;
    const bool more_fragments_expected =
        fragment_bounds_valid &&
        payload_offset + payload_length < total_payload_length;

    if (payload_offset == 0) {
        if (assembling_ || dropping_current_) {
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
        }
        assembling_ = false;
        dropping_current_ = false;

        if (!fragment_bounds_valid || !topic || topic_length == 0 ||
            topic_length > kTopicCapacity ||
            total_payload_length > kPayloadCapacity ||
            (payload_length > 0 && !payload)) {
            return dropCurrent(more_fragments_expected);
        }

        const uint8_t head = head_.load(std::memory_order_relaxed);
        const uint8_t next_head = advance(head);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return dropCurrent(more_fragments_expected);
        }

        Message &message = slots_[head];
        memcpy(message.topic, topic, topic_length);
        message.topic[topic_length] = '\0';
        message.topic_length = static_cast<uint16_t>(topic_length);
        message.payload_length = 0;
        message.payload[0] = '\0';
        assembling_ = true;
        assembling_slot_ = head;
        assembling_next_head_ = next_head;
        assembling_total_payload_length_ = total_payload_length;
    } else if (dropping_current_) {
        if (!more_fragments_expected) {
            dropping_current_ = false;
        }
        return PushResult::Dropped;
    }

    if (!assembling_ || !fragment_bounds_valid ||
        total_payload_length != assembling_total_payload_length_ ||
        (payload_length > 0 && !payload)) {
        return dropCurrent(more_fragments_expected);
    }

    Message &message = slots_[assembling_slot_];
    if (payload_offset != message.payload_length ||
        payload_length > kPayloadCapacity - message.payload_length) {
        return dropCurrent(more_fragments_expected);
    }
    if (payload_length > 0) {
        memcpy(message.payload + message.payload_length,
               payload,
               payload_length);
    }
    message.payload_length = static_cast<uint16_t>(
        message.payload_length + payload_length);
    message.payload[message.payload_length] = '\0';

    if (message.payload_length < assembling_total_payload_length_) {
        return PushResult::InProgress;
    }

    assembling_ = false;
    head_.store(assembling_next_head_, std::memory_order_release);
    return PushResult::Queued;
}

bool MqttCommandIngressQueue::pop(Message &out) {
    const uint8_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
        return false;
    }
    out = slots_[tail];
    tail_.store(advance(tail), std::memory_order_release);
    return true;
}

uint32_t MqttCommandIngressQueue::takeDroppedCount() {
    return dropped_count_.exchange(0, std::memory_order_acq_rel);
}

void MqttCommandIngressQueue::reset() {
    // Caller must stop/destroy the MQTT client first. ESP-IDF joins the MQTT
    // task before destroy returns, so the single producer is quiescent here.
    assembling_ = false;
    dropping_current_ = false;
    assembling_slot_ = 0;
    assembling_next_head_ = 0;
    assembling_total_payload_length_ = 0;
    tail_.store(0, std::memory_order_release);
    head_.store(0, std::memory_order_release);
    dropped_count_.store(0, std::memory_order_release);
}
