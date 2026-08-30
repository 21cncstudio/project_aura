import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import test from "node:test";
import {
  FLASH_SIZE_BYTES,
  SIGNATURE_SCHEMA,
  effectiveReleaseVersion,
  signatureBytes,
  stableJson,
  validateGeneratedBuildId,
  validateEsp32S3BinaryHeader,
  validateFlashLayout,
  validateHardwareIdentity,
  validateVersion,
} from "../lib/installer-release.mjs";

function validEsp32S3Image({ appendDigest = false } = {}) {
  const bytes = Buffer.alloc(appendDigest ? 80 : 48);
  bytes[0] = 0xe9;
  bytes[1] = 1;
  bytes[3] = 0x40;
  bytes.writeUInt16LE(0x0009, 12);
  bytes[23] = appendDigest ? 1 : 0;
  bytes.writeUInt32LE(0x3fc80000, 24);
  bytes.writeUInt32LE(4, 28);
  bytes.set([1, 2, 3, 4], 32);
  bytes[47] = 0xef ^ 1 ^ 2 ^ 3 ^ 4;
  if (appendDigest) {
    createHash("sha256").update(bytes.subarray(0, 48)).digest().copy(bytes, 48);
  }
  return bytes;
}

test("stable JSON is independent of object insertion order", () => {
  assert.equal(stableJson({ b: 2, a: 1 }), stableJson({ a: 1, b: 2 }));
});

test("signature payload is deterministic across asset order", () => {
  const base = {
    product_key: "aura-aq",
    version: "1.1.6",
    channel: "stable",
    title: "Aura AQ 4.3-inch 1.1.6",
    release_notes_sha256: "c".repeat(64),
    hardware_target: "aura-aq-v1",
    chip_family: "ESP32-S3",
    modes: ["full", "update"],
    compatibility: { flash_size_bytes: FLASH_SIZE_BYTES },
    provenance: {
      build_id: "aaaaaaa",
      commit: "a".repeat(40),
      environment: "project_aura",
      hardware_profile: "4_3",
      hardware_target: "aura-aq-v1",
    },
    signature: {
      schema: SIGNATURE_SCHEMA,
      algorithm: "ed25519",
      key_id: "key",
      value: "",
    },
  };
  const a = { file_name: "firmware.bin", asset_kind: "firmware", flash_offset: 0x10000, modes: ["update", "full"], sha256: "a".repeat(64), size_bytes: 100 };
  const b = { file_name: "bootloader.bin", asset_kind: "bootloader", flash_offset: 0, modes: ["full"], sha256: "b".repeat(64), size_bytes: 10 };
  assert.deepEqual(signatureBytes({ ...base, assets: [a, b] }), signatureBytes({ ...base, assets: [b, a] }));
});

test("signature payload matches the Aura Link v2 canonical field set", () => {
  const release = {
    product_key: "aura-aq",
    version: "1.2.3",
    channel: "beta",
    title: "Aura AQ 7-inch 1.2.3",
    release_notes_sha256: "b".repeat(64),
    hardware_target: "aura-aq-7-v1",
    chip_family: "ESP32-S3",
    modes: ["update", "full"],
    compatibility: { flash_size_bytes: FLASH_SIZE_BYTES, hardware_profile: "7_dual_i2c_scl6" },
    provenance: { build_id: "aaaaaaa-7_dual_i2c_scl6", commit: "a".repeat(40) },
    signature: { schema: SIGNATURE_SCHEMA, algorithm: "ed25519", key_id: "test", value: "ignored" },
    assets: [{
      file_name: "firmware.bin",
      asset_kind: "firmware",
      flash_offset: 65536,
      modes: ["update", "full"],
      sha256: "c".repeat(64),
      size_bytes: 42,
    }],
  };
  const canonical = signatureBytes(release).toString("utf8");
  assert.match(canonical, /^\{"assets":\[/);
  assert.match(canonical, /"asset_kind":"firmware"/);
  assert.match(canonical, /"release_notes_sha256":"b{64}"/);
  assert.match(canonical, /"schema":"aura-firmware-release-signature-v2"/);
  assert.match(canonical, /"title":"Aura AQ 7-inch 1.2.3"/);
});

test("hardware identity and generated build ID are strict pairs", () => {
  assert.equal(
    validateHardwareIdentity({
      environment: "project_aura_7",
      hardwareProfile: "7_dual_i2c_scl6",
      hardwareTarget: "aura-aq-7-v1",
    }).artifactSlug,
    "7",
  );
  assert.throws(() =>
    validateHardwareIdentity({
      environment: "project_aura_7",
      hardwareProfile: "7_dual_i2c_scl6",
      hardwareTarget: "aura-aq-v1",
    }),
  );
  assert.equal(
    validateGeneratedBuildId({
      buildId: "0123456-7_dual_i2c_scl6",
      commit: "0123456789abcdef0123456789abcdef01234567",
      environment: "project_aura_7",
    }),
    "0123456-7_dual_i2c_scl6",
  );
  assert.throws(() =>
    validateGeneratedBuildId({
      buildId: "0123456-7_dual_i2c_scl6-dirty",
      commit: "0123456789abcdef0123456789abcdef01234567",
      environment: "project_aura_7",
    }),
  );
});

test("effective release versions keep Stable fixed and bind prereleases to build identity", () => {
  assert.equal(effectiveReleaseVersion("1.1.6", "0123456"), "1.1.6");
  assert.equal(
    effectiveReleaseVersion("1.1.6-beta", "0123456-7_dual_i2c_scl6"),
    "1.1.6-beta-0123456-7_dual_i2c_scl6",
  );
});

test("invalid versions and overlapping layouts are rejected", () => {
  assert.throws(() => validateVersion("v1.1.6"));
  assert.throws(() => validateVersion("1.1.6-."));
  assert.throws(() =>
    validateFlashLayout(
      [
        { file_name: "a.bin", flash_offset: 0, size_bytes: 10, modes: ["full"] },
        { file_name: "b.bin", flash_offset: 5, size_bytes: 10, modes: ["full"] },
      ],
      ["full"],
    ),
  );
});

test("ESP32-S3 image validation checks structure, checksum, and optional digest", () => {
  assert.doesNotThrow(() =>
    validateEsp32S3BinaryHeader("firmware.bin", validEsp32S3Image()),
  );
  assert.doesNotThrow(() =>
    validateEsp32S3BinaryHeader(
      "firmware.bin",
      validEsp32S3Image({ appendDigest: true }),
    ),
  );

  const badChecksum = validEsp32S3Image();
  badChecksum[47] ^= 1;
  assert.throws(() =>
    validateEsp32S3BinaryHeader("firmware.bin", badChecksum),
  );

  const badDigest = validEsp32S3Image({ appendDigest: true });
  badDigest[48] ^= 1;
  assert.throws(() =>
    validateEsp32S3BinaryHeader("firmware.bin", badDigest),
  );

  const wrongChip = validEsp32S3Image();
  wrongChip.writeUInt16LE(0x000c, 12);
  assert.throws(() =>
    validateEsp32S3BinaryHeader("firmware.bin", wrongChip),
  );
  assert.throws(() =>
    validateEsp32S3BinaryHeader("firmware.bin", Buffer.alloc(24)),
  );
});
