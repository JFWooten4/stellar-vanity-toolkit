// Dependency-free Stellar StrKey (version byte + 32-byte value + CRC16-XModem).
export const ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
export const VERSION_PUBLIC = 0x30;
export const VERSION_SEED = 0x90;

export function crc16Xmodem(bytes) {
  let crc = 0;
  for (const byte of bytes) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) {
      crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
    }
  }
  return crc;
}

export function encodeStrKey(version, raw) {
  if (![VERSION_PUBLIC, VERSION_SEED].includes(version) || !(raw instanceof Uint8Array) || raw.length !== 32) {
    throw new TypeError("Expected a supported Stellar version and exactly 32 bytes");
  }
  const payload = new Uint8Array(35);
  payload[0] = version;
  payload.set(raw, 1);
  const crc = crc16Xmodem(payload.subarray(0, 33));
  payload[33] = crc & 0xff; // Stellar's checksum is little-endian
  payload[34] = crc >>> 8;
  let bits = 0, accumulator = 0, encoded = "";
  for (const byte of payload) {
    accumulator = (accumulator << 8) | byte;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      encoded += ALPHABET[(accumulator >>> bits) & 31];
    }
  }
  return encoded;
}

export function decodeStrKey(encoded, expectedVersion) {
  if (typeof encoded !== "string" || !/^[A-Z2-7]{56}$/.test(encoded)) throw new TypeError("Invalid StrKey");
  const bytes = new Uint8Array(35);
  let bits = 0, accumulator = 0, position = 0;
  for (const char of encoded) {
    accumulator = (accumulator << 5) | ALPHABET.indexOf(char);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      bytes[position++] = (accumulator >>> bits) & 0xff;
    }
  }
  if (bytes[0] !== expectedVersion || (bytes[33] | bytes[34] << 8) !== crc16Xmodem(bytes.subarray(0, 33))) {
    throw new TypeError("Invalid StrKey version or checksum");
  }
  return bytes.slice(1, 33);
}
