import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import { join } from "node:path";

export const PACKAGE_SCHEMA = "aura-firmware-release-package-v1";
export const SIGNATURE_SCHEMA = "aura-firmware-release-signature-v1";
export const FLASH_SIZE_BYTES = 16 * 1024 * 1024;

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
  if (typeof value !== "string" || !/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(value)) {
    throw new Error(`Invalid release version: ${String(value)}. Expected X.Y.Z.`);
  }
  return value;
}

export async function describeAsset({ directory, fileName, assetKind, flashOffset, modes }) {
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.test(fileName)) {
    throw new Error(`Unsafe release file name: ${fileName}`);
  }
  const bytes = await readFile(join(directory, fileName));
  return {
    file_name: fileName,
    asset_kind: assetKind,
    flash_offset: parseOffset(flashOffset),
    modes: [...modes].sort(),
    sha256: createHash("sha256").update(bytes).digest("hex"),
    size_bytes: bytes.byteLength,
  };
}

export function signaturePayload(release) {
  return {
    schema: SIGNATURE_SCHEMA,
    product_key: release.product_key,
    version: release.version,
    channel: release.channel,
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
