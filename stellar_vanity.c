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
  size_t data_len,
  char *out
) {
  uint32_t buffer = 0;
  int bits_left = 0;
  size_t out_pos = 0;

  for (size_t i = 0; i < data_len; i++) {
    buffer = (buffer << 8) | data[i];
    bits_left += 8;

    while (bits_left >= 5) {
      int index = (buffer >> (bits_left - 5)) & 31;
      out[out_pos++] = BASE32_ALPHABET[index];
      bits_left -= 5;
    }
  }

  if (bits_left > 0) {
    int index = (buffer << (5 - bits_left)) & 31;
    out[out_pos++] = BASE32_ALPHABET[index];
  }

  out[out_pos] = '\0';
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
  size_t s_len = strlen(s);
  size_t suffix_len = strlen(suffix);

  if (suffix_len > s_len) {
    return false;
  }

  return strcmp(s + s_len - suffix_len, suffix) == 0;
}

static void validate_search_span(size_t total_len) {
  if (total_len > 7) {
    fprintf(stderr, "Try shorter inputs unless you are ready to wait a very long time.\n");
    exit(1);
  }

  if (total_len == 5) {
    printf("Be advised: this could take a long time.\n");
  } else if (total_len == 6) {
    printf("Be advised: this could take hours or days.\n");
  } else if (total_len == 7) {
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

  char prefix_after_g[64];
  char suffix[64];

  snprintf(prefix_after_g, sizeof(prefix_after_g), "%s", argv[1]);
  snprintf(suffix, sizeof(suffix), "%s", argv[2]);

  uppercase_in_place(prefix_after_g);
  uppercase_in_place(suffix);

  bool show_partials = argc == 4 && strcmp(argv[3], "--partials") == 0;

  if (!is_base32_string(prefix_after_g) || !is_base32_string(suffix)) {
    fprintf(stderr, "Try base32 inputs.\n");
    return 1;
  }

  if (
    prefix_after_g[0] != '\0' &&
    prefix_after_g[0] != 'A' &&
    prefix_after_g[0] != 'B' &&
    prefix_after_g[0] != 'C' &&
    prefix_after_g[0] != 'D'
  ) {
    fprintf(stderr, "Try a prefix starting with A/B/C/D after the leading G.\n");
    return 1;
  }

  size_t prefix_len = strlen(prefix_after_g);
  size_t suffix_len = strlen(suffix);

  validate_search_span(prefix_len + suffix_len);

  char full_prefix[66];
  snprintf(full_prefix, sizeof(full_prefix), "G%s", prefix_after_g);

  printf("Searching for: %s...%s\n", full_prefix, suffix);
  printf("Partial matches: %s\n", show_partials ? "on" : "off");

  uint64_t attempts = 0;
  while (true) {
    uint8_t seed[crypto_sign_SEEDBYTES];
    uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_sign_SECRETKEYBYTES];

    char public_strkey[STRKEY_ENCODED_LEN + 1];
    char secret_strkey[STRKEY_ENCODED_LEN + 1];

    randombytes_buf(seed, sizeof(seed));
    crypto_sign_seed_keypair(public_key, secret_key, seed);

    strkey_encode(VERSION_ACCOUNT_ID, public_key, public_strkey);

    attempts++;

    bool prefix_match = starts_with(public_strkey, full_prefix);
    bool suffix_match = ends_with(public_strkey, suffix);

    if (prefix_match && suffix_match) {
      strkey_encode(VERSION_SEED, seed, secret_strkey);

      printf("\n\nKeypair found:\n");
      printf("Attempts:   %llu\n", (unsigned long long)attempts);
      printf("Public Key: %s\n", public_strkey);
      printf("Secret Key: %s\n", secret_strkey);

      sodium_memzero(seed, sizeof(seed));
      sodium_memzero(secret_key, sizeof(secret_key));
      sodium_memzero(secret_strkey, sizeof(secret_strkey));

      return 0;
    }

    if (show_partials) {
      if (prefix_len >= suffix_len && prefix_match) {
        printf("\rPartial Fit: %s...%s", full_prefix, public_strkey + STRKEY_ENCODED_LEN - 6);
        fflush(stdout);
      } else if (suffix_len > prefix_len && suffix_match) {
        printf("\rPartial Fit: %.6s...%s", public_strkey, suffix);
        fflush(stdout);
      }
    }

    sodium_memzero(seed, sizeof(seed));
    sodium_memzero(secret_key, sizeof(secret_key));
  }
}
