#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define STRKEY_RAW_LEN 35
#define STRKEY_ENCODED_LEN 56

#define VERSION_ACCOUNT_ID ((uint8_t)(6 << 3))   // G
#define VERSION_SEED       ((uint8_t)(18 << 3))  // S

static const char *BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static bool is_base32_string(const char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    char c = (char)toupper((unsigned char)s[i]);

    if (strchr(BASE32_ALPHABET, c) == NULL) {
      return false;
    }
  }

  return true;
}

static void uppercase_in_place(char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    s[i] = (char)toupper((unsigned char)s[i]);
  }
}

static uint16_t crc16_xmodem(const uint8_t *data, size_t len) {
  uint16_t crc = 0x0000;

  for (size_t i = 0; i < len; i++) {
    uint16_t code = (uint16_t)((crc >> 8) & 0xff);
    code ^= data[i] & 0xff;
    code ^= code >> 4;

    crc = (uint16_t)((crc << 8) & 0xffff);
    crc ^= code;
    code = (uint16_t)((code << 5) & 0xffff);
    crc ^= code;
    code = (uint16_t)((code << 7) & 0xffff);
    crc ^= code;
  }

  return crc;
}

static void base32_encode_no_padding(
  const uint8_t *data,
  size_t dataLen,
  char *out
) {
  uint32_t buffer = 0;
  int bitsLeft = 0;
  size_t outPos = 0;

  for (size_t i = 0; i < dataLen; i++) {
    buffer = (buffer << 8) | data[i];
    bitsLeft += 8;

    while (bitsLeft >= 5) {
      int index = (buffer >> (bitsLeft - 5)) & 31;
      out[outPos++] = BASE32_ALPHABET[index];
      bitsLeft -= 5;
    }
  }

  if (bitsLeft > 0) {
    int index = (buffer << (5 - bitsLeft)) & 31;
    out[outPos++] = BASE32_ALPHABET[index];
  }

  out[outPos] = '\0';
}

static void strkey_encode(uint8_t version, const uint8_t raw[32], char out[57]) {
  uint8_t payload[STRKEY_RAW_LEN];

  payload[0] = version;
  memcpy(payload + 1, raw, 32);

  uint16_t checksum = crc16_xmodem(payload, 33);

  // Stellar stores checksum little-endian.
  payload[33] = (uint8_t)(checksum & 0xff);
  payload[34] = (uint8_t)((checksum >> 8) & 0xff);

  base32_encode_no_padding(payload, STRKEY_RAW_LEN, out);
}

static bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool ends_with(const char *s, const char *suffix) {
  size_t sLen = strlen(s);
  size_t suffixLen = strlen(suffix);

  if (suffixLen > sLen) {
    return false;
  }

  return strcmp(s + sLen - suffixLen, suffix) == 0;
}

static void validate_search_span(size_t totalLen) {
  if (totalLen > 7) {
    fprintf(stderr, "Try shorter inputs unless you are ready to wait a very long time.\n");
    exit(1);
  }

  if (totalLen == 5) {
    printf("Be advised: this could take a long time.\n");
  } else if (totalLen == 6) {
    printf("Be advised: this could take hours or days.\n");
  } else if (totalLen == 7) {
    printf("Be advised: this could take days, weeks, or longer.\n");
  }
}

static void usage(const char *program) {
  fprintf(stderr, "Usage: %s <prefix-after-G> <suffix> [--partials]\n", program);
  fprintf(stderr, "Example: %s ABC XYZ --partials\n", program);
  fprintf(stderr, "This searches for public keys like GABC...XYZ\n");
}

int main(int argc, char **argv) {
  if (sodium_init() < 0) {
    fprintf(stderr, "libsodium init failed.\n");
    return 1;
  }

  if (argc < 3 || argc > 4) {
    usage(argv[0]);
    return 1;
  }

  char prefixAfterG[64];
  char suffix[64];

  snprintf(prefixAfterG, sizeof(prefixAfterG), "%s", argv[1]);
  snprintf(suffix, sizeof(suffix), "%s", argv[2]);

  uppercase_in_place(prefixAfterG);
  uppercase_in_place(suffix);

  if (argc == 4 && strcmp(argv[3], "--partials") != 0) {
    usage(argv[0]);
    return 1;
  }

  bool showPartials = argc == 4;

  if (!is_base32_string(prefixAfterG) || !is_base32_string(suffix)) {
    fprintf(stderr, "Try base32 inputs.\n");
    return 1;
  }

  if (
    prefixAfterG[0] != '\0' &&
    prefixAfterG[0] != 'A' &&
    prefixAfterG[0] != 'B' &&
    prefixAfterG[0] != 'C' &&
    prefixAfterG[0] != 'D'
  ) {
    fprintf(stderr, "Try a prefix starting with A/B/C/D after the leading G.\n");
    return 1;
  }

  size_t prefixLen = strlen(prefixAfterG);
  size_t suffixLen = strlen(suffix);

  validate_search_span(prefixLen + suffixLen);

  char fullPrefix[66];
  snprintf(fullPrefix, sizeof(fullPrefix), "G%s", prefixAfterG);

  printf("Searching for: %s...%s\n", fullPrefix, suffix);
  printf("Partial matches: %s\n", showPartials ? "on" : "off");

  uint64_t attempts = 0;
  while (true) {
    uint8_t seed[crypto_sign_SEEDBYTES];
    uint8_t publicKey[crypto_sign_PUBLICKEYBYTES];
    uint8_t secretKey[crypto_sign_SECRETKEYBYTES];

    char publicStrkey[STRKEY_ENCODED_LEN + 1];
    char secretStrkey[STRKEY_ENCODED_LEN + 1];

    randombytes_buf(seed, sizeof(seed));
    crypto_sign_seed_keypair(publicKey, secretKey, seed);

    strkey_encode(VERSION_ACCOUNT_ID, publicKey, publicStrkey);

    attempts++;

    bool prefixMatch = starts_with(publicStrkey, fullPrefix);
    bool suffixMatch = ends_with(publicStrkey, suffix);

    if (prefixMatch && suffixMatch) {
      strkey_encode(VERSION_SEED, seed, secretStrkey);

      printf("\n\nKeypair found:\n");
      printf("Attempts:   %llu\n", (unsigned long long)attempts);
      printf("Public Key: %s\n", publicStrkey);
      printf("Secret Key: %s\n", secretStrkey);

      sodium_memzero(seed, sizeof(seed));
      sodium_memzero(secretKey, sizeof(secretKey));
      sodium_memzero(secretStrkey, sizeof(secretStrkey));

      return 0;
    }

    if (showPartials) {
      if (prefixLen >= suffixLen && prefixMatch) {
        printf("\rPartial Fit: %s...%s", fullPrefix, publicStrkey + STRKEY_ENCODED_LEN - 6);
        fflush(stdout);
      } else if (suffixLen > prefixLen && suffixMatch) {
        printf("\rPartial Fit: %.6s...%s", publicStrkey, suffix);
        fflush(stdout);
      }
    }

    sodium_memzero(seed, sizeof(seed));
    sodium_memzero(secretKey, sizeof(secretKey));
  }
}
