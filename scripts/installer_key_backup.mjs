import { readFile, writeFile } from "node:fs/promises";
import {
  createInstallerKeyBackup,
  parseJsonWithOptionalBom,
  restoreInstallerKeyBackup,
} from "./lib/installer-key-backup.mjs";

const args = new Map();
for (let index = 3; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

const command = process.argv[2];
const passphrase = process.env.AURA_INSTALLER_BACKUP_PASSPHRASE;
if (!passphrase) {
  throw new Error("AURA_INSTALLER_BACKUP_PASSPHRASE is required.");
}

if (command === "create") {
  const privateKeyPath = args.get("--private-key");
  const publicKeyPath = args.get("--public-key");
  const metadataPath = args.get("--metadata");
  const outputPath = args.get("--output");
  if (!privateKeyPath || !publicKeyPath || !metadataPath || !outputPath) {
    throw new Error("Usage: installer_key_backup.mjs create --private-key PATH --public-key PATH --metadata PATH --output PATH");
  }
  const [privateKeyPem, publicKeyPem, metadataJson] = await Promise.all([
    readFile(privateKeyPath),
    readFile(publicKeyPath, "utf8"),
    readFile(metadataPath, "utf8"),
  ]);
  const backup = await createInstallerKeyBackup({
    privateKeyPem,
    publicKeyPem,
    metadata: parseJsonWithOptionalBom(metadataJson),
    passphrase,
  });
  await writeFile(outputPath, `${JSON.stringify(backup, null, 2)}\n`, {
    encoding: "utf8",
    flag: "wx",
  });
  process.stdout.write(`${backup.key_id}\n`);
} else if (command === "restore") {
  const backupPath = args.get("--backup");
  const privateOut = args.get("--private-out");
  const publicOut = args.get("--public-out");
  const metadataOut = args.get("--metadata-out");
  if (!backupPath || !privateOut || !publicOut || !metadataOut) {
    throw new Error("Usage: installer_key_backup.mjs restore --backup PATH --private-out PATH --public-out PATH --metadata-out PATH");
  }
  const backup = parseJsonWithOptionalBom(await readFile(backupPath, "utf8"));
  const restored = await restoreInstallerKeyBackup({ backup, passphrase });
  await Promise.all([
    writeFile(privateOut, restored.privateKeyPem, { flag: "wx", mode: 0o600 }),
    writeFile(publicOut, restored.publicKeyPem, { flag: "wx", mode: 0o644 }),
    writeFile(metadataOut, `${JSON.stringify(restored.metadata, null, 2)}\n`, {
      encoding: "utf8",
      flag: "wx",
    }),
  ]);
  process.stdout.write(`${restored.metadata.key_id}\n`);
} else {
  throw new Error("Usage: installer_key_backup.mjs <create|restore> ...");
}
