import assert from "node:assert/strict";
import { generateKeyPairSync, verify } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { signatureBytes } from "../lib/installer-release.mjs";

const packageScript = fileURLToPath(
  new URL("../package_installer_release.mjs", import.meta.url),
);

async function fixture() {
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
  for (let index = 0; index < parts.length; index += 1) {
    await writeFile(join(source, parts[index][0]), Buffer.alloc(index + 2, index + 1));
  }
  await writeFile(join(source, "manifest.json"), JSON.stringify({
    builds: [{ parts: parts.map(([path, offset]) => ({ path, offset })) }],
  }));
  await writeFile(join(source, "manifest-update.json"), JSON.stringify({
    builds: [{ parts: [{ path: "firmware.bin", offset: "0x10000" }] }],
  }));
  const notes = join(root, "notes.md");
  await writeFile(notes, "Release notes.");
  return { root, source, staging, privateKeyPath, publicKey, notes };
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
      "--commit", "0123456789abcdef0123456789abcdef01234567",
      "--key-id", "test-key",
      "--private-key", value.privateKeyPath,
      "--notes", value.notes,
    ], { encoding: "utf8" });
    assert.equal(result.status, 0, result.stderr);
    const release = JSON.parse(
      await readFile(join(value.staging, "release.json"), "utf8"),
    );
    assert.equal(release.assets.length, 5);
    assert.deepEqual(release.modes, ["full", "update"]);
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
      "--commit", "0123456789abcdef0123456789abcdef01234567",
      "--key-id", "test-key",
      "--private-key", value.privateKeyPath,
    ], { encoding: "utf8" });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Invalid release version/);
  } finally {
    await rm(value.root, { recursive: true, force: true });
  }
});
