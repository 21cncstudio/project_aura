import { createHash, createPrivateKey, createPublicKey, sign } from "node:crypto";
import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import {
  CANONICAL_RELEASE_LAYOUT,
  FLASH_SIZE_BYTES,
  PACKAGE_SCHEMA,
  SIGNATURE_SCHEMA,
  describeAsset,
  effectiveReleaseVersion,
  signatureBytes,
  validateCanonicalReleaseManifests,
  validateGeneratedBuildId,
  validateFlashLayout,
  validateHardwareIdentity,
  validateReleaseArtifactStamp,
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
const requestedVersion = validateVersion(required("--version"));
const channel = required("--channel").toLowerCase();
const commit = required("--commit");
const environment = required("--environment");
const hardwareProfile = required("--hardware-profile");
const hardwareTarget = required("--hardware-target");
const buildId = required("--build-id");
const keyId = required("--key-id");
const privateKeyPath = resolve(required("--private-key"));
const notesPath = resolve(required("--notes"));

if (!["stable", "beta"].includes(channel)) {
  if (channel === "recovery") {
    throw new Error(
      "Recovery packaging is disabled until it has a dedicated artifact-bound build pipeline.",
    );
  }
  throw new Error(`Unsupported release channel: ${channel}`);
}
if (channel === "stable" && requestedVersion.includes("-")) {
  throw new Error("Stable releases require an exact X.Y.Z version without a prerelease suffix.");
}
if (channel === "beta" && !requestedVersion.includes("-")) {
  throw new Error("Beta releases require an explicit prerelease suffix, for example X.Y.Z-beta.");
}
if (!/^[a-f0-9]{40}$/i.test(commit)) throw new Error("Source commit must be a full Git SHA.");
const hardwareIdentity = validateHardwareIdentity({
  environment,
  hardwareProfile,
  hardwareTarget,
});
validateGeneratedBuildId({ buildId, commit, environment });
const version = effectiveReleaseVersion(requestedVersion, buildId);
const privateKey = createPrivateKey(await readFile(privateKeyPath, "utf8"));
if (privateKey.asymmetricKeyType !== "ed25519") {
  throw new Error("The installer release key is not Ed25519.");
}
const derivedKeyId = `aura-installer-ed25519-${createHash("sha256")
  .update(createPublicKey(privateKey).export({ type: "spki", format: "der" }))
  .digest("hex")
  .slice(0, 16)}`;
if (keyId !== derivedKeyId) {
  throw new Error("Installer key ID does not match the supplied private key.");
}

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
  const { fullParts, updateParts } = validateCanonicalReleaseManifests({
    fullManifest: manifestFull,
    updateManifest: manifestUpdate,
    version,
    hardwareTarget,
    hardwareProfile,
    buildId,
  });
  updateNames = new Set(updateParts.map((part) => String(part.path)));
  sourceParts = fullParts.map((part) => ({
    fileName: String(part.path),
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
      assetKind:
        channel === "recovery"
          ? "recovery"
          : CANONICAL_RELEASE_LAYOUT[part.fileName].assetKind,
      flashOffset: part.flashOffset,
      modes: assetModes,
    }),
  );
}
validateFlashLayout(assets, modes);
const artifactStamp = JSON.parse(
  await readFile(join(sourceDir, "release-artifacts.json"), "utf8"),
);
validateReleaseArtifactStamp({
  stamp: artifactStamp,
  environment,
  commit,
  buildId,
  hardwareProfile,
  hardwareTarget,
  assets,
});
const releaseNotes = await readFile(notesPath);
if (releaseNotes.byteLength === 0) throw new Error("Release notes must not be empty.");
const releaseNotesSha256 = createHash("sha256").update(releaseNotes).digest("hex");

const release = {
  schema: PACKAGE_SCHEMA,
  product_key: "aura-aq",
  version,
  channel,
  title: `${hardwareIdentity.displayName} ${version}`,
  release_notes_file: "release-notes.md",
  release_notes_sha256: releaseNotesSha256,
  hardware_target: hardwareTarget,
  chip_family: "ESP32-S3",
  modes,
  compatibility: {
    hardware_revision: "v1",
    hardware_profile: hardwareProfile,
    flash_size_bytes: FLASH_SIZE_BYTES,
  },
  provenance: {
    build_id: buildId,
    commit,
    environment,
    hardware_profile: hardwareProfile,
    hardware_target: hardwareTarget,
  },
  signature: {
    schema: SIGNATURE_SCHEMA,
    algorithm: "ed25519",
    key_id: keyId,
    value: "",
  },
  assets,
};

release.signature.value = sign(null, signatureBytes(release), privateKey).toString("base64");

await mkdir(stagingDir, { recursive: false });
for (const asset of assets) {
  await copyFile(join(sourceDir, asset.file_name), join(stagingDir, asset.file_name));
}
await copyFile(notesPath, join(stagingDir, "release-notes.md"));
await writeFile(join(stagingDir, "release.json"), `${JSON.stringify(release, null, 2)}\n`, "utf8");

process.stdout.write(JSON.stringify({ stagingDir, release }, null, 2));
