import { encodeStrKey, VERSION_PUBLIC, VERSION_SEED } from "./strkey.mjs";

// RFC 8410 OneAsymmetricKey / PKCS#8 Ed25519 DER header before 32-byte seed.
const PKCS8_PREFIX = [0x30,0x2e,0x02,0x01,0x00,0x30,0x05,0x06,0x03,0x2b,0x65,0x70,0x04,0x22,0x04,0x20];
let started = false;

self.onmessage = async ({ data }) => {
  if (started || data?.type !== "start") return;
  started = true;
  const { prefix, suffix } = data;
  let attempts = 0;
  try {
    if (!self.isSecureContext || !self.crypto?.subtle) throw new Error("Secure-context Web Crypto is required (HTTPS or localhost).");
    for (;;) {
      const pair = await crypto.subtle.generateKey("Ed25519", true, ["sign", "verify"]);
      const publicBytes = new Uint8Array(await crypto.subtle.exportKey("raw", pair.publicKey));
      const address = encodeStrKey(VERSION_PUBLIC, publicBytes);
      attempts++;
      if (address.startsWith("G" + prefix) && address.endsWith(suffix)) {
        const privateDer = new Uint8Array(await crypto.subtle.exportKey("pkcs8", pair.privateKey));
        if (privateDer.length !== 48 || !PKCS8_PREFIX.every((b, i) => privateDer[i] === b)) {
          privateDer.fill(0);
          throw new Error("Unexpected Ed25519 key format. No key was returned.");
        }
        const secret = encodeStrKey(VERSION_SEED, privateDer.slice(16));
        privateDer.fill(0);
        self.postMessage({ type: "match", address, secret, attempts });
        return;
      }
      if (attempts % 100 === 0) self.postMessage({ type: "progress", attempts });
    }
  } catch (error) {
    self.postMessage({ type: "error", message: error instanceof Error ? error.message : "Key generation failed." });
  }
};
