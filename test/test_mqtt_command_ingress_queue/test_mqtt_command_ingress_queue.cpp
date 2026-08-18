#include <unity.h>

#include <string.h>

#include "core/MqttCommandIngressQueue.h"

void setUp() {}
void tearDown() {}

namespace {

void test_single_fragment_round_trip() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/backlight";
    const uint8_t payload[] = {'O', 'N'};

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Queued),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            payload,
                                            sizeof(payload),
                                            sizeof(payload),
                                            0)));

    MqttCommandIngressQueue::Message message{};
    TEST_ASSERT_TRUE(queue.pop(message));
    TEST_ASSERT_EQUAL_STRING(topic, message.topic);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), message.payload_length);
    TEST_ASSERT_EQUAL_MEMORY(payload, message.payload, sizeof(payload));
    TEST_ASSERT_FALSE(queue.pop(message));
}

void test_fragmented_message_is_not_visible_until_complete() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/fan_timer";
    const uint8_t first[] = {'3', '6'};
    const uint8_t second[] = {'0', '0'};
    MqttCommandIngressQueue::Message message{};

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::InProgress),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            first,
                                            sizeof(first),
                                            4,
                                            0)));
    TEST_ASSERT_FALSE(queue.pop(message));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Queued),
        static_cast<int>(queue.pushFragment(nullptr,
                                            0,
                                            second,
                                            sizeof(second),
                                            4,
                                            2)));
    TEST_ASSERT_TRUE(queue.pop(message));
    TEST_ASSERT_EQUAL_STRING("3600", reinterpret_cast<const char *>(message.payload));
}

void test_oversized_payload_is_dropped_without_truncation() {
    MqttCommandIngressQueue queue;
    uint8_t payload[MqttCommandIngressQueue::kPayloadCapacity + 1] = {};

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Dropped),
        static_cast<int>(queue.pushFragment("aura/command/backlight",
                                            strlen("aura/command/backlight"),
                                            payload,
                                            sizeof(payload),
                                            sizeof(payload),
                                            0)));
    MqttCommandIngressQueue::Message message{};
    TEST_ASSERT_FALSE(queue.pop(message));
    TEST_ASSERT_EQUAL_UINT32(1, queue.takeDroppedCount());
    TEST_ASSERT_EQUAL_UINT32(0, queue.takeDroppedCount());
}

void test_queue_full_drops_newest_and_preserves_order() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/backlight";
    for (size_t i = 0; i < MqttCommandIngressQueue::kQueueCapacity; ++i) {
        const uint8_t payload[] = {static_cast<uint8_t>('0' + i)};
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(MqttCommandIngressQueue::PushResult::Queued),
            static_cast<int>(queue.pushFragment(topic,
                                                strlen(topic),
                                                payload,
                                                sizeof(payload),
                                                sizeof(payload),
                                                0)));
    }
    const uint8_t overflow[] = {'X'};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Dropped),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            overflow,
                                            sizeof(overflow),
                                            sizeof(overflow),
                                            0)));

    MqttCommandIngressQueue::Message message{};
    for (size_t i = 0; i < MqttCommandIngressQueue::kQueueCapacity; ++i) {
        TEST_ASSERT_TRUE(queue.pop(message));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>('0' + i), message.payload[0]);
    }
    TEST_ASSERT_FALSE(queue.pop(message));
    TEST_ASSERT_EQUAL_UINT32(1, queue.takeDroppedCount());
}

void test_out_of_order_fragment_drops_whole_message() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/fan_timer";
    const uint8_t first[] = {'3', '6'};
    const uint8_t bad_second[] = {'0'};

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::InProgress),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            first,
                                            sizeof(first),
                                            4,
                                            0)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Dropped),
        static_cast<int>(queue.pushFragment(nullptr,
                                            0,
                                            bad_second,
                                            sizeof(bad_second),
                                            4,
                                            3)));
    MqttCommandIngressQueue::Message message{};
    TEST_ASSERT_FALSE(queue.pop(message));
    TEST_ASSERT_EQUAL_UINT32(1, queue.takeDroppedCount());
}

void test_slots_are_reused_after_wraparound() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/backlight";
    MqttCommandIngressQueue::Message message{};
    for (size_t round = 0; round < 3; ++round) {
        for (size_t i = 0; i < MqttCommandIngressQueue::kQueueCapacity; ++i) {
            const uint8_t payload[] = {
                static_cast<uint8_t>('A' + round *
                                           MqttCommandIngressQueue::kQueueCapacity + i)};
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(MqttCommandIngressQueue::PushResult::Queued),
                static_cast<int>(queue.pushFragment(topic,
                                                    strlen(topic),
                                                    payload,
                                                    sizeof(payload),
                                                    sizeof(payload),
                                                    0)));
        }
        for (size_t i = 0; i < MqttCommandIngressQueue::kQueueCapacity; ++i) {
            TEST_ASSERT_TRUE(queue.pop(message));
            TEST_ASSERT_EQUAL_UINT8(
                static_cast<uint8_t>('A' + round *
                                               MqttCommandIngressQueue::kQueueCapacity + i),
                message.payload[0]);
        }
        TEST_ASSERT_FALSE(queue.pop(message));
    }
}

void test_reset_discards_complete_and_partial_messages() {
    MqttCommandIngressQueue queue;
    const char *topic = "aura/command/backlight";
    const uint8_t complete[] = {'O', 'N'};
    const uint8_t partial[] = {'O'};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::Queued),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            complete,
                                            sizeof(complete),
                                            sizeof(complete),
                                            0)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MqttCommandIngressQueue::PushResult::InProgress),
        static_cast<int>(queue.pushFragment(topic,
                                            strlen(topic),
                                            partial,
                                            sizeof(partial),
                                            2,
                                            0)));
    queue.reset();
    MqttCommandIngressQueue::Message message{};
    TEST_ASSERT_FALSE(queue.pop(message));
    TEST_ASSERT_EQUAL_UINT32(0, queue.takeDroppedCount());
}

} // namespace

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_single_fragment_round_trip);
    RUN_TEST(test_fragmented_message_is_not_visible_until_complete);
    RUN_TEST(test_oversized_payload_is_dropped_without_truncation);
    RUN_TEST(test_queue_full_drops_newest_and_preserves_order);
    RUN_TEST(test_out_of_order_fragment_drops_whole_message);
    RUN_TEST(test_slots_are_reused_after_wraparound);
    RUN_TEST(test_reset_discards_complete_and_partial_messages);
    return UNITY_END();
}
