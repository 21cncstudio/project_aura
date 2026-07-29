import { mkdir, readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import {
  createSignedReleaseDocument,
  readReleaseAssets,
  validateReleaseBinaryHeaders,
  validateReleaseInputs,
} from "./lib/installer_release_package.mjs";

function argumentsMap(values) {
  const result = new Map();
  for (let index = 0; index < values.length; index += 2) {
    const name = values[index];
    const value = values[index + 1];
    if (!name?.startsWith("--") || value === undefined) {
      throw new Error(`Invalid argument near ${name ?? "end of command"}.`);
    }
    result.set(name.slice(2), value);
  }
  return result;
}

function required(map, name) {
  const value = map.get(name)?.trim();
  if (!value) throw new Error(`Missing --${name}.`);
  return value;
}

const args = argumentsMap(process.argv.slice(2));
const sourceDirectory = resolve(required(args, "source"));
const stagingDirectory = resolve(required(args, "staging"));
const notesFile = resolve(required(args, "notes"));
const version = required(args, "version");
const channel = required(args, "channel");
const commit = required(args, "commit");
const buildId = required(args, "build-id");
const keyId = required(args, "key-id");
const configuredVersion = required(args, "configured-version");
const workingTreeClean = required(args, "working-tree-clean") === "true";
const privateKeyPem = process.env.AURA_INSTALLER_RELEASE_PRIVATE_KEY_PEM?.trim();

if (!privateKeyPem) {
  throw new Error("The DPAPI-unwrapped installer release key was not supplied.");
}

validateReleaseInputs({
  version,
  configuredVersion,
  channel,
  commit,
  workingTreeClean,
});

const assets = await readReleaseAssets(sourceDirectory, channel);
validateReleaseBinaryHeaders(assets, channel);
const releaseNotes = await readFile(notesFile, "utf8");
if (!releaseNotes.trim()) throw new Error("Release notes cannot be empty.");

const release = createSignedReleaseDocument({
  version,
  channel,
  commit,
  buildId,
  keyId,
  privateKeyPem,
  assets,
  releaseNotes,
});

await mkdir(stagingDirectory, { recursive: true });
await writeFile(
  resolve(stagingDirectory, "release.json"),
  `${JSON.stringify(release, null, 2)}\n`,
  "utf8",
);
await writeFile(
  resolve(stagingDirectory, "release-notes.md"),
  releaseNotes,
  "utf8",
);

for (const asset of assets) {
  await writeFile(resolve(stagingDirectory, asset.fileName), asset.bytes);
}

process.stdout.write(JSON.stringify({
  version,
  channel,
  key_id: keyId,
  commit,
  files: assets.length + 2,
  binary_bytes: assets.reduce((sum, asset) => sum + asset.sizeBytes, 0),
}));
