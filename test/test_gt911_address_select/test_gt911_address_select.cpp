#include <unity.h>

#include "core/Gt911AddressSelect.h"

namespace {

enum class EventType : uint8_t {
    IntOutput,
    IntLevel,
    ResetLevel,
    ReleaseInt,
    Delay,
};

struct Event {
    EventType type;
    uint32_t value;
};

struct FakeContext {
    Event events[16] = {};
    size_t event_count = 0;
    EventType fail_event = EventType::Delay;
    size_t fail_occurrence = 0;
    size_t int_level_calls = 0;

    void record(EventType type, uint32_t value) {
        events[event_count++] = {type, value};
    }
};

bool setIntOutput(void *opaque) {
    auto *ctx = static_cast<FakeContext *>(opaque);
    ctx->record(EventType::IntOutput, 0U);
    return !(ctx->fail_event == EventType::IntOutput &&
             ctx->fail_occurrence == 1U);
}

bool setIntLevel(void *opaque, bool high) {
    auto *ctx = static_cast<FakeContext *>(opaque);
    ++ctx->int_level_calls;
    ctx->record(EventType::IntLevel, high ? 1U : 0U);
    return !(ctx->fail_event == EventType::IntLevel &&
             ctx->fail_occurrence == ctx->int_level_calls);
}

bool setResetLevel(void *opaque, bool high) {
    auto *ctx = static_cast<FakeContext *>(opaque);
    ctx->record(EventType::ResetLevel, high ? 1U : 0U);
    return true;
}

bool releaseInt(void *opaque) {
    auto *ctx = static_cast<FakeContext *>(opaque);
    ctx->record(EventType::ReleaseInt, 0U);
    return true;
}

void delayMs(void *opaque, uint32_t delay_ms) {
    auto *ctx = static_cast<FakeContext *>(opaque);
    ctx->record(EventType::Delay, delay_ms);
}

Gt911AddressSelect::Ops makeOps(FakeContext &ctx) {
    return {
        &ctx,
        setIntOutput,
        setIntLevel,
        setResetLevel,
        releaseInt,
        delayMs,
    };
}

void assertEvent(const FakeContext &ctx,
                 size_t index,
                 EventType type,
                 uint32_t value) {
    TEST_ASSERT_LESS_THAN(ctx.event_count, index);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type),
                            static_cast<uint8_t>(ctx.events[index].type));
    TEST_ASSERT_EQUAL_UINT32(value, ctx.events[index].value);
}

} // namespace

void setUp() {}
void tearDown() {}

void test_backup_address_sequence_matches_gt911_timing() {
    FakeContext ctx;
    const auto result = Gt911AddressSelect::selectAddress(makeOps(ctx), 0x14);

    TEST_ASSERT_TRUE(result.ok());
    TEST_ASSERT_EQUAL_UINT32(11U, ctx.event_count);
    assertEvent(ctx, 0, EventType::IntOutput, 0U);
    assertEvent(ctx, 1, EventType::IntLevel, 0U);
    assertEvent(ctx, 2, EventType::Delay, 50U);
    assertEvent(ctx, 3, EventType::ResetLevel, 0U);
    assertEvent(ctx, 4, EventType::Delay, 50U);
    assertEvent(ctx, 5, EventType::IntLevel, 1U);
    assertEvent(ctx, 6, EventType::Delay, 5U);
    assertEvent(ctx, 7, EventType::ResetLevel, 1U);
    assertEvent(ctx, 8, EventType::Delay, 350U);
    assertEvent(ctx, 9, EventType::Delay, 150U);
    assertEvent(ctx, 10, EventType::ReleaseInt, 0U);
}

void test_failure_after_reset_assertion_releases_reset_and_int() {
    FakeContext ctx;
    ctx.fail_event = EventType::IntLevel;
    ctx.fail_occurrence = 2U;

    const auto result = Gt911AddressSelect::selectAddress(makeOps(ctx), 0x14);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gt911AddressSelect::Failure::IntSelect),
        static_cast<uint8_t>(result.failure));
    TEST_ASSERT_EQUAL_UINT32(8U, ctx.event_count);
    assertEvent(ctx, 5, EventType::IntLevel, 1U);
    assertEvent(ctx, 6, EventType::ResetLevel, 1U);
    assertEvent(ctx, 7, EventType::ReleaseInt, 0U);
}

void test_missing_operation_is_rejected_without_gpio_activity() {
    FakeContext ctx;
    auto ops = makeOps(ctx);
    ops.release_int = nullptr;

    const auto result = Gt911AddressSelect::selectAddress(ops, 0x14);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gt911AddressSelect::Failure::InvalidOps),
        static_cast<uint8_t>(result.failure));
    TEST_ASSERT_EQUAL_UINT32(0U, ctx.event_count);
}

void test_primary_address_changes_only_the_strap_level() {
    FakeContext alternate;
    FakeContext primary;
    TEST_ASSERT_TRUE(Gt911AddressSelect::selectAddress(makeOps(alternate), 0x14).ok());
    TEST_ASSERT_TRUE(Gt911AddressSelect::selectAddress(makeOps(primary), 0x5D).ok());

    TEST_ASSERT_EQUAL_UINT32(alternate.event_count, primary.event_count);
    for (size_t i = 0; i < primary.event_count; ++i) {
        assertEvent(primary, i, alternate.events[i].type,
                    i == 5 ? 0U : alternate.events[i].value);
    }
    assertEvent(primary, 5, EventType::IntLevel, 0U);
    assertEvent(primary, 7, EventType::ResetLevel, 1U);
}

void test_primary_strap_failure_also_releases_reset_and_int() {
    FakeContext ctx;
    ctx.fail_event = EventType::IntLevel;
    ctx.fail_occurrence = 2U;
    const auto result = Gt911AddressSelect::selectAddress(makeOps(ctx), 0x5D);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gt911AddressSelect::Failure::IntSelect),
                            static_cast<uint8_t>(result.failure));
    assertEvent(ctx, 5, EventType::IntLevel, 0U);
    assertEvent(ctx, 6, EventType::ResetLevel, 1U);
    assertEvent(ctx, 7, EventType::ReleaseInt, 0U);
}

void test_invalid_address_is_rejected_without_gpio_activity() {
    FakeContext ctx;
    // 0xBA is the 8-bit write address, not the 7-bit address required here.
    const auto result = Gt911AddressSelect::selectAddress(makeOps(ctx), 0xBA);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gt911AddressSelect::Failure::InvalidAddress),
        static_cast<uint8_t>(result.failure));
    TEST_ASSERT_EQUAL_UINT32(0U, ctx.event_count);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_backup_address_sequence_matches_gt911_timing);
    RUN_TEST(test_failure_after_reset_assertion_releases_reset_and_int);
    RUN_TEST(test_missing_operation_is_rejected_without_gpio_activity);
    RUN_TEST(test_primary_address_changes_only_the_strap_level);
    RUN_TEST(test_primary_strap_failure_also_releases_reset_and_int);
    RUN_TEST(test_invalid_address_is_rejected_without_gpio_activity);
    return UNITY_END();
}
