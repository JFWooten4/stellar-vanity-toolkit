import test from "node:test";
import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
import { crc16Xmodem, encodeStrKey, decodeStrKey, VERSION_PUBLIC, VERSION_SEED } from "../web/strkey.mjs";

test("CRC16-XModem matches the standard test vector", () => {
  assert.equal(crc16Xmodem(new TextEncoder().encode("123456789")), 0x31c3);
});
test("canonical Stellar zero-byte public key and seed vectors", () => {
  const zeros = new Uint8Array(32);
  assert.equal(encodeStrKey(VERSION_PUBLIC, zeros), "GAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAWHF");
  assert.equal(encodeStrKey(VERSION_SEED, zeros), "SAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABSU2");
});
test("StrKey encoding round trips and checksum rejects corruption", () => {
  const raw = new Uint8Array(32).map((_, i) => i);
  for (const version of [VERSION_PUBLIC, VERSION_SEED]) {
    const encoded = encodeStrKey(version, raw);
    assert.equal(encoded.length, 56);
    assert.deepEqual(decodeStrKey(encoded, version), raw);
    assert.throws(() => decodeStrKey(encoded.slice(0, -1) + (encoded.endsWith("A") ? "B" : "A"), version));
  }
  assert.throws(() => encodeStrKey(VERSION_PUBLIC, new Uint8Array(31)));
  assert.throws(() => decodeStrKey("not-an-address", VERSION_PUBLIC));
});
test("Web Crypto Ed25519 export follows expected RFC 8410 format", async () => {
  const pair = await webcrypto.subtle.generateKey("Ed25519", true, ["sign", "verify"]);
  const publicRaw = new Uint8Array(await webcrypto.subtle.exportKey("raw", pair.publicKey));
  const privateDer = new Uint8Array(await webcrypto.subtle.exportKey("pkcs8", pair.privateKey));
  assert.equal(privateDer.length, 48);
  assert.equal(Buffer.from(privateDer.subarray(0,16)).toString("hex"), "302e020100300506032b657004220420");
  assert.equal(decodeStrKey(encodeStrKey(VERSION_PUBLIC, publicRaw), VERSION_PUBLIC).length, 32);
  assert.equal(decodeStrKey(encodeStrKey(VERSION_SEED, privateDer.slice(16)), VERSION_SEED).length, 32);
  const message = new TextEncoder().encode("vanity-toolkit-test");
  const signature = await webcrypto.subtle.sign("Ed25519", pair.privateKey, message);
  assert.equal(await webcrypto.subtle.verify("Ed25519", pair.publicKey, signature, message), true);
});
