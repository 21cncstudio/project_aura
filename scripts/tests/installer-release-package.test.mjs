import assert from "node:assert/strict";
import { createHash, generateKeyPairSync, verify } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import test from "node:test";
import {
  effectiveReleaseVersion,
  signatureBytes,
} from "../lib/installer-release.mjs";

const packageScript = fileURLToPath(
  new URL("../package_installer_release.mjs", import.meta.url),
);
const sourceCommit = "0123456789abcdef0123456789abcdef01234567";

function installerKeyId(publicKey) {
  return `aura-installer-ed25519-${createHash("sha256")
    .update(publicKey.export({ type: "spki", format: "der" }))
    .digest("hex")
    .slice(0, 16)}`;
}

function esp32S3ImageHeader() {
  const bytes = Buffer.alloc(48);
  bytes[0] = 0xe9;
  bytes[1] = 1;
  bytes[3] = 0x40;
  bytes.writeUInt16LE(0x0009, 12);
  bytes.writeUInt32LE(0x3fc80000, 24);
  bytes.writeUInt32LE(4, 28);
  bytes.set([1, 2, 3, 4], 32);
  bytes[47] = 0xef ^ 1 ^ 2 ^ 3 ^ 4;
  return bytes;
}

function identityArgs(target = "4_3") {
  return target === "7"
    ? [
        "--environment", "project_aura_7",
        "--hardware-profile", "7_dual_i2c_scl6",
        "--hardware-target", "aura-aq-7-v1",
        "--build-id", "0123456-7_dual_i2c_scl6",
      ]
    : [
        "--environment", "project_aura",
        "--hardware-profile", "4_3",
        "--hardware-target", "aura-aq-v1",
        "--build-id", "0123456",
      ];
}

async function fixture({ target = "4_3", version = "1.1.6" } = {}) {
  const root = await mkdtemp(join(tmpdir(), "aura-installer-package-"));
  const source = join(root, "source");
  const staging = join(root, "staging");
  const { privateKey, publicKey } = generateKeyPairSync("ed25519");
  const privateKeyPath = join(root, "private.pem");
  await mkdir(source);
  await writeFile(
    privateKeyPath,
    privateKey.export({ format: "pem", type: "pkcs8" }),
  );
  const parts = [
    ["bootloader.bin", "0x0000"],
    ["partitions.bin", "0x8000"],
    ["boot_app0.bin", "0xE000"],
    ["firmware.bin", "0x10000"],
    ["littlefs.bin", "0xC90000"],
  ];
  const binaryFixtures = {
    "bootloader.bin": esp32S3ImageHeader(),
    "partitions.bin": Buffer.from([0xaa, 0x50]),
    "boot_app0.bin": Buffer.from([0x01, 0x00, 0x00, 0x00]),
    "firmware.bin": esp32S3ImageHeader(),
    "littlefs.bin": Buffer.from([0xff]),
  };
  for (const [fileName] of parts) {
    await writeFile(join(source, fileName), binaryFixtures[fileName]);
  }
  const seven = target === "7";
  const hardwareTarget = seven ? "aura-aq-7-v1" : "aura-aq-v1";
  const hardwareProfile = seven ? "7_dual_i2c_scl6" : "4_3";
  const buildId = seven ? "0123456-7_dual_i2c_scl6" : "0123456";
  const manifestIdentity = {
    version: effectiveReleaseVersion(version, buildId),
    hardware_target: hardwareTarget,
    hardware_profile: hardwareProfile,
    build_id: buildId,
  };
  await writeFile(join(source, "manifest.json"), JSON.stringify({
    ...manifestIdentity,
    builds: [{ chipFamily: "ESP32-S3", parts: parts.map(([path, offset]) => ({ path, offset })) }],
  }));
  await writeFile(join(source, "manifest-update.json"), JSON.stringify({
    ...manifestIdentity,
    builds: [{ chipFamily: "ESP32-S3", parts: [{ path: "firmware.bin", offset: "0x10000" }] }],
  }));
  await writeFile(join(source, "release-artifacts.json"), JSON.stringify({
    schema: "project-aura.release-artifacts.v1",
    environment: seven ? "project_aura_7" : "project_aura",
    source_commit: sourceCommit,
    build_id: buildId,
    hardware_profile: hardwareProfile,
    hardware_target: hardwareTarget,
    files: parts.map(([fileName]) => ({
      file_name: fileName,
      size_bytes: binaryFixtures[fileName].byteLength,
      sha256: createHash("sha256").update(binaryFixtures[fileName]).digest("hex"),
    })),
  }));
  const notes = join(root, "notes.md");
  await writeFile(notes, "Release notes.");
  return {
    root,
    source,
    staging,
    privateKeyPath,
    publicKey,
    keyId: installerKeyId(publicKey),
    notes,
  };
}

test("package CLI prepares a verifiable signed Stable release directory", async () => {
  const value = await fixture();
  try {
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.equal(result.status, 0, result.stderr);
    const release = JSON.parse(
      await readFile(join(value.staging, "release.json"), "utf8"),
    );
    assert.equal(release.assets.length, 5);
    assert.deepEqual(release.modes, ["full", "update"]);
    assert.equal(release.signature.schema, "aura-firmware-release-signature-v2");
    assert.equal(release.compatibility.hardware_profile, "4_3");
    assert.equal(release.provenance.build_id, "0123456");
    assert.match(release.release_notes_sha256, /^[a-f0-9]{64}$/);
    assert.equal(
      verify(
        null,
        signatureBytes(release),
        value.publicKey,
        Buffer.from(release.signature.value, "base64"),
      ),
      true,
    );

    release.assets[0].sha256 = "0".repeat(64);
    assert.equal(
      verify(
        null,
        signatureBytes(release),
        value.publicKey,
        Buffer.from(release.signature.value, "base64"),
      ),
      false,
    );

    release.assets[0].sha256 = createHash("sha256")
      .update(await readFile(join(value.source, release.assets[0].file_name)))
      .digest("hex");
    release.title = "Tampered title";
    assert.equal(
      verify(
        null,
        signatureBytes(release),
        value.publicKey,
        Buffer.from(release.signature.value, "base64"),
      ),
      false,
    );
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});

test("package CLI rejects a version with the wrong format", async () => {
  const value = await fixture();
  try {
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "v1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Invalid release version/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});

test("package CLI creates an isolated 7-inch identity and rejects cross-pairs", async () => {
  const value = await fixture({ target: "7", version: "1.1.6-beta" });
  try {
    const valid = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6-beta",
      "--channel", "beta",
      "--commit", sourceCommit,
      ...identityArgs("7"),
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.equal(valid.status, 0, valid.stderr);
    const release = JSON.parse(await readFile(join(value.staging, "release.json"), "utf8"));
    assert.equal(release.hardware_target, "aura-aq-7-v1");
    assert.equal(release.compatibility.hardware_profile, "7_dual_i2c_scl6");
    assert.equal(release.version, "1.1.6-beta-0123456-7_dual_i2c_scl6");

    await rm(value.staging, { recursive: true, force: true });
    const invalid = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6-beta",
      "--channel", "beta",
      "--commit", sourceCommit,
      "--environment", "project_aura_7",
      "--hardware-profile", "7_dual_i2c_scl6",
      "--hardware-target", "aura-aq-v1",
      "--build-id", "0123456-7_dual_i2c_scl6",
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.notEqual(invalid.status, 0);
    assert.match(invalid.stderr, /Unsupported hardware identity/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});

test("package CLI rejects manifest identity drift and invalid ESP32-S3 headers", async () => {
  const identityDrift = await fixture();
  try {
    const manifestPath = join(identityDrift.source, "manifest.json");
    const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
    manifest.hardware_target = "aura-aq-7-v1";
    await writeFile(manifestPath, JSON.stringify(manifest));
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", identityDrift.source,
      "--staging", identityDrift.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", identityDrift.keyId,
      "--private-key", identityDrift.privateKeyPath,
      "--notes", identityDrift.notes,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /does not match the selected release identity/);
  } finally {
    await rm(identityDrift.root, { recursive: true, force: true });
  }

  const invalidHeader = await fixture();
  try {
    await writeFile(join(invalidHeader.source, "firmware.bin"), Buffer.from([0, 0, 0, 0]));
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", invalidHeader.source,
      "--staging", invalidHeader.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", invalidHeader.keyId,
      "--private-key", invalidHeader.privateKeyPath,
      "--notes", invalidHeader.notes,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Invalid ESP32-S3 binary header/);
  } finally {
    await rm(invalidHeader.root, { recursive: true, force: true });
  }
});

test("package CLI rejects a binary that no longer matches the post-build stamp", async () => {
  const value = await fixture();
  try {
    const changedFirmware = esp32S3ImageHeader();
    changedFirmware[4] = 1;
    await writeFile(join(value.source, "firmware.bin"), changedFirmware);
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Post-build artifact stamp mismatch/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});

test("package CLI binds stable and beta channels to version shape", async () => {
  const prerelease = await fixture({ version: "1.1.6-beta" });
  try {
    const stablePrerelease = spawnSync(process.execPath, [
      packageScript,
      "--source", prerelease.source,
      "--staging", prerelease.staging,
      "--version", "1.1.6-beta",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", prerelease.keyId,
      "--private-key", prerelease.privateKeyPath,
      "--notes", prerelease.notes,
    ], { encoding: "utf8" });
    assert.notEqual(stablePrerelease.status, 0);
    assert.match(stablePrerelease.stderr, /Stable releases require an exact X\.Y\.Z/);
  } finally {
    await rm(prerelease.root, { recursive: true, force: true });
  }

  const stable = await fixture();
  try {
    const betaStable = spawnSync(process.execPath, [
      packageScript,
      "--source", stable.source,
      "--staging", stable.staging,
      "--version", "1.1.6",
      "--channel", "beta",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", stable.keyId,
      "--private-key", stable.privateKeyPath,
      "--notes", stable.notes,
    ], { encoding: "utf8" });
    assert.notEqual(betaStable.status, 0);
    assert.match(betaStable.stderr, /Beta releases require an explicit prerelease suffix/);
  } finally {
    await rm(stable.root, { recursive: true, force: true });
  }
});

test("package CLI binds key ID to the supplied Ed25519 private key", async () => {
  const value = await fixture();
  try {
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", "aura-installer-ed25519-0000000000000000",
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /key ID does not match the supplied private key/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});

test("package CLI rejects a non-S3 ESP image header", async () => {
  const value = await fixture();
  try {
    const otherEsp = esp32S3ImageHeader();
    otherEsp.writeUInt16LE(0x000c, 12);
    await writeFile(join(value.source, "firmware.bin"), otherEsp);
    const result = spawnSync(process.execPath, [
      packageScript,
      "--source", value.source,
      "--staging", value.staging,
      "--version", "1.1.6",
      "--channel", "stable",
      "--commit", sourceCommit,
      ...identityArgs(),
      "--key-id", value.keyId,
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Invalid ESP32-S3 binary header/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});
