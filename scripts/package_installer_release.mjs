import { createPrivateKey, sign } from "node:crypto";
import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { basename, join, resolve } from "node:path";
import {
  FLASH_SIZE_BYTES,
  PACKAGE_SCHEMA,
  describeAsset,
  signatureBytes,
  validateFlashLayout,
  validateVersion,
} from "./lib/installer-release.mjs";

const args = new Map();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

function required(name) {
  const value = args.get(name)?.trim();
  if (!value) throw new Error(`Missing ${name}`);
  return value;
}

const sourceDir = resolve(required("--source"));
const stagingDir = resolve(required("--staging"));
const version = validateVersion(required("--version"));
const channel = required("--channel").toLowerCase();
const commit = required("--commit");
const keyId = required("--key-id");
const privateKeyPath = resolve(required("--private-key"));
const notesPath = args.get("--notes") ? resolve(args.get("--notes")) : null;

if (!["stable", "beta", "recovery"].includes(channel)) {
  throw new Error(`Unsupported release channel: ${channel}`);
}
if (!/^[a-f0-9]{40}$/i.test(commit)) throw new Error("Source commit must be a full Git SHA.");

let sourceParts;
let updateNames;
if (channel === "recovery") {
  sourceParts = [{ fileName: "recovery.bin", flashOffset: 0 }];
  updateNames = new Set();
} else {
  const manifestFull = JSON.parse(await readFile(join(sourceDir, "manifest.json"), "utf8"));
  const manifestUpdate = JSON.parse(
    await readFile(join(sourceDir, "manifest-update.json"), "utf8"),
  );
  const fullParts = manifestFull?.builds?.[0]?.parts;
  const updateParts = manifestUpdate?.builds?.[0]?.parts;
  if (!Array.isArray(fullParts) || fullParts.length === 0) {
    throw new Error("manifest.json does not contain a valid ESP32-S3 build.");
  }
  if (!Array.isArray(updateParts) || updateParts.length === 0) {
    throw new Error("manifest-update.json does not contain an Update build.");
  }
  updateNames = new Set(updateParts.map((part) => basename(String(part.path))));
  sourceParts = fullParts.map((part) => ({
    fileName: basename(String(part.path)),
    flashOffset: part.offset,
  }));
}
const expectedNames =
  channel === "recovery"
    ? ["recovery.bin"]
    : ["bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin", "littlefs.bin"];
if (
  sourceParts.length !== expectedNames.length ||
  expectedNames.some((name) => !sourceParts.some((part) => part.fileName === name))
) {
  throw new Error(`The package must contain exactly: ${expectedNames.join(", ")}`);
}

const modes = channel === "recovery" ? ["recovery"] : ["full", "update"];
const assets = [];
for (const part of sourceParts) {
  const assetModes =
    channel === "recovery"
      ? ["recovery"]
      : ["full", ...(updateNames.has(part.fileName) ? ["update"] : [])];
  assets.push(
    await describeAsset({
      directory: sourceDir,
      fileName: part.fileName,
      assetKind: part.fileName.replace(/\.bin$/i, "").replaceAll("-", "_"),
      flashOffset: part.flashOffset,
      modes: assetModes,
    }),
  );
}
validateFlashLayout(assets, modes);

const release = {
  schema: PACKAGE_SCHEMA,
  product_key: "aura-aq",
  version,
  channel,
  title: `Aura AQ ${version}`,
  release_notes_file: notesPath ? "release-notes.md" : null,
  hardware_target: "aura-aq-v1",
  chip_family: "ESP32-S3",
  modes,
  compatibility: {
    hardware_revision: "v1",
    flash_size_bytes: FLASH_SIZE_BYTES,
  },
  provenance: {
    build_id: commit.slice(0, 12),
    commit,
  },
  signature: {
    algorithm: "ed25519",
    key_id: keyId,
    value: "",
  },
  assets,
};

const privateKey = createPrivateKey(await readFile(privateKeyPath, "utf8"));
if (privateKey.asymmetricKeyType !== "ed25519") {
  throw new Error("The installer release key is not Ed25519.");
}
release.signature.value = sign(null, signatureBytes(release), privateKey).toString("base64");

await mkdir(stagingDir, { recursive: false });
for (const asset of assets) {
  await copyFile(join(sourceDir, asset.file_name), join(stagingDir, asset.file_name));
}
if (notesPath) await copyFile(notesPath, join(stagingDir, "release-notes.md"));
await writeFile(join(stagingDir, "release.json"), `${JSON.stringify(release, null, 2)}\n`, "utf8");

process.stdout.write(JSON.stringify({ stagingDir, release }, null, 2));
