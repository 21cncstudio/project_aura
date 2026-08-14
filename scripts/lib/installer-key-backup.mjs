import {
  createCipheriv,
  createDecipheriv,
  createHash,
  createPrivateKey,
  createPublicKey,
  randomBytes,
  scrypt as scryptCallback,
} from "node:crypto";
import { promisify } from "node:util";

export const INSTALLER_KEY_BACKUP_SCHEMA = "aura-installer-key-backup-v1";

const scrypt = promisify(scryptCallback);
const KDF_PARAMETERS = Object.freeze({ name: "scrypt", N: 32768, r: 8, p: 1 });
const CIPHER_NAME = "aes-256-gcm";
const SCRYPT_MAX_MEMORY = 128 * 1024 * 1024;

export function parseJsonWithOptionalBom(value) {
  return JSON.parse(value.replace(/^\uFEFF/, ""));
}

function requireString(value, name) {
  if (typeof value !== "string" || value.length === 0) {
    throw new Error(`Invalid installer key backup field: ${name}`);
  }
  return value;
}

function publicDer(key) {
  const publicKey = key?.type === "public" ? key : createPublicKey(key);
  return publicKey.export({ type: "spki", format: "der" });
}

export function installerKeyId(publicKey) {
  return `aura-installer-ed25519-${createHash("sha256")
    .update(publicDer(publicKey))
    .digest("hex")
    .slice(0, 16)}`;
}

function assertEd25519PrivateKey(privateKeyPem) {
  const privateKey = createPrivateKey(privateKeyPem);
  if (privateKey.asymmetricKeyType !== "ed25519") {
    throw new Error("The installer release key is not Ed25519.");
  }
  return privateKey;
}

function assertMatchingKeyMaterial(privateKey, publicKeyPem, keyId) {
  const derivedPublic = createPublicKey(privateKey);
  if (!publicDer(derivedPublic).equals(publicDer(publicKeyPem))) {
    throw new Error("Installer private and public keys do not match.");
  }
  if (installerKeyId(derivedPublic) !== keyId) {
    throw new Error("Installer key ID does not match the key material.");
  }
  return derivedPublic.export({ type: "spki", format: "pem" }).toString();
}

function validatePassphrase(passphrase) {
  if (typeof passphrase !== "string" || passphrase.length < 16) {
    throw new Error("The backup passphrase must contain at least 16 characters.");
  }
}

function backupHeader(backup) {
  return {
    schema: backup.schema,
    key_id: backup.key_id,
    algorithm: backup.algorithm,
    created_at: backup.created_at,
    key_created_at: backup.key_created_at,
    public_key: backup.public_key,
    kdf: backup.kdf,
    cipher: backup.cipher,
  };
}

function backupAad(backup) {
  return Buffer.from(JSON.stringify(backupHeader(backup)), "utf8");
}

async function deriveEncryptionKey(passphrase, salt) {
  return scrypt(passphrase, salt, 32, {
    N: KDF_PARAMETERS.N,
    r: KDF_PARAMETERS.r,
    p: KDF_PARAMETERS.p,
    maxmem: SCRYPT_MAX_MEMORY,
  });
}

export async function createInstallerKeyBackup({
  privateKeyPem,
  publicKeyPem,
  metadata,
  passphrase,
  createdAt = new Date().toISOString(),
}) {
  validatePassphrase(passphrase);
  const keyId = requireString(metadata?.key_id, "key_id");
  const privateKey = assertEd25519PrivateKey(privateKeyPem);
  const normalizedPublic = assertMatchingKeyMaterial(privateKey, publicKeyPem, keyId);
  const salt = randomBytes(16);
  const iv = randomBytes(12);
  const backup = {
    schema: INSTALLER_KEY_BACKUP_SCHEMA,
    key_id: keyId,
    algorithm: "ed25519",
    created_at: createdAt,
    key_created_at: requireString(metadata?.created_at, "key_created_at"),
    public_key: normalizedPublic,
    kdf: { ...KDF_PARAMETERS, salt_base64: salt.toString("base64") },
    cipher: { name: CIPHER_NAME, iv_base64: iv.toString("base64") },
    encrypted_private_key_base64: "",
    authentication_tag_base64: "",
  };
  const encryptionKey = await deriveEncryptionKey(passphrase, salt);
  try {
    const cipher = createCipheriv(CIPHER_NAME, encryptionKey, iv);
    cipher.setAAD(backupAad(backup));
    const ciphertext = Buffer.concat([
      cipher.update(Buffer.from(privateKeyPem)),
      cipher.final(),
    ]);
    backup.encrypted_private_key_base64 = ciphertext.toString("base64");
    backup.authentication_tag_base64 = cipher.getAuthTag().toString("base64");
  } finally {
    encryptionKey.fill(0);
  }
  return backup;
}

function validateBackupStructure(backup) {
  if (!backup || backup.schema !== INSTALLER_KEY_BACKUP_SCHEMA) {
    throw new Error("Unsupported installer key backup format.");
  }
  requireString(backup.key_id, "key_id");
  requireString(backup.created_at, "created_at");
  requireString(backup.key_created_at, "key_created_at");
  requireString(backup.public_key, "public_key");
  requireString(backup.encrypted_private_key_base64, "encrypted_private_key_base64");
  requireString(backup.authentication_tag_base64, "authentication_tag_base64");
  if (backup.algorithm !== "ed25519") {
    throw new Error("Unsupported installer key algorithm.");
  }
  if (
    backup.kdf?.name !== KDF_PARAMETERS.name ||
    backup.kdf?.N !== KDF_PARAMETERS.N ||
    backup.kdf?.r !== KDF_PARAMETERS.r ||
    backup.kdf?.p !== KDF_PARAMETERS.p
  ) {
    throw new Error("Unsupported installer key backup KDF parameters.");
  }
  if (backup.cipher?.name !== CIPHER_NAME) {
    throw new Error("Unsupported installer key backup cipher.");
  }
}

export async function restoreInstallerKeyBackup({ backup, passphrase }) {
  validatePassphrase(passphrase);
  validateBackupStructure(backup);
  try {
    const salt = Buffer.from(requireString(backup.kdf.salt_base64, "salt_base64"), "base64");
    const iv = Buffer.from(requireString(backup.cipher.iv_base64, "iv_base64"), "base64");
    const tag = Buffer.from(backup.authentication_tag_base64, "base64");
    const ciphertext = Buffer.from(backup.encrypted_private_key_base64, "base64");
    if (salt.length !== 16 || iv.length !== 12 || tag.length !== 16 || ciphertext.length === 0) {
      throw new Error("Invalid encrypted payload.");
    }
    const encryptionKey = await deriveEncryptionKey(passphrase, salt);
    let privateKeyPem;
    try {
      const decipher = createDecipheriv(CIPHER_NAME, encryptionKey, iv);
      decipher.setAAD(backupAad(backup));
      decipher.setAuthTag(tag);
      privateKeyPem = Buffer.concat([
        decipher.update(ciphertext),
        decipher.final(),
      ]);
    } finally {
      encryptionKey.fill(0);
    }
    const privateKey = assertEd25519PrivateKey(privateKeyPem);
    const publicKeyPem = assertMatchingKeyMaterial(
      privateKey,
      backup.public_key,
      backup.key_id,
    );
    return {
      privateKeyPem,
      publicKeyPem,
      metadata: {
        key_id: backup.key_id,
        algorithm: "ed25519",
        created_at: backup.key_created_at,
      },
    };
  } catch (error) {
    if (
      error instanceof Error &&
      (error.message.startsWith("Installer ") || error.message.startsWith("The installer "))
    ) {
      throw error;
    }
    throw new Error("Backup authentication failed. The password is wrong or the backup is damaged.");
  }
}
