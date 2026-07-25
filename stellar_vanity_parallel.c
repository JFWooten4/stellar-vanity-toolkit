#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define STRKEY_RAW_LEN 35
#define STRKEY_ENCODED_LEN 56
#define MAX_THREADS 64

#define VERSION_ACCOUNT_ID ((uint8_t)(6 << 3))
#define VERSION_SEED       ((uint8_t)(18 << 3))

static const char *BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

typedef struct {
  char full_prefix[66];
  char suffix[64];
  size_t prefix_len;
  size_t suffix_len;
#ifdef _WIN32
  volatile LONG found;
#else
  pthread_mutex_t found_mutex;
  int found;
#endif
  char found_public[STRKEY_ENCODED_LEN + 1];
  char found_secret[STRKEY_ENCODED_LEN + 1];
} SearchContext;

typedef struct {
  SearchContext *ctx;
  uint64_t attempts;
} WorkerArgs;

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

  payload[33] = (uint8_t)(checksum & 0xff);
  payload[34] = (uint8_t)((checksum >> 8) & 0xff);

  base32_encode_no_padding(payload, STRKEY_RAW_LEN, out);
}

static bool matches_search(const SearchContext *ctx, const char *public_strkey) {
  if (memcmp(public_strkey, ctx->full_prefix, ctx->prefix_len) != 0) {
    return false;
  }

  if (ctx->suffix_len == 0) {
    return true;
  }

  return memcmp(
    public_strkey + STRKEY_ENCODED_LEN - ctx->suffix_len,
    ctx->suffix,
    ctx->suffix_len
  ) == 0;
}

static void validate_search_span(size_t total_len) {
  if (total_len > 10) {
    fprintf(stderr, "Try shorter inputs. Each extra character makes the search 32x harder.\n");
    exit(1);
  }

  if (total_len == 5) {
    printf("Be advised: this could take a long time.\n");
  } else if (total_len == 6) {
    printf("Be advised: this could take hours or days.\n");
  } else if (total_len == 7) {
    printf("Be advised: this could take days, weeks, or longer.\n");
  } else if (total_len >= 8) {
    printf("Be advised: this is still a very large search even with threads.\n");
  }
}

#ifdef _WIN32
static bool search_is_found(SearchContext *ctx) {
  return InterlockedCompareExchange(&ctx->found, 0, 0) != 0;
}

static bool claim_found(SearchContext *ctx) {
  return InterlockedCompareExchange(&ctx->found, 1, 0) == 0;
}

static void mark_found(SearchContext *ctx) {
  InterlockedExchange(&ctx->found, 1);
}

static unsigned int __stdcall search_worker(void *arg) {
#else
static bool search_is_found(SearchContext *ctx) {
  bool found;

  pthread_mutex_lock(&ctx->found_mutex);
  found = ctx->found != 0;
  pthread_mutex_unlock(&ctx->found_mutex);

  return found;
}

static bool claim_found(SearchContext *ctx) {
  bool claimed = false;

  pthread_mutex_lock(&ctx->found_mutex);
  if (!ctx->found) {
    ctx->found = 1;
    claimed = true;
  }
  pthread_mutex_unlock(&ctx->found_mutex);

  return claimed;
}

static void mark_found(SearchContext *ctx) {
  pthread_mutex_lock(&ctx->found_mutex);
  ctx->found = 1;
  pthread_mutex_unlock(&ctx->found_mutex);
}

static void *search_worker(void *arg) {
#endif
  WorkerArgs *worker = (WorkerArgs *)arg;
  SearchContext *ctx = worker->ctx;

  while (!search_is_found(ctx)) {
    uint8_t seed[crypto_sign_SEEDBYTES];
    uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
    uint8_t secret_key[crypto_sign_SECRETKEYBYTES];
    char public_strkey[STRKEY_ENCODED_LEN + 1];
    char secret_strkey[STRKEY_ENCODED_LEN + 1];

    randombytes_buf(seed, sizeof(seed));
    crypto_sign_seed_keypair(public_key, secret_key, seed);
    strkey_encode(VERSION_ACCOUNT_ID, public_key, public_strkey);
    worker->attempts++;

    if (matches_search(ctx, public_strkey)) {
      if (claim_found(ctx)) {
        strkey_encode(VERSION_SEED, seed, secret_strkey);
        memcpy(ctx->found_public, public_strkey, sizeof(ctx->found_public));
        memcpy(ctx->found_secret, secret_strkey, sizeof(ctx->found_secret));
        sodium_memzero(secret_strkey, sizeof(secret_strkey));
      }

      sodium_memzero(seed, sizeof(seed));
      sodium_memzero(secret_key, sizeof(secret_key));
#ifdef _WIN32
      return 0;
#else
      return NULL;
#endif
    }
  }

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

static unsigned int default_thread_count(void) {
#ifdef _WIN32
  SYSTEM_INFO info;
  GetSystemInfo(&info);

  if (info.dwNumberOfProcessors == 0) {
    return 1;
  }

  return info.dwNumberOfProcessors > MAX_THREADS
    ? MAX_THREADS
    : info.dwNumberOfProcessors;
#else
  long processors = sysconf(_SC_NPROCESSORS_ONLN);

  if (processors <= 0) {
    return 1;
  }

  return processors > MAX_THREADS
    ? MAX_THREADS
    : (unsigned int)processors;
#endif
}

static unsigned int parse_thread_count(const char *value) {
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);

  if (end == value || *end != '\0' || parsed == 0 || parsed > MAX_THREADS) {
    fprintf(stderr, "Thread count must be between 1 and %d.\n", MAX_THREADS);
    exit(1);
  }

  return (unsigned int)parsed;
}

static void usage(const char *program) {
  fprintf(stderr, "Usage: %s <prefix-after-G> <suffix> [threads]\n", program);
  fprintf(stderr, "Example: %s ABC XYZ 8\n", program);
  fprintf(stderr, "This searches for public keys like GABC...XYZ.\n");
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

  unsigned int thread_count = argc == 4
    ? parse_thread_count(argv[3])
    : default_thread_count();

  size_t prefix_after_g_len = strlen(prefix_after_g);
  size_t suffix_len = strlen(suffix);

  validate_search_span(prefix_after_g_len + suffix_len);

  SearchContext ctx;
  memset(&ctx, 0, sizeof(ctx));
#ifndef _WIN32
  if (pthread_mutex_init(&ctx.found_mutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize thread lock.\n");
    return 1;
  }
#endif
  snprintf(ctx.full_prefix, sizeof(ctx.full_prefix), "G%s", prefix_after_g);
  snprintf(ctx.suffix, sizeof(ctx.suffix), "%s", suffix);
  ctx.prefix_len = strlen(ctx.full_prefix);
  ctx.suffix_len = suffix_len;

  printf("Searching for: %s...%s\n", ctx.full_prefix, ctx.suffix);
  printf("Threads: %u\n", thread_count);

  WorkerArgs workers[MAX_THREADS];

#ifdef _WIN32
  HANDLE handles[MAX_THREADS];

  for (unsigned int i = 0; i < thread_count; i++) {
    workers[i].ctx = &ctx;
    workers[i].attempts = 0;
    handles[i] = (HANDLE)_beginthreadex(
      NULL,
      0,
      search_worker,
      &workers[i],
      0,
      NULL
    );

    if (handles[i] == NULL) {
      fprintf(stderr, "Failed to start worker thread.\n");
      mark_found(&ctx);

      for (unsigned int j = 0; j < i; j++) {
        WaitForSingleObject(handles[j], INFINITE);
        CloseHandle(handles[j]);
      }

      return 1;
    }
  }

  WaitForMultipleObjects(thread_count, handles, TRUE, INFINITE);

  uint64_t total_attempts = 0;
  for (unsigned int i = 0; i < thread_count; i++) {
    total_attempts += workers[i].attempts;
    CloseHandle(handles[i]);
  }
#else
  pthread_t handles[MAX_THREADS];

  for (unsigned int i = 0; i < thread_count; i++) {
    workers[i].ctx = &ctx;
    workers[i].attempts = 0;

    if (pthread_create(&handles[i], NULL, search_worker, &workers[i]) != 0) {
      fprintf(stderr, "Failed to start worker thread.\n");
      mark_found(&ctx);

      for (unsigned int j = 0; j < i; j++) {
        pthread_join(handles[j], NULL);
      }

      pthread_mutex_destroy(&ctx.found_mutex);
      return 1;
    }
  }

  uint64_t total_attempts = 0;
  for (unsigned int i = 0; i < thread_count; i++) {
    pthread_join(handles[i], NULL);
    total_attempts += workers[i].attempts;
  }
#endif

  if (ctx.found) {
    printf("\nKeypair found:\n");
    printf("Attempts:   %llu\n", (unsigned long long)total_attempts);
    printf("Public Key: %s\n", ctx.found_public);
    printf("Secret Key: %s\n", ctx.found_secret);

    sodium_memzero(ctx.found_secret, sizeof(ctx.found_secret));
#ifndef _WIN32
    pthread_mutex_destroy(&ctx.found_mutex);
#endif
    return 0;
  }

  fprintf(stderr, "Search stopped without a result.\n");
#ifndef _WIN32
  pthread_mutex_destroy(&ctx.found_mutex);
#endif
  return 1;
}
