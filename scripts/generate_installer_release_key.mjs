import { generateKeyPairSync } from "node:crypto";
import { signingKeyId } from "./lib/installer_release_package.mjs";

const { privateKey, publicKey } = generateKeyPairSync("ed25519");
const privateKeyPem = privateKey.export({ format: "pem", type: "pkcs8" });
const publicKeyPem = publicKey.export({ format: "pem", type: "spki" });

process.stdout.write(JSON.stringify({
  schema: "aura-installer-release-key-v1",
  key_id: signingKeyId(publicKeyPem),
  private_key_pem: privateKeyPem,
  public_key_pem: publicKeyPem,
}));
