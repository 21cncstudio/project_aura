import assert from "node:assert/strict";
import test from "node:test";
import {
  FLASH_SIZE_BYTES,
  signatureBytes,
  stableJson,
  validateFlashLayout,
  validateVersion,
} from "../lib/installer-release.mjs";

test("stable JSON is independent of object insertion order", () => {
  assert.equal(stableJson({ b: 2, a: 1 }), stableJson({ a: 1, b: 2 }));
});

test("signature payload is deterministic across asset order", () => {
  const base = {
    product_key: "aura-aq",
    version: "1.1.6",
    channel: "stable",
    hardware_target: "aura-aq-v1",
    chip_family: "ESP32-S3",
    modes: ["full", "update"],
    compatibility: { flash_size_bytes: FLASH_SIZE_BYTES },
    provenance: { build_id: "abc", commit: "a".repeat(40) },
    signature: { algorithm: "ed25519", key_id: "key", value: "" },
  };
  const a = { file_name: "firmware.bin", flash_offset: 0x10000, modes: ["update", "full"], sha256: "a".repeat(64), size_bytes: 100 };
  const b = { file_name: "bootloader.bin", flash_offset: 0, modes: ["full"], sha256: "b".repeat(64), size_bytes: 10 };
  assert.deepEqual(signatureBytes({ ...base, assets: [a, b] }), signatureBytes({ ...base, assets: [b, a] }));
});

test("invalid versions and overlapping layouts are rejected", () => {
  assert.throws(() => validateVersion("v1.1.6"));
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

