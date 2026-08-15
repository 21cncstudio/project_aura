// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BacklightWakeBreadcrumbs.h"

#include <stddef.h>
#include <string.h>

#if !defined(UNIT_TEST)
#include <esp_attr.h>
#endif

namespace BacklightWakeBreadcrumbs {
namespace {

constexpr uint32_t kMagic = 0x424C5754u; // "BLWT"
constexpr uint16_t kVersion = 2;
constexpr uint8_t kFlagTargetOn = 1u << 0;
constexpr uint8_t kFlagPreviousOn = 1u << 1;
constexpr uint8_t kFlagBeforeValid = 1u << 2;
constexpr uint8_t kFlagBeforeSdaHigh = 1u << 3;
constexpr uint8_t kFlagBeforeSclHigh = 1u << 4;
constexpr uint8_t kFlagAfterDriverValid = 1u << 5;
constexpr uint8_t kFlagAfterDriverSdaHigh = 1u << 6;
constexpr uint8_t kFlagAfterDriverSclHigh = 1u << 7;

constexpr uint8_t kProbeFlagValid = 1u << 0;
constexpr uint8_t kProbeFlagSdaHigh = 1u << 1;
constexpr uint8_t kProbeFlagSclHigh = 1u << 2;

struct RetainedRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t revision;
    uint32_t sequence;
    uint32_t uptime_ms;
    uint32_t epoch_s;
    uint32_t driver_duration_us;
    uint32_t expected_network_manager_addr;
    uint32_t post_backlight_network_manager_addr;
    uint32_t pre_render_network_manager_addr;
    uint32_t post_backlight_task_handle;
    uint32_t pre_render_task_handle;
    uint8_t event;
    uint8_t stage;
    uint8_t driver_result;
    uint8_t flags;
    uint8_t probe_flags;
    uint8_t reserved[3];
    uint32_t crc32;
};

struct RetainedStorage {
    RetainedRecord slots[2];
};

static_assert(sizeof(RetainedRecord) == 60, "retained record layout changed");
static_assert(sizeof(RetainedStorage) == 120, "retained storage layout changed");

#if defined(UNIT_TEST)
RetainedStorage g_retained{};
#else
RTC_NOINIT_ATTR RetainedStorage g_retained;
#endif

BootSnapshot g_boot_snapshot{};
uint32_t g_boot_revision = 0;

uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

uint32_t recordCrc(const RetainedRecord &record) {
    return crc32(reinterpret_cast<const uint8_t *>(&record),
                 offsetof(RetainedRecord, crc32));
}

bool recordValid(const RetainedRecord &record) {
    return record.magic == kMagic &&
           record.version == kVersion &&
           record.size == sizeof(RetainedRecord) &&
           record.event <= static_cast<uint8_t>(Event::AlarmWake) &&
           record.stage <= static_cast<uint8_t>(Stage::PowerSettleReturned) &&
           record.driver_result <= static_cast<uint8_t>(DriverResult::Failed) &&
           record.crc32 == recordCrc(record);
}

bool storageAllZero() {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&g_retained);
    for (size_t i = 0; i < sizeof(g_retained); ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

bool revisionNewer(uint32_t candidate, uint32_t current) {
    return static_cast<int32_t>(candidate - current) > 0;
}

int latestValidSlot() {
    const bool slot0_valid = recordValid(g_retained.slots[0]);
    const bool slot1_valid = recordValid(g_retained.slots[1]);
    if (!slot0_valid) {
        return slot1_valid ? 1 : -1;
    }
    if (!slot1_valid) {
        return 0;
    }
    return revisionNewer(g_retained.slots[1].revision,
                         g_retained.slots[0].revision) ? 1 : 0;
}

void commitRecord(RetainedRecord record) {
    const int current_slot = latestValidSlot();
    const uint32_t current_revision =
        current_slot >= 0 ? g_retained.slots[current_slot].revision : 0;
    record.magic = kMagic;
    record.version = kVersion;
    record.size = sizeof(RetainedRecord);
    record.revision = current_revision + 1u;
    if (record.revision == 0) {
        record.revision = 1;
    }
    record.crc32 = recordCrc(record);
    const int next_slot = current_slot == 0 ? 1 : 0;
    memcpy(&g_retained.slots[next_slot], &record, sizeof(record));
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

RetainedRecord currentRecord() {
    const int slot = latestValidSlot();
    return slot >= 0 ? g_retained.slots[slot] : RetainedRecord{};
}

void markStage(Stage stage) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None)) {
        return;
    }
    record.stage = static_cast<uint8_t>(stage);
    commitRecord(record);
}

uint8_t encodePrimaryFlags(bool target_on,
                           bool previous_on,
                           LineState before,
                           LineState after_driver) {
    uint8_t flags = 0;
    if (target_on) flags |= kFlagTargetOn;
    if (previous_on) flags |= kFlagPreviousOn;
    if (before.valid) flags |= kFlagBeforeValid;
    if (before.sda_high) flags |= kFlagBeforeSdaHigh;
    if (before.scl_high) flags |= kFlagBeforeSclHigh;
    if (after_driver.valid) flags |= kFlagAfterDriverValid;
    if (after_driver.sda_high) flags |= kFlagAfterDriverSdaHigh;
    if (after_driver.scl_high) flags |= kFlagAfterDriverSclHigh;
    return flags;
}

uint8_t encodeProbeFlags(LineState state) {
    uint8_t flags = 0;
    if (state.valid) flags |= kProbeFlagValid;
    if (state.sda_high) flags |= kProbeFlagSdaHigh;
    if (state.scl_high) flags |= kProbeFlagSclHigh;
    return flags;
}

LineState decodeLine(uint8_t flags,
                     uint8_t valid_mask,
                     uint8_t sda_mask,
                     uint8_t scl_mask) {
    return {
        (flags & valid_mask) != 0,
        (flags & sda_mask) != 0,
        (flags & scl_mask) != 0,
    };
}

Trace decodeTrace(const RetainedRecord &record) {
    Trace trace{};
    trace.sequence = record.sequence;
    trace.uptime_ms = record.uptime_ms;
    trace.epoch_s = record.epoch_s;
    trace.driver_duration_us = record.driver_duration_us;
    trace.expected_network_manager_addr = record.expected_network_manager_addr;
    trace.post_backlight_network_manager_addr =
        record.post_backlight_network_manager_addr;
    trace.pre_render_network_manager_addr = record.pre_render_network_manager_addr;
    trace.post_backlight_task_handle = record.post_backlight_task_handle;
    trace.pre_render_task_handle = record.pre_render_task_handle;
    trace.event = static_cast<Event>(record.event);
    trace.stage = static_cast<Stage>(record.stage);
    trace.driver_result = static_cast<DriverResult>(record.driver_result);
    trace.target_on = (record.flags & kFlagTargetOn) != 0;
    trace.previous_on = (record.flags & kFlagPreviousOn) != 0;
    trace.before = decodeLine(record.flags,
                              kFlagBeforeValid,
                              kFlagBeforeSdaHigh,
                              kFlagBeforeSclHigh);
    trace.after_driver = decodeLine(record.flags,
                                    kFlagAfterDriverValid,
                                    kFlagAfterDriverSdaHigh,
                                    kFlagAfterDriverSclHigh);
    trace.after_wake_probe = decodeLine(record.probe_flags,
                                        kProbeFlagValid,
                                        kProbeFlagSdaHigh,
                                        kProbeFlagSclHigh);
    return trace;
}

} // namespace

void initializeAtBoot(bool cold_power_start) {
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
    if (cold_power_start) {
        memset(&g_retained, 0, sizeof(g_retained));
        g_boot_snapshot.status = CaptureStatus::PowerLost;
        return;
    }

    const int slot = latestValidSlot();
    if (slot < 0) {
        g_boot_snapshot.status = storageAllZero()
            ? CaptureStatus::Empty
            : CaptureStatus::Corrupt;
        return;
    }

    const RetainedRecord &record = g_retained.slots[slot];
    g_boot_revision = record.revision;
    if (record.event == static_cast<uint8_t>(Event::None) ||
        record.stage == static_cast<uint8_t>(Stage::None)) {
        g_boot_snapshot.status = CaptureStatus::Empty;
        return;
    }

    g_boot_snapshot.has_trace = true;
    g_boot_snapshot.trace = decodeTrace(record);
    g_boot_snapshot.status =
        g_boot_snapshot.trace.stage == Stage::Completed
            ? CaptureStatus::Completed
            : CaptureStatus::Active;
}

const BootSnapshot &bootSnapshot() {
    return g_boot_snapshot;
}

void acknowledgeBootSnapshot() {
    const RetainedRecord latest_before_ack = currentRecord();
    const uint32_t latest_revision =
        recordValid(latest_before_ack) ? latest_before_ack.revision : 0;
    if (latest_revision != g_boot_revision) {
        return;
    }
    RetainedRecord empty{};
    if (recordValid(latest_before_ack)) {
        empty.sequence = latest_before_ack.sequence;
    }
    commitRecord(empty);
}

void beginWake(Event event,
               uint32_t uptime_ms,
               uint32_t epoch_s,
               bool target_on,
               bool previous_on,
               LineState before) {
    if (event == Event::None || event > Event::AlarmWake) {
        return;
    }
    const RetainedRecord latest = currentRecord();
    uint32_t sequence = recordValid(latest) ? latest.sequence + 1u : 1u;
    if (sequence == 0) {
        sequence = 1;
    }

    RetainedRecord record{};
    record.sequence = sequence;
    record.uptime_ms = uptime_ms;
    record.epoch_s = epoch_s;
    record.event = static_cast<uint8_t>(event);
    record.stage = static_cast<uint8_t>(Stage::Request);
    record.driver_result = static_cast<uint8_t>(DriverResult::Unknown);
    record.flags = encodePrimaryFlags(target_on, previous_on, before, {});
    commitRecord(record);
}

void markDriverCallBegin() {
    markStage(Stage::DriverCallBegin);
}

void markTouchIrqMaskBegin() {
    markStage(Stage::TouchIrqMaskBegin);
}

void markTouchIrqMaskReturned() {
    markStage(Stage::TouchIrqMaskReturned);
}

void markDriverCallReturned(bool succeeded,
                            bool skipped,
                            uint32_t duration_us,
                            LineState after) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None)) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::DriverCallReturned);
    record.driver_result = static_cast<uint8_t>(
        skipped ? DriverResult::Skipped
                : (succeeded ? DriverResult::Succeeded : DriverResult::Failed));
    record.driver_duration_us = duration_us;
    const bool target_on = (record.flags & kFlagTargetOn) != 0;
    const bool previous_on = (record.flags & kFlagPreviousOn) != 0;
    const LineState before = decodeLine(record.flags,
                                        kFlagBeforeValid,
                                        kFlagBeforeSdaHigh,
                                        kFlagBeforeSclHigh);
    record.flags = encodePrimaryFlags(target_on, previous_on, before, after);
    commitRecord(record);
}

void markPowerSettleBegin() {
    markStage(Stage::PowerSettleBegin);
}

void markPowerSettleReturned() {
    markStage(Stage::PowerSettleReturned);
}

void markWakeProbeUpdateBegin() {
    markStage(Stage::WakeProbeUpdateBegin);
}

void markWakeProbeUpdateReturned(LineState after) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None)) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::WakeProbeUpdateReturned);
    record.probe_flags = encodeProbeFlags(after);
    commitRecord(record);
}

void markLvglActivityBegin() {
    markStage(Stage::LvglActivityBegin);
}

void markLvglActivityReturned() {
    markStage(Stage::LvglActivityReturned);
}

void markCommandReturned() {
    markStage(Stage::CommandReturned);
}

void markCompleted() {
    markStage(Stage::Completed);
}

void markUiPostBacklightContext(uint32_t expected_network_manager_addr,
                                uint32_t actual_network_manager_addr,
                                uint32_t task_handle) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        record.post_backlight_task_handle != 0) {
        return;
    }
    record.expected_network_manager_addr = expected_network_manager_addr;
    record.post_backlight_network_manager_addr = actual_network_manager_addr;
    record.post_backlight_task_handle = task_handle;
    commitRecord(record);
}

void markUiPreRenderContext(uint32_t expected_network_manager_addr,
                            uint32_t actual_network_manager_addr,
                            uint32_t task_handle) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        record.pre_render_task_handle != 0) {
        return;
    }
    record.expected_network_manager_addr = expected_network_manager_addr;
    record.pre_render_network_manager_addr = actual_network_manager_addr;
    record.pre_render_task_handle = task_handle;
    commitRecord(record);
}

const char *statusText(CaptureStatus status) {
    switch (status) {
        case CaptureStatus::PowerLost: return "power_lost";
        case CaptureStatus::Empty: return "empty";
        case CaptureStatus::Active: return "active";
        case CaptureStatus::Completed: return "completed";
        case CaptureStatus::Corrupt: return "corrupt";
        default: return "unknown";
    }
}

const char *eventText(Event event) {
    switch (event) {
        case Event::None: return "none";
        case Event::ScheduleWake: return "schedule_wake";
        case Event::TouchWake: return "touch_wake";
        case Event::AlarmWake: return "alarm_wake";
        default: return "unknown";
    }
}

const char *stageText(Stage stage) {
    switch (stage) {
        case Stage::None: return "none";
        case Stage::Request: return "request";
        case Stage::DriverCallBegin: return "driver_call_begin";
        case Stage::DriverCallReturned: return "driver_call_returned";
        case Stage::WakeProbeUpdateBegin: return "wake_probe_update_begin";
        case Stage::WakeProbeUpdateReturned: return "wake_probe_update_returned";
        case Stage::LvglActivityBegin: return "lvgl_activity_begin";
        case Stage::LvglActivityReturned: return "lvgl_activity_returned";
        case Stage::CommandReturned: return "command_returned";
        case Stage::Completed: return "completed";
        case Stage::TouchIrqMaskBegin: return "touch_irq_mask_begin";
        case Stage::TouchIrqMaskReturned: return "touch_irq_mask_returned";
        case Stage::PowerSettleBegin: return "power_settle_begin";
        case Stage::PowerSettleReturned: return "power_settle_returned";
        default: return "unknown";
    }
}

const char *driverResultText(DriverResult result) {
    switch (result) {
        case DriverResult::Unknown: return "unknown";
        case DriverResult::Skipped: return "skipped";
        case DriverResult::Succeeded: return "succeeded";
        case DriverResult::Failed: return "failed";
        default: return "unknown";
    }
}

#if defined(UNIT_TEST)
namespace test {

void resetRetained() {
    memset(&g_retained, 0, sizeof(g_retained));
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
}

void corruptRetained() {
    memset(&g_retained, 0xA5, sizeof(g_retained));
}

void corruptLatestRecord() {
    const int slot = latestValidSlot();
    if (slot >= 0) {
        g_retained.slots[slot].crc32 ^= 1u;
    }
}

size_t retainedRecordSize() {
    return sizeof(RetainedRecord);
}

} // namespace test
#endif

} // namespace BacklightWakeBreadcrumbs
