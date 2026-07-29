import assert from "node:assert/strict";
import { createHash, generateKeyPairSync, verify } from "node:crypto";
import test from "node:test";
import {
  createSignedReleaseDocument,
  firmwareSignaturePayload,
  signingKeyId,
  validateReleaseBinaryHeaders,
  validateReleaseInputs,
} from "../lib/installer_release_package.mjs";

function assets() {
  return [
    ["bootloader.bin", "bootloader", 0x0000, ["full"]],
    ["partitions.bin", "partitions", 0x8000, ["full"]],
    ["boot_app0.bin", "boot_app0", 0xe000, ["full"]],
    ["firmware.bin", "firmware", 0x10000, ["full", "update"]],
    ["littlefs.bin", "littlefs", 0xc90000, ["full"]],
  ].map(([fileName, assetKind, flashOffset, modes], index) => ({
    fileName,
    assetKind,
    flashOffset,
    modes,
    sha256: String(index + 1).padStart(64, "0"),
    sizeBytes: index + 10,
  }));
}

test("creates a verifiable Stable release signature", () => {
  const { privateKey, publicKey } = generateKeyPairSync("ed25519");
  const releaseAssets = assets();
  const keyId = signingKeyId(publicKey);
  const releaseNotes = "Release notes for 1.1.6.";
  const document = createSignedReleaseDocument({
    version: "1.1.6",
    channel: "stable",
    commit: "0123456789abcdef0123456789abcdef01234567",
    buildId: "project-aura-v1.1.6-0123456",
    keyId,
    privateKeyPem: privateKey.export({ format: "pem", type: "pkcs8" }),
    assets: releaseAssets,
    releaseNotes,
  });

  const payload = firmwareSignaturePayload({
    signatureSchema: document.signature.schema,
    productKey: document.product_key,
    version: document.version,
    channel: document.channel,
    title: document.title,
    releaseNotesSha256: document.release_notes_sha256,
    hardwareTarget: document.hardware_target,
    chipFamily: document.chip_family,
    modes: document.modes,
    compatibility: document.compatibility,
    provenance: document.provenance,
    signatureAlgorithm: document.signature.algorithm,
    signatureKeyId: document.signature.key_id,
    assets: releaseAssets,
  });

  assert.equal(
    verify(
      null,
      payload,
      publicKey,
      Buffer.from(document.signature.value, "base64"),
    ),
    true,
  );
  assert.deepEqual(document.modes, ["full", "update"]);
});

test("rejects a private key whose declared key ID does not match", () => {
  const { privateKey } = generateKeyPairSync("ed25519");

  assert.throws(
    () => createSignedReleaseDocument({
      version: "1.1.6",
      channel: "stable",
      commit: "0123456789abcdef0123456789abcdef01234567",
      buildId: "project-aura-v1.1.6-0123456",
      keyId: "aura-installer-ed25519-wrong",
      privateKeyPem: privateKey.export({ format: "pem", type: "pkcs8" }),
      assets: assets(),
      releaseNotes: "Release notes.",
    }),
    /key ID mismatch/i,
  );
});

test("matches the Aura Link canonical signature payload contract", () => {
  const payload = firmwareSignaturePayload({
    signatureSchema: "aura-firmware-release-signature-v2",
    productKey: "aura-aq",
    version: "1.2.0",
    channel: "stable",
    title: "Aura AQ 1.2.0",
    releaseNotesSha256: "b".repeat(64),
    hardwareTarget: "aura-aq-v1",
    chipFamily: "ESP32-S3",
    modes: ["update", "full"],
    compatibility: {
      flash_size_bytes: 16_777_216,
      hardware_revision: "v1",
    },
    provenance: {
      build_id: "ci-123",
      commit: "abc123",
    },
    signatureAlgorithm: "ed25519",
    signatureKeyId: "aura-test",
    assets: [{
      fileName: "firmware.bin",
      assetKind: "firmware",
      flashOffset: 0x10000,
      modes: ["full", "update"],
      sha256: "a".repeat(64),
      sizeBytes: 4096,
    }],
  });

  assert.equal(
    createHash("sha256").update(payload).digest("hex"),
    "3daa85a90895723c04e8460689984ead6801c2ed7ff6bdd4a5bce49b9a4375eb",
  );
});

test("accepts ESP32-S3 16 MB release binary headers", () => {
  assert.doesNotThrow(() => validateReleaseBinaryHeaders([
    { fileName: "bootloader.bin", bytes: new Uint8Array([0xe9, 4, 2, 0x4f]) },
    { fileName: "partitions.bin", bytes: new Uint8Array([0xaa, 0x50]) },
    { fileName: "boot_app0.bin", bytes: new Uint8Array([1, 0, 0, 0]) },
    { fileName: "firmware.bin", bytes: new Uint8Array([0xe9, 6, 2, 0x4f]) },
    { fileName: "littlefs.bin", bytes: new Uint8Array([1]) },
  ], "stable"));
});

test("rejects an ESP32 image built for 8 MB flash", () => {
  assert.throws(
    () => validateReleaseBinaryHeaders([
      { fileName: "bootloader.bin", bytes: new Uint8Array([0xe9, 4, 2, 0x3f]) },
      { fileName: "partitions.bin", bytes: new Uint8Array([0xaa, 0x50]) },
      { fileName: "boot_app0.bin", bytes: new Uint8Array([1, 0, 0, 0]) },
      { fileName: "firmware.bin", bytes: new Uint8Array([0xe9, 6, 2, 0x4f]) },
      { fileName: "littlefs.bin", bytes: new Uint8Array([1]) },
    ], "stable"),
    /configured for 16 MB flash/i,
  );
});

test("rejects a dirty Git tree", () => {
  assert.throws(
    () => validateReleaseInputs({
      version: "1.1.6",
      configuredVersion: "1.1.6",
      channel: "stable",
      commit: "0123456789abcdef0123456789abcdef01234567",
      workingTreeClean: false,
    }),
    /working tree is dirty/i,
  );
});

test("rejects a version that does not match platformio.ini", () => {
  assert.throws(
    () => validateReleaseInputs({
      version: "1.1.6",
      configuredVersion: "1.1.5",
      channel: "stable",
      commit: "0123456789abcdef0123456789abcdef01234567",
      workingTreeClean: true,
    }),
    /platformio\.ini contains APP_VERSION 1\.1\.5/i,
  );
});

test("rejects a prerelease suffix on the Stable channel", () => {
  assert.throws(
    () => validateReleaseInputs({
      version: "1.1.6-beta.1",
      configuredVersion: "1.1.6-beta.1",
      channel: "stable",
      commit: "0123456789abcdef0123456789abcdef01234567",
      workingTreeClean: true,
    }),
    /Stable release versions cannot/i,
  );
});
