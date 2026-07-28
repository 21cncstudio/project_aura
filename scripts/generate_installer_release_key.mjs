import { generateKeyPairSync } from "node:crypto";
import { writeFile } from "node:fs/promises";

const args = new Map();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

const privateOut = args.get("--private-out");
const publicOut = args.get("--public-out");
if (!privateOut || !publicOut) {
  throw new Error("Usage: node scripts/generate_installer_release_key.mjs --private-out PATH --public-out PATH");
}

const { privateKey, publicKey } = generateKeyPairSync("ed25519", {
  privateKeyEncoding: { type: "pkcs8", format: "pem" },
  publicKeyEncoding: { type: "spki", format: "pem" },
});

await Promise.all([
  writeFile(privateOut, privateKey, { encoding: "utf8", mode: 0o600, flag: "wx" }),
  writeFile(publicOut, publicKey, { encoding: "utf8", mode: 0o644, flag: "wx" }),
]);

