import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import { join } from "node:path";

export const PACKAGE_SCHEMA = "aura-firmware-release-package-v1";
export const SIGNATURE_SCHEMA = "aura-firmware-release-signature-v2";
export const FLASH_SIZE_BYTES = 16 * 1024 * 1024;

export const CANONICAL_RELEASE_LAYOUT = Object.freeze({
  "bootloader.bin": Object.freeze({ assetKind: "bootloader", offset: 0x0000 }),
  "partitions.bin": Object.freeze({ assetKind: "partitions", offset: 0x8000 }),
  "boot_app0.bin": Object.freeze({ assetKind: "boot_app0", offset: 0xe000 }),
  "firmware.bin": Object.freeze({ assetKind: "firmware", offset: 0x10000 }),
  "littlefs.bin": Object.freeze({ assetKind: "littlefs", offset: 0xc90000 }),
});

export const HARDWARE_IDENTITIES = Object.freeze({
  project_aura: Object.freeze({
    hardwareTarget: "aura-aq-v1",
    hardwareProfile: "4_3",
    buildIdSuffix: "",
    artifactSlug: "4_3",
    displayName: "Aura AQ 4.3-inch",
  }),
  project_aura_7: Object.freeze({
    hardwareTarget: "aura-aq-7-v1",
    hardwareProfile: "7_dual_i2c_scl6",
    buildIdSuffix: "7-dual-i2c-scl6",
    artifactSlug: "7",
    displayName: "Aura AQ 7-inch",
  }),
});

export function validateHardwareIdentity({
  environment,
  hardwareProfile,
  hardwareTarget,
}) {
  const expected = HARDWARE_IDENTITIES[environment];
  if (
    !expected ||
    expected.hardwareProfile !== hardwareProfile ||
    expected.hardwareTarget !== hardwareTarget
  ) {
    throw new Error(
      `Unsupported hardware identity: environment=${environment} ` +
        `profile=${hardwareProfile} target=${hardwareTarget}`,
    );
  }
  return expected;
}

export function validateGeneratedBuildId({ buildId, commit, environment }) {
  const identity = HARDWARE_IDENTITIES[environment];
  if (!identity || !/^[a-f0-9]{40}$/i.test(commit)) {
    throw new Error("A full source commit and supported environment are required.");
  }
  const expected = `${commit.slice(0, 7).toLowerCase()}${
    identity.buildIdSuffix ? `-${identity.buildIdSuffix}` : ""
  }`;
  if (buildId !== expected) {
    throw new Error(
      `Generated build ID does not match the clean source identity: ` +
        `expected=${expected} actual=${buildId}`,
    );
  }
  return buildId;
}

export function effectiveReleaseVersion(version, buildId) {
  const validated = validateVersion(version);
  if (/^\d+(?:\.\d+)+$/.test(validated)) return validated;
  return validateVersion(`${validated}-${buildId}`);
}

export function stableJson(value) {
  if (value === null || typeof value !== "object") return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(stableJson).join(",")}]`;
  return `{${Object.keys(value)
    .sort()
    .map((key) => `${JSON.stringify(key)}:${stableJson(value[key])}`)
    .join(",")}}`;
}

export function parseOffset(value) {
  if (typeof value === "number" && Number.isSafeInteger(value) && value >= 0) return value;
  if (typeof value !== "string" || !/^(?:0x[0-9a-f]+|\d+)$/i.test(value)) {
    throw new Error(`Invalid flash offset: ${String(value)}`);
  }
  const parsed = value.toLowerCase().startsWith("0x")
    ? Number.parseInt(value.slice(2), 16)
    : Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed < 0) {
    throw new Error(`Invalid flash offset: ${value}`);
  }
  return parsed;
}

export function validateVersion(value) {
  if (
    typeof value !== "string" ||
    !/^\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$/.test(value)
  ) {
    throw new Error(
      `Invalid release version: ${String(value)}. Expected X.Y.Z or a safe prerelease suffix.`,
    );
  }
  return value;
}

export async function describeAsset({ directory, fileName, assetKind, flashOffset, modes }) {
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.test(fileName)) {
    throw new Error(`Unsafe release file name: ${fileName}`);
  }
  const bytes = await readFile(join(directory, fileName));
  if (bytes.byteLength < 1) throw new Error(`Release asset is empty: ${fileName}`);
  validateEsp32S3BinaryHeader(fileName, bytes);
  return {
    file_name: fileName,
    asset_kind: assetKind,
    flash_offset: parseOffset(flashOffset),
    modes: [...modes].sort(),
    sha256: createHash("sha256").update(bytes).digest("hex"),
    size_bytes: bytes.byteLength,
  };
}

export function validateEsp32S3BinaryHeader(fileName, bytes) {
  let valid = true;
  if (["bootloader.bin", "firmware.bin", "recovery.bin"].includes(fileName)) {
    valid = validateEsp32S3Image(bytes);
  } else if (fileName === "partitions.bin") {
    valid = bytes.byteLength >= 2 && bytes[0] === 0xaa && bytes[1] === 0x50;
  } else if (fileName === "boot_app0.bin") {
    valid =
      bytes.byteLength >= 4 &&
      bytes[0] === 0x01 &&
      bytes[1] === 0x00 &&
      bytes[2] === 0x00 &&
      bytes[3] === 0x00;
  }
  if (!valid) throw new Error(`Invalid ESP32-S3 binary header: ${fileName}`);
}

function validateEsp32S3Image(bytes) {
  if (
    bytes.byteLength < 24 ||
    bytes[0] !== 0xe9 ||
    bytes[1] < 1 ||
    bytes[1] > 16 ||
    (bytes[3] >> 4) !== 4 ||
    bytes.readUInt16LE(12) !== 0x0009 ||
    (bytes[23] !== 0 && bytes[23] !== 1)
  ) {
    return false;
  }

  let offset = 24;
  let checksum = 0xef;
  for (let index = 0; index < bytes[1]; index += 1) {
    if (offset + 8 > bytes.byteLength) return false;
    const size = bytes.readUInt32LE(offset + 4);
    offset += 8;
    if (size < 1 || offset + size > bytes.byteLength) return false;
    for (let cursor = offset; cursor < offset + size; cursor += 1) {
      checksum ^= bytes[cursor];
    }
    offset += size;
  }

  const checksumOffset = offset + (15 - (offset % 16));
  if (checksumOffset >= bytes.byteLength || bytes[checksumOffset] !== checksum) {
    return false;
  }
  if (bytes[23] === 1) {
    const digestOffset = checksumOffset + 1;
    if (digestOffset + 32 > bytes.byteLength) return false;
    const expectedDigest = createHash("sha256")
      .update(bytes.subarray(0, digestOffset))
      .digest();
    if (!expectedDigest.equals(bytes.subarray(digestOffset, digestOffset + 32))) {
      return false;
    }
  }
  return true;
}

function canonicalManifestParts(manifest, label) {
  const builds = manifest?.builds;
  if (
    !Array.isArray(builds) ||
    builds.length !== 1 ||
    builds[0]?.chipFamily !== "ESP32-S3" ||
    !Array.isArray(builds[0]?.parts)
  ) {
    throw new Error(`${label} does not contain exactly one ESP32-S3 build.`);
  }
  return builds[0].parts;
}

function assertManifestIdentity(manifest, identity, label) {
  if (
    manifest?.version !== identity.version ||
    manifest?.hardware_target !== identity.hardwareTarget ||
    manifest?.hardware_profile !== identity.hardwareProfile ||
    manifest?.build_id !== identity.buildId
  ) {
    throw new Error(`${label} does not match the selected release identity.`);
  }
}

export function validateCanonicalReleaseManifests({
  fullManifest,
  updateManifest,
  version,
  hardwareTarget,
  hardwareProfile,
  buildId,
}) {
  const identity = { version, hardwareTarget, hardwareProfile, buildId };
  assertManifestIdentity(fullManifest, identity, "manifest.json");
  assertManifestIdentity(updateManifest, identity, "manifest-update.json");

  const fullParts = canonicalManifestParts(fullManifest, "manifest.json");
  const expectedEntries = Object.entries(CANONICAL_RELEASE_LAYOUT);
  if (fullParts.length !== expectedEntries.length) {
    throw new Error("manifest.json does not contain the canonical full layout.");
  }
  for (const [fileName, layout] of expectedEntries) {
    const matches = fullParts.filter((part) => part?.path === fileName);
    if (matches.length !== 1 || parseOffset(matches[0].offset) !== layout.offset) {
      throw new Error(`manifest.json has a non-canonical ${fileName} entry.`);
    }
  }

  const updateParts = canonicalManifestParts(updateManifest, "manifest-update.json");
  if (
    updateParts.length !== 1 ||
    updateParts[0]?.path !== "firmware.bin" ||
    parseOffset(updateParts[0].offset) !== CANONICAL_RELEASE_LAYOUT["firmware.bin"].offset
  ) {
    throw new Error("manifest-update.json does not contain the canonical Update layout.");
  }

  return {
    fullParts,
    updateParts,
  };
}

export function validateReleaseArtifactStamp({
  stamp,
  environment,
  commit,
  buildId,
  hardwareProfile,
  hardwareTarget,
  assets,
}) {
  if (
    stamp?.schema !== "project-aura.release-artifacts.v1" ||
    stamp?.environment !== environment ||
    stamp?.source_commit !== commit.toLowerCase() ||
    stamp?.build_id !== buildId ||
    stamp?.hardware_profile !== hardwareProfile ||
    stamp?.hardware_target !== hardwareTarget ||
    !Array.isArray(stamp?.files) ||
    stamp.files.length !== assets.length
  ) {
    throw new Error("Post-build artifact stamp does not match the selected release identity.");
  }

  const records = new Map();
  for (const record of stamp.files) {
    if (
      typeof record?.file_name !== "string" ||
      records.has(record.file_name) ||
      !Number.isSafeInteger(record?.size_bytes) ||
      record.size_bytes < 1 ||
      typeof record?.sha256 !== "string" ||
      !/^[a-f0-9]{64}$/.test(record.sha256)
    ) {
      throw new Error("Post-build artifact stamp has an invalid or duplicate file record.");
    }
    records.set(record.file_name, record);
  }
  for (const asset of assets) {
    const record = records.get(asset.file_name);
    if (
      !record ||
      record.size_bytes !== asset.size_bytes ||
      record.sha256 !== asset.sha256
    ) {
      throw new Error(`Post-build artifact stamp mismatch: ${asset.file_name}`);
    }
  }
  return stamp;
}

export function signaturePayload(release) {
  if (release.signature?.schema !== SIGNATURE_SCHEMA) {
    throw new Error(`Unsupported signature schema: ${release.signature?.schema}`);
  }
  return {
    schema: release.signature.schema,
    product_key: release.product_key,
    version: release.version,
    channel: release.channel,
    title: release.title,
    release_notes_sha256: release.release_notes_sha256,
    hardware_target: release.hardware_target,
    chip_family: release.chip_family,
    modes: [...release.modes].sort(),
    compatibility: release.compatibility,
    provenance: release.provenance,
    signature: {
      algorithm: release.signature.algorithm,
      key_id: release.signature.key_id,
    },
    assets: release.assets
      .map((asset) => ({
        file_name: asset.file_name,
        asset_kind: asset.asset_kind,
        flash_offset: asset.flash_offset,
        modes: [...asset.modes].sort(),
        sha256: asset.sha256,
        size_bytes: asset.size_bytes,
      }))
      .sort(
        (left, right) =>
          left.flash_offset - right.flash_offset ||
          left.file_name.localeCompare(right.file_name),
      ),
  };
}

export function signatureBytes(release) {
  return Buffer.from(stableJson(signaturePayload(release)), "utf8");
}

export function validateFlashLayout(assets, modes, flashSize = FLASH_SIZE_BYTES) {
  for (const mode of modes) {
    const ranges = assets
      .filter((asset) => asset.modes.includes(mode))
      .map((asset) => ({
        name: asset.file_name,
        start: asset.flash_offset,
        end: asset.flash_offset + asset.size_bytes,
      }))
      .sort((left, right) => left.start - right.start);

    if (ranges.length === 0) throw new Error(`No binary parts are assigned to ${mode}.`);
    for (let index = 0; index < ranges.length; index += 1) {
      const range = ranges[index];
      if (range.start < 0 || range.end > flashSize) {
        throw new Error(`${range.name} does not fit the 16 MB flash layout.`);
      }
      if (index > 0 && range.start < ranges[index - 1].end) {
        throw new Error(`${range.name} overlaps ${ranges[index - 1].name} in ${mode} mode.`);
      }
    }
  }
}
