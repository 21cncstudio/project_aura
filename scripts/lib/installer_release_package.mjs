import {
  createHash,
  createPrivateKey,
  createPublicKey,
  sign,
} from "node:crypto";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

export const RELEASE_PACKAGE_SCHEMA = "aura-firmware-release-package-v1";
export const RELEASE_SIGNATURE_SCHEMA = "aura-firmware-release-signature-v2";
export const FLASH_SIZE_BYTES = 16 * 1024 * 1024;

const STANDARD_ASSETS = [
  {
    fileName: "bootloader.bin",
    assetKind: "bootloader",
    flashOffset: 0x0000,
    modes: ["full"],
  },
  {
    fileName: "partitions.bin",
    assetKind: "partitions",
    flashOffset: 0x8000,
    modes: ["full"],
  },
  {
    fileName: "boot_app0.bin",
    assetKind: "boot_app0",
    flashOffset: 0xe000,
    modes: ["full"],
  },
  {
    fileName: "firmware.bin",
    assetKind: "firmware",
    flashOffset: 0x10000,
    modes: ["full", "update"],
  },
  {
    fileName: "littlefs.bin",
    assetKind: "littlefs",
    flashOffset: 0xc90000,
    modes: ["full"],
  },
];

const RECOVERY_ASSETS = [
  {
    fileName: "recovery.bin",
    assetKind: "recovery",
    flashOffset: 0,
    modes: ["recovery"],
  },
];

function jsonValue(value) {
  if (value === null || typeof value !== "object") return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(jsonValue).join(",")}]`;
  return `{${Object.keys(value)
    .sort()
    .map((key) => `${JSON.stringify(key)}:${jsonValue(value[key])}`)
    .join(",")}}`;
}

export function stableJson(value) {
  return jsonValue(value);
}

export function releaseAssetDefinitions(channel) {
  return channel === "recovery" ? RECOVERY_ASSETS : STANDARD_ASSETS;
}

export function validateReleaseInputs({
  version,
  configuredVersion,
  channel,
  commit,
  workingTreeClean,
}) {
  if (!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version)) {
    throw new Error("Release version must use semantic versioning, for example 1.1.6.");
  }
  if (!["stable", "beta", "recovery"].includes(channel)) {
    throw new Error("Release channel must be stable, beta, or recovery.");
  }
  if (configuredVersion !== version) {
    throw new Error(
      `platformio.ini contains APP_VERSION ${configuredVersion}; expected ${version}.`,
    );
  }
  if (channel === "stable" && version.includes("-")) {
    throw new Error("Stable release versions cannot contain a prerelease suffix.");
  }
  if (!/^[a-f0-9]{40}$/i.test(commit)) {
    throw new Error("A full 40-character Git commit is required.");
  }
  if (!workingTreeClean) {
    throw new Error("The Git working tree is dirty. Commit the release source before packaging.");
  }
}

export function firmwareSignaturePayload({
  signatureSchema = RELEASE_SIGNATURE_SCHEMA,
  productKey,
  version,
  channel,
  title,
  releaseNotesSha256,
  hardwareTarget,
  chipFamily,
  modes,
  compatibility,
  provenance,
  signatureAlgorithm,
  signatureKeyId,
  assets,
}) {
  return Buffer.from(
    stableJson({
      schema: signatureSchema,
      product_key: productKey,
      version,
      channel,
      ...(signatureSchema === RELEASE_SIGNATURE_SCHEMA
        ? {
            title,
            release_notes_sha256: releaseNotesSha256,
          }
        : {}),
      hardware_target: hardwareTarget,
      chip_family: chipFamily,
      modes: [...modes].sort(),
      compatibility,
      provenance,
      signature: {
        algorithm: signatureAlgorithm,
        key_id: signatureKeyId,
      },
      assets: assets
        .map((asset) => ({
          file_name: asset.fileName,
          ...(signatureSchema === RELEASE_SIGNATURE_SCHEMA
            ? { asset_kind: asset.assetKind }
            : {}),
          flash_offset: asset.flashOffset,
          modes: [...asset.modes].sort(),
          sha256: asset.sha256,
          size_bytes: asset.sizeBytes,
        }))
        .sort(
          (left, right) =>
            left.flash_offset - right.flash_offset ||
            left.file_name.localeCompare(right.file_name),
        ),
    }),
    "utf8",
  );
}

export function signingKeyId(publicKey) {
  const key =
    publicKey && typeof publicKey === "object" && publicKey.type === "public"
      ? publicKey
      : createPublicKey(publicKey);
  if (key.asymmetricKeyType !== "ed25519") {
    throw new Error("Installer release signing key must be Ed25519.");
  }
  const der = key.export({ format: "der", type: "spki" });
  return `aura-installer-ed25519-${createHash("sha256")
    .update(der)
    .digest("hex")
    .slice(0, 16)}`;
}

export async function readReleaseAssets(sourceDirectory, channel) {
  const assets = [];
  for (const definition of releaseAssetDefinitions(channel)) {
    const bytes = await readFile(resolve(sourceDirectory, definition.fileName));
    assets.push({
      ...definition,
      bytes,
      sha256: createHash("sha256").update(bytes).digest("hex"),
      sizeBytes: bytes.byteLength,
    });
  }
  return assets;
}

export function validateReleaseBinaryHeaders(assets, channel) {
  const byName = new Map(assets.map((asset) => [asset.fileName, asset.bytes]));
  const espImageNames = channel === "recovery"
    ? ["recovery.bin"]
    : ["bootloader.bin", "firmware.bin"];

  for (const fileName of espImageNames) {
    const bytes = byName.get(fileName);
    if (
      !bytes ||
      bytes.byteLength < 4 ||
      bytes[0] !== 0xe9 ||
      (bytes[3] >> 4) !== 4
    ) {
      throw new Error(
        `${fileName} is not an ESP32 image configured for 16 MB flash.`,
      );
    }
  }

  if (channel !== "recovery") {
    const partitions = byName.get("partitions.bin");
    if (
      !partitions ||
      partitions.byteLength < 2 ||
      partitions[0] !== 0xaa ||
      partitions[1] !== 0x50
    ) {
      throw new Error("partitions.bin does not contain an ESP32 partition table.");
    }

    const bootApp0 = byName.get("boot_app0.bin");
    if (
      !bootApp0 ||
      bootApp0.byteLength < 4 ||
      bootApp0[0] !== 0x01 ||
      bootApp0[1] !== 0x00 ||
      bootApp0[2] !== 0x00 ||
      bootApp0[3] !== 0x00
    ) {
      throw new Error("boot_app0.bin does not contain the expected OTA boot marker.");
    }
  }
}

export function createSignedReleaseDocument({
  version,
  channel,
  commit,
  buildId,
  keyId,
  privateKeyPem,
  assets,
  releaseNotes,
}) {
  const productKey = "aura-aq";
  const hardwareTarget = "aura-aq-v1";
  const chipFamily = "ESP32-S3";
  const modes = channel === "recovery" ? ["recovery"] : ["full", "update"];
  const compatibility = {
    flash_size_bytes: FLASH_SIZE_BYTES,
    hardware_revision: "v1",
  };
  const provenance = {
    build_id: buildId,
    commit,
  };
  const title = `Aura AQ ${version}`;
  const releaseNotesSha256 = createHash("sha256")
    .update(releaseNotes, "utf8")
    .digest("hex");
  const signatureAlgorithm = "ed25519";
  const privateKey = createPrivateKey(privateKeyPem);

  if (privateKey.asymmetricKeyType !== "ed25519") {
    throw new Error("Installer release signing key must be Ed25519.");
  }
  const derivedKeyId = signingKeyId(createPublicKey(privateKey));
  if (derivedKeyId !== keyId) {
    throw new Error(
      `Installer release key ID mismatch: key derives ${derivedKeyId}, but the key file declares ${keyId}.`,
    );
  }

  const signature = sign(
    null,
    firmwareSignaturePayload({
      signatureSchema: RELEASE_SIGNATURE_SCHEMA,
      productKey,
      version,
      channel,
      title,
      releaseNotesSha256,
      hardwareTarget,
      chipFamily,
      modes,
      compatibility,
      provenance,
      signatureAlgorithm,
      signatureKeyId: keyId,
      assets,
    }),
    privateKey,
  ).toString("base64");

  return {
    schema: RELEASE_PACKAGE_SCHEMA,
    product_key: productKey,
    version,
    channel,
    title,
    hardware_target: hardwareTarget,
    chip_family: chipFamily,
    modes,
    compatibility,
    provenance,
    release_notes_file: "release-notes.md",
    release_notes_sha256: releaseNotesSha256,
    assets: assets.map((asset) => ({
      file_name: asset.fileName,
      asset_kind: asset.assetKind,
      flash_offset: asset.flashOffset,
      modes: asset.modes,
      sha256: asset.sha256,
      size_bytes: asset.sizeBytes,
    })),
    signature: {
      schema: RELEASE_SIGNATURE_SCHEMA,
      algorithm: signatureAlgorithm,
      key_id: keyId,
      value: signature,
    },
  };
}
