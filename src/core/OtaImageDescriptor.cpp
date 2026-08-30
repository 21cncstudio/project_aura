// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OtaImageIdentity.h"
#include "AppBuildId.generated.h"

#include <esp_app_desc.h>
#include <esp_app_format.h>
#include <soc/soc.h>

static_assert(sizeof(esp_image_header_t) == OtaImageIdentity::kImageHeaderSize,
              "ESP image header layout changed");
static_assert(sizeof(esp_image_segment_header_t) == OtaImageIdentity::kSegmentHeaderSize,
              "ESP segment header layout changed");
static_assert(sizeof(esp_app_desc_t) == OtaImageIdentity::kAppDescriptorSize,
              "ESP app descriptor layout changed");
static_assert(ESP_IMAGE_HEADER_MAGIC == 0xE9 && ESP_APP_DESC_MAGIC_WORD == 0xABCD5432,
              "ESP image descriptor magic changed");
static_assert(ESP_CHIP_ID_ESP32S3 == 0x0009,
              "ESP32-S3 chip identity changed");
static_assert(SOC_DROM_LOW == 0x3C000000 && SOC_DROM_HIGH == 0x3E000000,
              "ESP32-S3 DROM layout changed");
static_assert(sizeof(APP_HARDWARE_TARGET) <= OtaImageIdentity::kTargetSize,
              "OTA hardware target does not fit descriptor");

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The OTA descriptor integer fields require a little-endian build target"
#endif

// ESP-IDF's standard custom descriptor slot is at BIN offset 0x120. `used`
// prevents compiler removal; -Wl,-u,aura_ota_image_identity prevents linker GC.
// The post-BIN build check fails if this fixed location or target ever drifts.
extern "C" {
extern const OtaImageIdentity::Descriptor aura_ota_image_identity
    __attribute__((section(".rodata_custom_desc"), used, aligned(4))) = {
        "AURA_OTA_TARGET",
        OtaImageIdentity::kDescriptorVersion,
        OtaImageIdentity::kDescriptorSize,
        APP_HARDWARE_TARGET,
        {},
    };
}
