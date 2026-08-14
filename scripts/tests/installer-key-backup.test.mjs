import assert from "node:assert/strict";
import { generateKeyPairSync } from "node:crypto";
import test from "node:test";
import {
  createInstallerKeyBackup,
  installerKeyId,
  parseJsonWithOptionalBom,
  restoreInstallerKeyBackup,
} from "../lib/installer-key-backup.mjs";

function fixture() {
  const { privateKey, publicKey } = generateKeyPairSync("ed25519", {
    privateKeyEncoding: { type: "pkcs8", format: "pem" },
    publicKeyEncoding: { type: "spki", format: "pem" },
  });
  return {
    privateKey,
    publicKey,
    metadata: {
      key_id: installerKeyId(publicKey),
      algorithm: "ed25519",
      created_at: "2026-08-14T20:30:46.595Z",
    },
  };
}

test("installer key JSON accepts the Windows PowerShell UTF-8 BOM", () => {
  assert.deepEqual(parseJsonWithOptionalBom('\uFEFF{"key_id":"test"}'), {
    key_id: "test",
  });
});

test("portable installer key backup round-trips the exact private key", async () => {
  const value = fixture();
  const backup = await createInstallerKeyBackup({
    privateKeyPem: value.privateKey,
    publicKeyPem: value.publicKey,
    metadata: value.metadata,
    passphrase: "correct horse battery staple",
    createdAt: "2026-08-14T21:00:00.000Z",
  });
  const restored = await restoreInstallerKeyBackup({
    backup,
    passphrase: "correct horse battery staple",
  });
  assert.equal(restored.privateKeyPem.toString(), value.privateKey);
  assert.equal(restored.publicKeyPem, value.publicKey);
  assert.deepEqual(restored.metadata, value.metadata);
});

test("portable installer key backup rejects a wrong password", async () => {
  const value = fixture();
  const backup = await createInstallerKeyBackup({
    privateKeyPem: value.privateKey,
    publicKeyPem: value.publicKey,
    metadata: value.metadata,
    passphrase: "correct horse battery staple",
  });
  await assert.rejects(
    restoreInstallerKeyBackup({ backup, passphrase: "this password is incorrect" }),
    /password is wrong or the backup is damaged/i,
  );
});

test("portable installer key backup rejects tampered metadata", async () => {
  const value = fixture();
  const backup = await createInstallerKeyBackup({
    privateKeyPem: value.privateKey,
    publicKeyPem: value.publicKey,
    metadata: value.metadata,
    passphrase: "correct horse battery staple",
  });
  backup.key_created_at = "2027-01-01T00:00:00.000Z";
  await assert.rejects(
    restoreInstallerKeyBackup({ backup, passphrase: "correct horse battery staple" }),
    /password is wrong or the backup is damaged/i,
  );
});

test("portable installer key backup rejects mismatched source keys", async () => {
  const value = fixture();
  const other = fixture();
  await assert.rejects(
    createInstallerKeyBackup({
      privateKeyPem: value.privateKey,
      publicKeyPem: other.publicKey,
      metadata: value.metadata,
      passphrase: "correct horse battery staple",
    }),
    /do not match/i,
  );
});
