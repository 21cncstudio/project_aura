// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/BacklightWakeBreadcrumbs.h"

#include <stddef.h>
#include <string.h>

#if !defined(UNIT_TEST)
#include "core/BootState.h"
#include <esp_attr.h>
#endif

namespace BacklightWakeBreadcrumbs {
namespace {

constexpr uint32_t kMagic = 0x424C5754u; // "BLWT"
constexpr uint16_t kLegacyVersion = 2;
constexpr uint16_t kVersion = 3;
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
constexpr uint8_t kProbeLineFlagsMask =
    kProbeFlagValid | kProbeFlagSdaHigh | kProbeFlagSclHigh;
constexpr uint8_t kCommandResultShift = 3;
constexpr uint8_t kCommandResultMask = 0x3u << kCommandResultShift;
constexpr uint8_t kPreQuietStateFlag = 1u << 7;
constexpr uint8_t kPreQuietActiveOperationsMask = 0x7Fu;
constexpr uint32_t kEvidenceMarkerMagic = 0x42574D4Bu; // "BWMK"
constexpr uint32_t kEvidenceFlagUncertain = 1u << 0;
constexpr uint32_t kEvidenceFlagTornSibling = 1u << 1;
constexpr uint32_t kEvidenceKnownFlags =
    kEvidenceFlagUncertain | kEvidenceFlagTornSibling;

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
    uint8_t pre_quiet_state;
    uint16_t pre_quiet_elapsed_ms;
    uint32_t crc32;
};

struct RetainedStorage {
    RetainedRecord slots[2];
};

struct RetainedEvidenceMarker {
    uint32_t magic;
    uint32_t generation;
    uint32_t revision;
    uint32_t flags;
    uint32_t crc32;
};

struct RetainedEvidenceStorage {
    RetainedEvidenceMarker slots[2];
};

static_assert(sizeof(RetainedRecord) == 60, "retained record layout changed");
static_assert(sizeof(RetainedStorage) == 120, "retained storage layout changed");
static_assert(sizeof(RetainedEvidenceMarker) == 20,
              "retained evidence marker layout changed");
static_assert(sizeof(RetainedEvidenceStorage) == 40,
              "retained evidence storage layout changed");

#if defined(UNIT_TEST)
RetainedStorage g_retained{};
RetainedEvidenceStorage g_evidence_storage{};
#else
RTC_NOINIT_ATTR RetainedStorage g_retained;
static_assert(
    sizeof(boot_backlight_wake_evidence_words) ==
        sizeof(RetainedEvidenceStorage),
    "BootState retained evidence storage size changed");
#endif

BootSnapshot g_boot_snapshot{};
uint32_t g_boot_revision = 0;

uint8_t *evidenceStorageBytes() {
#if defined(UNIT_TEST)
    return reinterpret_cast<uint8_t *>(&g_evidence_storage);
#else
    return reinterpret_cast<uint8_t *>(boot_backlight_wake_evidence_words);
#endif
}

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
    if (record.magic != kMagic ||
        (record.version != kLegacyVersion && record.version != kVersion) ||
        record.size != sizeof(RetainedRecord) ||
        record.driver_result > static_cast<uint8_t>(DriverResult::Failed) ||
        record.crc32 != recordCrc(record)) {
        return false;
    }

    const uint8_t max_event = record.version == kLegacyVersion
        ? static_cast<uint8_t>(Event::AlarmWake)
        : static_cast<uint8_t>(Event::MqttWake);
    const uint8_t max_stage = record.version == kLegacyVersion
        ? static_cast<uint8_t>(Stage::PowerSettleReturned)
        : static_cast<uint8_t>(Stage::Aborted);
    if (record.event > max_event || record.stage > max_stage) {
        return false;
    }

    if (record.version == kLegacyVersion) {
        return true;
    }

    const CommandResult command_result = static_cast<CommandResult>(
        (record.probe_flags & kCommandResultMask) >> kCommandResultShift);
    const Stage stage = static_cast<Stage>(record.stage);
    if ((stage == Stage::Completed && command_result != CommandResult::Succeeded) ||
        (stage == Stage::Failed && command_result != CommandResult::Failed) ||
        (stage == Stage::Aborted && command_result != CommandResult::Aborted)) {
        return false;
    }
    return true;
}

uint32_t evidenceMarkerCrc(const RetainedEvidenceMarker &marker) {
    return crc32(reinterpret_cast<const uint8_t *>(&marker),
                 offsetof(RetainedEvidenceMarker, crc32));
}

bool evidenceMarkerValid(const RetainedEvidenceMarker &marker) {
    return marker.magic == kEvidenceMarkerMagic &&
           marker.generation != 0 &&
           (marker.flags & kEvidenceFlagUncertain) != 0 &&
           (marker.flags & ~kEvidenceKnownFlags) == 0 &&
           marker.crc32 == evidenceMarkerCrc(marker);
}

bool evidenceMarkerAllZero(const RetainedEvidenceMarker &marker) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&marker);
    for (size_t i = 0; i < sizeof(marker); ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

RetainedEvidenceMarker loadEvidenceMarker(int slot) {
    RetainedEvidenceMarker marker{};
    memcpy(&marker,
           evidenceStorageBytes() + slot * sizeof(RetainedEvidenceMarker),
           sizeof(marker));
    return marker;
}

void storeEvidenceMarker(int slot, const RetainedEvidenceMarker &marker) {
    memcpy(evidenceStorageBytes() + slot * sizeof(RetainedEvidenceMarker),
           &marker,
           sizeof(marker));
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

int latestValidEvidenceSlot() {
    const RetainedEvidenceMarker slot0 = loadEvidenceMarker(0);
    const RetainedEvidenceMarker slot1 = loadEvidenceMarker(1);
    const bool slot0_valid = evidenceMarkerValid(slot0);
    const bool slot1_valid = evidenceMarkerValid(slot1);
    if (!slot0_valid) {
        return slot1_valid ? 1 : -1;
    }
    if (!slot1_valid) {
        return 0;
    }
    return static_cast<int32_t>(slot1.generation - slot0.generation) > 0
        ? 1
        : 0;
}

RetainedEvidenceMarker currentEvidenceMarker() {
    const int slot = latestValidEvidenceSlot();
    return slot >= 0 ? loadEvidenceMarker(slot) : RetainedEvidenceMarker{};
}

bool evidenceStorageHasInvalidNonzeroSlot() {
    for (int slot = 0; slot < 2; ++slot) {
        const RetainedEvidenceMarker marker = loadEvidenceMarker(slot);
        if (!evidenceMarkerAllZero(marker) && !evidenceMarkerValid(marker)) {
            return true;
        }
    }
    return false;
}

void clearEvidenceMarker() {
    memset(evidenceStorageBytes(), 0, sizeof(RetainedEvidenceStorage));
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

void retainUncertainEvidence(uint32_t revision, bool torn_sibling) {
    const int current_slot = latestValidEvidenceSlot();
    const RetainedEvidenceMarker current = current_slot >= 0
        ? loadEvidenceMarker(current_slot)
        : RetainedEvidenceMarker{};
    if (current_slot < 0) {
        // The storage did not exist in v2 firmware and may contain arbitrary
        // RTC bytes after an OTA upgrade. Establish a clean dual-slot journal.
        memset(evidenceStorageBytes(), 0, sizeof(RetainedEvidenceStorage));
    }

    RetainedEvidenceMarker marker{};
    marker.magic = kEvidenceMarkerMagic;
    marker.generation = current.generation + 1u;
    if (marker.generation == 0) {
        marker.generation = 1;
    }
    marker.revision = revision;
    marker.flags = kEvidenceFlagUncertain |
        (torn_sibling ? kEvidenceFlagTornSibling : 0u);
    marker.crc32 = evidenceMarkerCrc(marker);
    storeEvidenceMarker(current_slot == 0 ? 1 : 0, marker);
}

bool evidenceMarkerAppliesTo(uint32_t revision) {
    const RetainedEvidenceMarker marker = currentEvidenceMarker();
    return evidenceMarkerValid(marker) && marker.revision == revision;
}

bool evidenceMarkerReportsTornSibling(uint32_t revision) {
    const RetainedEvidenceMarker marker = currentEvidenceMarker();
    return evidenceMarkerValid(marker) && marker.revision == revision &&
           (marker.flags & kEvidenceFlagTornSibling) != 0;
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

bool recordAllZero(const RetainedRecord &record) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < sizeof(record); ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

bool hasInvalidNonzeroSibling(int valid_slot) {
    for (int slot = 0; slot < 2; ++slot) {
        if (slot == valid_slot) {
            continue;
        }
        const RetainedRecord &record = g_retained.slots[slot];
        if (!recordAllZero(record) && !recordValid(record)) {
            return true;
        }
    }
    return false;
}

bool isTerminalStage(Stage stage) {
    return stage == Stage::Completed ||
           stage == Stage::Failed ||
           stage == Stage::Aborted;
}

void markStage(Stage stage) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
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

uint8_t encodeCommandResult(uint8_t probe_flags, CommandResult result) {
    const uint8_t encoded = static_cast<uint8_t>(result) << kCommandResultShift;
    return static_cast<uint8_t>((probe_flags & ~kCommandResultMask) |
                                (encoded & kCommandResultMask));
}

CommandResult decodeCommandResult(const RetainedRecord &record) {
    if (record.version == kLegacyVersion) {
        return CommandResult::Unknown;
    }
    return static_cast<CommandResult>(
        (record.probe_flags & kCommandResultMask) >> kCommandResultShift);
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
    trace.pre_quiet_elapsed_ms = record.pre_quiet_elapsed_ms;
    trace.pre_quiet_active_operations =
        record.pre_quiet_state & kPreQuietActiveOperationsMask;
    if (record.version == kLegacyVersion) {
        trace.pre_quiet_forced_by_timeout =
            (record.pre_quiet_state & kPreQuietStateFlag) != 0;
    } else {
        trace.pre_quiet_wait_exceeded =
            (record.pre_quiet_state & kPreQuietStateFlag) != 0;
        trace.pre_quiet_wait_exceeded_active_operations =
            record.pre_quiet_state & kPreQuietActiveOperationsMask;
    }
    trace.expected_network_manager_addr = record.expected_network_manager_addr;
    trace.post_backlight_network_manager_addr =
        record.post_backlight_network_manager_addr;
    trace.pre_render_network_manager_addr = record.pre_render_network_manager_addr;
    trace.post_backlight_task_handle = record.post_backlight_task_handle;
    trace.pre_render_task_handle = record.pre_render_task_handle;
    trace.event = static_cast<Event>(record.event);
    trace.stage = static_cast<Stage>(record.stage);
    trace.driver_result = static_cast<DriverResult>(record.driver_result);
    trace.command_result = decodeCommandResult(record);
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

CaptureStatus captureStatusForStage(Stage stage) {
    switch (stage) {
        case Stage::Completed: return CaptureStatus::Completed;
        case Stage::Failed: return CaptureStatus::Failed;
        case Stage::Aborted: return CaptureStatus::Aborted;
        default: return CaptureStatus::Active;
    }
}

void captureBootSnapshot(const RetainedRecord &record,
                         bool retention_uncertain) {
    g_boot_snapshot.has_trace = true;
    g_boot_snapshot.retention_uncertain = retention_uncertain;
    g_boot_snapshot.trace = decodeTrace(record);
    g_boot_snapshot.status = captureStatusForStage(g_boot_snapshot.trace.stage);
}

void beginWakeAtStage(Event event,
                      Stage stage,
                      uint32_t uptime_ms,
                      uint32_t epoch_s,
                      bool target_on,
                      bool previous_on,
                      LineState before) {
    if (event == Event::None || event > Event::MqttWake) {
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
    record.stage = static_cast<uint8_t>(stage);
    record.driver_result = static_cast<uint8_t>(DriverResult::Unknown);
    record.probe_flags = encodeCommandResult(
        record.probe_flags, CommandResult::Unknown);
    record.flags = encodePrimaryFlags(target_on, previous_on, before, {});
    commitRecord(record);
}

} // namespace

void initializeAtBoot(bool cold_power_start,
                      bool preserve_valid_trace_on_cold_start) {
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
    if (cold_power_start) {
        const bool retained_storage_nonempty = !storageAllZero();
        if (preserve_valid_trace_on_cold_start) {
            const int slot = latestValidSlot();
            if (slot >= 0) {
                const RetainedRecord &record = g_retained.slots[slot];
                g_boot_revision = record.revision;
                const bool torn_sibling = hasInvalidNonzeroSibling(slot);
                if (record.event != static_cast<uint8_t>(Event::None) &&
                    record.stage != static_cast<uint8_t>(Stage::None)) {
                    captureBootSnapshot(record, true);
                }
                if (torn_sibling) {
                    // Preserve a decodable fallback trace, but do not present
                    // it as the latest complete retained state when its sibling
                    // contains a torn write.
                    g_boot_snapshot.status = CaptureStatus::Corrupt;
                    g_boot_snapshot.retention_uncertain = true;
                }
                if (g_boot_snapshot.has_trace || torn_sibling) {
                    retainUncertainEvidence(record.revision, torn_sibling);
                    return;
                }
            } else if (retained_storage_nonempty) {
                g_boot_snapshot.status = CaptureStatus::Corrupt;
                g_boot_snapshot.retention_uncertain = true;
                retainUncertainEvidence(0, true);
                return;
            }
        }
        memset(&g_retained, 0, sizeof(g_retained));
        clearEvidenceMarker();
        g_boot_snapshot.status = CaptureStatus::PowerLost;
        return;
    }

    const int slot = latestValidSlot();
    if (slot < 0) {
        const bool retained_storage_nonempty = !storageAllZero();
        const RetainedEvidenceMarker marker = currentEvidenceMarker();
        const bool marker_evidence_present =
            evidenceMarkerValid(marker) ||
            evidenceStorageHasInvalidNonzeroSlot();
        g_boot_snapshot.status =
            retained_storage_nonempty || marker_evidence_present
            ? CaptureStatus::Corrupt
            : CaptureStatus::Empty;
        g_boot_snapshot.retention_uncertain = marker_evidence_present;
        return;
    }

    const RetainedRecord &record = g_retained.slots[slot];
    g_boot_revision = record.revision;
    const bool marker_applies = evidenceMarkerAppliesTo(record.revision);
    const bool marker_torn =
        evidenceMarkerReportsTornSibling(record.revision);
    const bool record_torn = hasInvalidNonzeroSibling(slot);
    // v2 firmware had no marker storage. Ignore arbitrary invalid bytes beyond
    // its RTC layout, but treat an invalid marker as evidence once a v3 record
    // proves this firmware initialized that area.
    const bool marker_torn_or_corrupt = marker_torn ||
        (record.version == kVersion &&
         evidenceStorageHasInvalidNonzeroSlot());
    const bool retained_uncertain =
        marker_applies || record_torn || marker_torn_or_corrupt;
    const bool torn_evidence = record_torn || marker_torn_or_corrupt;
    if (record.event == static_cast<uint8_t>(Event::None) ||
        record.stage == static_cast<uint8_t>(Stage::None)) {
        g_boot_snapshot.status = torn_evidence
            ? CaptureStatus::Corrupt
            : CaptureStatus::Empty;
        g_boot_snapshot.retention_uncertain = retained_uncertain;
        return;
    }

    captureBootSnapshot(record, retained_uncertain);
    if (torn_evidence) {
        g_boot_snapshot.status = CaptureStatus::Corrupt;
    }
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
    const bool clean_corrupt_sibling =
        g_boot_snapshot.status == CaptureStatus::Corrupt;
    RetainedRecord empty{};
    if (recordValid(latest_before_ack)) {
        empty.sequence = latest_before_ack.sequence;
    }
    // Commit a durable empty anchor before removing corrupt evidence. If reset
    // interrupts this write, the previous valid fallback and marker remain.
    commitRecord(empty);
    if (clean_corrupt_sibling) {
        const int empty_slot = latestValidSlot();
        if (empty_slot >= 0) {
            const int sibling_slot = empty_slot == 0 ? 1 : 0;
            memset(&g_retained.slots[sibling_slot],
                   0,
                   sizeof(g_retained.slots[sibling_slot]));
            __atomic_thread_fence(__ATOMIC_RELEASE);
        }
    }
    clearEvidenceMarker();
}

void beginWake(Event event,
               uint32_t uptime_ms,
               uint32_t epoch_s,
               bool target_on,
               bool previous_on,
               LineState before) {
    beginWakeAtStage(event,
                     Stage::Request,
                     uptime_ms,
                     epoch_s,
                     target_on,
                     previous_on,
                     before);
}

void beginPreQuietWake(Event event,
                       uint32_t uptime_ms,
                       uint32_t epoch_s,
                       bool target_on,
                       bool previous_on,
                       LineState at_request) {
    beginWakeAtStage(event,
                     Stage::PreQuietBegin,
                     uptime_ms,
                     epoch_s,
                     target_on,
                     previous_on,
                     at_request);
}

void updateWakeEvent(Event event) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage)) ||
        event == Event::None || event > Event::MqttWake ||
        static_cast<uint8_t>(event) == record.event) {
        return;
    }
    record.event = static_cast<uint8_t>(event);
    commitRecord(record);
}

void markPreQuietWaitExceeded(uint32_t elapsed_ms,
                              uint32_t active_operations) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage)) ||
        (record.pre_quiet_state & kPreQuietStateFlag) != 0) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::PreQuietWaitExceeded);
    record.pre_quiet_elapsed_ms = static_cast<uint16_t>(
        elapsed_ms > UINT16_MAX ? UINT16_MAX : elapsed_ms);
    record.pre_quiet_state = static_cast<uint8_t>(
        active_operations > kPreQuietActiveOperationsMask
            ? kPreQuietActiveOperationsMask
            : active_operations);
    record.pre_quiet_state |= kPreQuietStateFlag;
    commitRecord(record);
}

void markPreQuietReady(uint32_t elapsed_ms, LineState before_driver) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::PreQuietReady);
    record.pre_quiet_elapsed_ms = static_cast<uint16_t>(
        elapsed_ms > UINT16_MAX ? UINT16_MAX : elapsed_ms);
    const bool target_on = (record.flags & kFlagTargetOn) != 0;
    const bool previous_on = (record.flags & kFlagPreviousOn) != 0;
    const LineState after_driver = decodeLine(record.flags,
                                               kFlagAfterDriverValid,
                                               kFlagAfterDriverSdaHigh,
                                               kFlagAfterDriverSclHigh);
    record.flags = encodePrimaryFlags(
        target_on, previous_on, before_driver, after_driver);
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
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
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
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::WakeProbeUpdateReturned);
    record.probe_flags = static_cast<uint8_t>(
        (record.probe_flags & ~kProbeLineFlagsMask) | encodeProbeFlags(after));
    commitRecord(record);
}

void markLvglActivityBegin() {
    markStage(Stage::LvglActivityBegin);
}

void markLvglActivityReturned() {
    markStage(Stage::LvglActivityReturned);
}

void markCommandReturned(CommandResult result) {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
        return;
    }
    record.probe_flags = encodeCommandResult(record.probe_flags, result);
    switch (result) {
        case CommandResult::Failed:
            record.stage = static_cast<uint8_t>(Stage::Failed);
            break;
        case CommandResult::Aborted:
            record.stage = static_cast<uint8_t>(Stage::Aborted);
            break;
        default:
            record.stage = static_cast<uint8_t>(Stage::CommandReturned);
            break;
    }
    commitRecord(record);
}

void markCommandReturnedPendingSettle(CommandResult result) {
    if (result != CommandResult::Succeeded && result != CommandResult::Failed) {
        return;
    }
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        record.event == static_cast<uint8_t>(Event::None) ||
        isTerminalStage(static_cast<Stage>(record.stage))) {
        return;
    }
    record.probe_flags = encodeCommandResult(record.probe_flags, result);
    record.stage = static_cast<uint8_t>(Stage::CommandReturned);
    commitRecord(record);
}

void markGuardSettleBegin() {
    RetainedRecord record = currentRecord();
    if (!recordValid(record)) {
        return;
    }
    const CommandResult result = decodeCommandResult(record);
    if (result != CommandResult::Succeeded && result != CommandResult::Failed) {
        return;
    }
    markStage(Stage::GuardSettleBegin);
}

void markGuardSettleReturned() {
    RetainedRecord record = currentRecord();
    if (!recordValid(record)) {
        return;
    }
    const CommandResult result = decodeCommandResult(record);
    if (result != CommandResult::Succeeded && result != CommandResult::Failed) {
        return;
    }
    markStage(Stage::GuardSettleReturned);
}

void markCompleted() {
    RetainedRecord record = currentRecord();
    const Stage stage = static_cast<Stage>(record.stage);
    if (!recordValid(record) ||
        decodeCommandResult(record) != CommandResult::Succeeded ||
        (stage != Stage::GuardSettleReturned &&
         stage != Stage::CommandReturned)) {
        return;
    }
    markStage(Stage::Completed);
}

void markFailed() {
    RetainedRecord record = currentRecord();
    if (!recordValid(record) ||
        decodeCommandResult(record) != CommandResult::Failed ||
        static_cast<Stage>(record.stage) != Stage::GuardSettleReturned) {
        return;
    }
    record.stage = static_cast<uint8_t>(Stage::Failed);
    commitRecord(record);
}

void markAborted() {
    markCommandReturned(CommandResult::Aborted);
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
        case CaptureStatus::Failed: return "failed";
        case CaptureStatus::Aborted: return "aborted";
        default: return "unknown";
    }
}

const char *eventText(Event event) {
    switch (event) {
        case Event::None: return "none";
        case Event::ScheduleWake: return "schedule_wake";
        case Event::TouchWake: return "touch_wake";
        case Event::AlarmWake: return "alarm_wake";
        case Event::WebWake: return "web_wake";
        case Event::MqttWake: return "mqtt_wake";
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
        case Stage::PreQuietBegin: return "pre_quiet_begin";
        case Stage::PreQuietWaitExceeded: return "pre_quiet_wait_exceeded";
        case Stage::PreQuietReady: return "pre_quiet_ready";
        case Stage::GuardSettleBegin: return "guard_settle_begin";
        case Stage::GuardSettleReturned: return "guard_settle_returned";
        case Stage::Failed: return "failed";
        case Stage::Aborted: return "aborted";
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

const char *commandResultText(CommandResult result) {
    switch (result) {
        case CommandResult::Unknown: return "unknown";
        case CommandResult::Succeeded: return "succeeded";
        case CommandResult::Failed: return "failed";
        case CommandResult::Aborted: return "aborted";
        default: return "unknown";
    }
}

#if defined(UNIT_TEST)
namespace test {

void resetRetained() {
    memset(&g_retained, 0, sizeof(g_retained));
    clearEvidenceMarker();
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
}

void corruptRetained() {
    memset(&g_retained, 0xA5, sizeof(g_retained));
    clearEvidenceMarker();
}

void corruptLatestRecord() {
    const int slot = latestValidSlot();
    if (slot >= 0) {
        g_retained.slots[slot].crc32 ^= 1u;
    }
}

void corruptEvidenceStorage() {
    memset(evidenceStorageBytes(), 0xA5, sizeof(RetainedEvidenceStorage));
}

void corruptLatestEvidenceMarker() {
    const int slot = latestValidEvidenceSlot();
    if (slot >= 0) {
        RetainedEvidenceMarker marker = loadEvidenceMarker(slot);
        marker.crc32 ^= 1u;
        storeEvidenceMarker(slot, marker);
    }
}

void seedValidEmptyWithCorruptSibling() {
    memset(&g_retained, 0, sizeof(g_retained));
    clearEvidenceMarker();
    RetainedRecord empty{};
    empty.magic = kMagic;
    empty.version = kVersion;
    empty.size = sizeof(RetainedRecord);
    empty.revision = 4;
    empty.crc32 = recordCrc(empty);
    memcpy(&g_retained.slots[0], &empty, sizeof(empty));
    memset(&g_retained.slots[1], 0xA5, sizeof(g_retained.slots[1]));
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
}

void seedTerminalWithCorruptSibling() {
    memset(&g_retained, 0, sizeof(g_retained));
    clearEvidenceMarker();
    RetainedRecord record{};
    record.magic = kMagic;
    record.version = kVersion;
    record.size = sizeof(RetainedRecord);
    record.revision = 8;
    record.sequence = 24;
    record.uptime_ms = 5678;
    record.epoch_s = 1787000100;
    record.event = static_cast<uint8_t>(Event::TouchWake);
    record.stage = static_cast<uint8_t>(Stage::Completed);
    record.driver_result = static_cast<uint8_t>(DriverResult::Succeeded);
    record.probe_flags = encodeCommandResult(
        record.probe_flags, CommandResult::Succeeded);
    record.flags = encodePrimaryFlags(
        true, false, {true, true, true}, {true, true, true});
    record.crc32 = recordCrc(record);
    memcpy(&g_retained.slots[0], &record, sizeof(record));
    memset(&g_retained.slots[1], 0xA5, sizeof(g_retained.slots[1]));
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
}

void seedLegacyV2CompletedTrace() {
    memset(&g_retained, 0, sizeof(g_retained));
    clearEvidenceMarker();
    RetainedRecord record{};
    record.magic = kMagic;
    record.version = kLegacyVersion;
    record.size = sizeof(RetainedRecord);
    record.revision = 7;
    record.sequence = 23;
    record.uptime_ms = 4567;
    record.epoch_s = 1787000000;
    record.driver_duration_us = 321;
    record.event = static_cast<uint8_t>(Event::AlarmWake);
    record.stage = static_cast<uint8_t>(Stage::Completed);
    record.driver_result = static_cast<uint8_t>(DriverResult::Succeeded);
    record.flags = encodePrimaryFlags(
        true, false, {true, true, true}, {true, true, true});
    record.probe_flags = encodeProbeFlags({true, true, true});
    record.pre_quiet_state = static_cast<uint8_t>(
        kPreQuietStateFlag | 3u);
    record.pre_quiet_elapsed_ms = 500;
    record.crc32 = recordCrc(record);
    memcpy(&g_retained.slots[0], &record, sizeof(record));
    g_boot_snapshot = BootSnapshot{};
    g_boot_revision = 0;
}

size_t retainedRecordSize() {
    return sizeof(RetainedRecord);
}

} // namespace test
#endif

} // namespace BacklightWakeBreadcrumbs
