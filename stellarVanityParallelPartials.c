#define STELLAR_VANITY_PARTIALS

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
  char fullPrefix[66];
  char suffix[64];
  size_t prefixLen;
  size_t suffixLen;
#ifdef _WIN32
  volatile LONG found;
#else
  pthread_mutex_t foundMutex;
  int found;
#endif
#ifdef STELLAR_VANITY_PARTIALS
#ifdef _WIN32
  CRITICAL_SECTION outputLock;
#else
  pthread_mutex_t outputMutex;
#endif
#endif
  char foundPublic[STRKEY_ENCODED_LEN + 1];
  char foundSecret[STRKEY_ENCODED_LEN + 1];
} SearchContext;

typedef struct {
  SearchContext *ctx;
  uint64_t attempts;
} WorkerArgs;

static bool isBase32String(const char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    char c = (char)toupper((unsigned char)s[i]);

    if (strchr(BASE32_ALPHABET, c) == NULL) {
      return false;
    }
  }

  return true;
}

static void uppercaseInPlace(char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    s[i] = (char)toupper((unsigned char)s[i]);
  }
}

static uint16_t crc16Xmodem(const uint8_t *data, size_t len) {
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

static void base32EncodeNoPadding(
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

static void strkeyEncode(uint8_t version, const uint8_t raw[32], char out[57]) {
  uint8_t payload[STRKEY_RAW_LEN];

  payload[0] = version;
  memcpy(payload + 1, raw, 32);

  uint16_t checksum = crc16Xmodem(payload, 33);

  payload[33] = (uint8_t)(checksum & 0xff);
  payload[34] = (uint8_t)((checksum >> 8) & 0xff);

  base32EncodeNoPadding(payload, STRKEY_RAW_LEN, out);
}

static bool matchesSearch(const SearchContext *ctx, const char *publicStrkey) {
  if (memcmp(publicStrkey, ctx->fullPrefix, ctx->prefixLen) != 0) {
    return false;
  }

  if (ctx->suffixLen == 0) {
    return true;
  }

  return memcmp(
    publicStrkey + STRKEY_ENCODED_LEN - ctx->suffixLen,
    ctx->suffix,
    ctx->suffixLen
  ) == 0;
}

static void validateSearchSpan(size_t totalLen) {
  if (totalLen > 10) {
    fprintf(stderr, "Try shorter inputs. Each extra character makes the search 32x harder.\n");
    exit(1);
  }

  if (totalLen == 5) {
    printf("Be advised: this could take a long time.\n");
  } else if (totalLen == 6) {
    printf("Be advised: this could take hours or days.\n");
  } else if (totalLen == 7) {
    printf("Be advised: this could take days, weeks, or longer.\n");
  } else if (totalLen >= 8) {
    printf("Be advised: this is still a very large search even with threads.\n");
  }
}

#ifdef STELLAR_VANITY_PARTIALS
static bool initOutputLock(SearchContext *ctx) {
#ifdef _WIN32
  InitializeCriticalSection(&ctx->outputLock);
  return true;
#else
  return pthread_mutex_init(&ctx->outputMutex, NULL) == 0;
#endif
}

static void destroyOutputLock(SearchContext *ctx) {
#ifdef _WIN32
  DeleteCriticalSection(&ctx->outputLock);
#else
  pthread_mutex_destroy(&ctx->outputMutex);
#endif
}

static void showPartial(
  SearchContext *ctx,
  const char *publicStrkey,
  bool prefixMatch,
  bool suffixMatch
) {
  size_t prefixAfterGLen = ctx->prefixLen - 1;
  bool preferPrefix = prefixAfterGLen >= ctx->suffixLen;

  if ((preferPrefix && !prefixMatch) || (!preferPrefix && !suffixMatch)) {
    return;
  }

#ifdef _WIN32
  EnterCriticalSection(&ctx->outputLock);
#else
  pthread_mutex_lock(&ctx->outputMutex);
#endif

  if (preferPrefix) {
    printf(
      "\rPartial Fit: %s...%s",
      ctx->fullPrefix,
      publicStrkey + STRKEY_ENCODED_LEN - 6
    );
  } else {
    printf("\rPartial Fit: %.6s...%s", publicStrkey, ctx->suffix);
  }
  fflush(stdout);

#ifdef _WIN32
  LeaveCriticalSection(&ctx->outputLock);
#else
  pthread_mutex_unlock(&ctx->outputMutex);
#endif
}
#endif

#ifdef _WIN32
static bool searchIsFound(SearchContext *ctx) {
  return InterlockedCompareExchange(&ctx->found, 0, 0) != 0;
}

static bool claimFound(SearchContext *ctx) {
  return InterlockedCompareExchange(&ctx->found, 1, 0) == 0;
}

static void markFound(SearchContext *ctx) {
  InterlockedExchange(&ctx->found, 1);
}

static unsigned int __stdcall searchWorker(void *arg) {
#else
static bool searchIsFound(SearchContext *ctx) {
  bool found;

  pthread_mutex_lock(&ctx->foundMutex);
  found = ctx->found != 0;
  pthread_mutex_unlock(&ctx->foundMutex);

  return found;
}

static bool claimFound(SearchContext *ctx) {
  bool claimed = false;

  pthread_mutex_lock(&ctx->foundMutex);
  if (!ctx->found) {
    ctx->found = 1;
    claimed = true;
  }
  pthread_mutex_unlock(&ctx->foundMutex);

  return claimed;
}

static void markFound(SearchContext *ctx) {
  pthread_mutex_lock(&ctx->foundMutex);
  ctx->found = 1;
  pthread_mutex_unlock(&ctx->foundMutex);
}

static void *searchWorker(void *arg) {
#endif
  WorkerArgs *worker = (WorkerArgs *)arg;
  SearchContext *ctx = worker->ctx;

  while (!searchIsFound(ctx)) {
    uint8_t seed[crypto_sign_SEEDBYTES];
    uint8_t publicKey[crypto_sign_PUBLICKEYBYTES];
    uint8_t secretKey[crypto_sign_SECRETKEYBYTES];
    char publicStrkey[STRKEY_ENCODED_LEN + 1];
    char secretStrkey[STRKEY_ENCODED_LEN + 1];

    randombytes_buf(seed, sizeof(seed));
    crypto_sign_seed_keypair(publicKey, secretKey, seed);
    strkeyEncode(VERSION_ACCOUNT_ID, publicKey, publicStrkey);
    worker->attempts++;

    if (matchesSearch(ctx, publicStrkey)) {
      if (claimFound(ctx)) {
        strkeyEncode(VERSION_SEED, seed, secretStrkey);
        memcpy(ctx->foundPublic, publicStrkey, sizeof(ctx->foundPublic));
        memcpy(ctx->foundSecret, secretStrkey, sizeof(ctx->foundSecret));
        sodium_memzero(secretStrkey, sizeof(secretStrkey));
      }

      sodium_memzero(seed, sizeof(seed));
      sodium_memzero(secretKey, sizeof(secretKey));
#ifdef _WIN32
      return 0;
#else
      return NULL;
#endif
    }

#ifdef STELLAR_VANITY_PARTIALS
    bool prefixMatch =
      memcmp(publicStrkey, ctx->fullPrefix, ctx->prefixLen) == 0;
    bool suffixMatch =
      ctx->suffixLen == 0 ||
      memcmp(
        publicStrkey + STRKEY_ENCODED_LEN - ctx->suffixLen,
        ctx->suffix,
        ctx->suffixLen
      ) == 0;
    showPartial(ctx, publicStrkey, prefixMatch, suffixMatch);
#endif
  }

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

static unsigned int defaultThreadCount(void) {
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

static unsigned int parseThreadCount(const char *value) {
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

  char prefixAfterG[64];
  char suffix[64];

  snprintf(prefixAfterG, sizeof(prefixAfterG), "%s", argv[1]);
  snprintf(suffix, sizeof(suffix), "%s", argv[2]);

  uppercaseInPlace(prefixAfterG);
  uppercaseInPlace(suffix);

  if (!isBase32String(prefixAfterG) || !isBase32String(suffix)) {
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

  unsigned int threadCount = argc == 4
    ? parseThreadCount(argv[3])
    : defaultThreadCount();

  size_t prefixAfterGLen = strlen(prefixAfterG);
  size_t suffixLen = strlen(suffix);

  validateSearchSpan(prefixAfterGLen + suffixLen);

  SearchContext ctx;
  memset(&ctx, 0, sizeof(ctx));
#ifndef _WIN32
  if (pthread_mutex_init(&ctx.foundMutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize thread lock.\n");
    return 1;
  }
#endif
#ifdef STELLAR_VANITY_PARTIALS
  if (!initOutputLock(&ctx)) {
    fprintf(stderr, "Failed to initialize output lock.\n");
#ifndef _WIN32
    pthread_mutex_destroy(&ctx.foundMutex);
#endif
    return 1;
  }
#endif
  snprintf(ctx.fullPrefix, sizeof(ctx.fullPrefix), "G%s", prefixAfterG);
  snprintf(ctx.suffix, sizeof(ctx.suffix), "%s", suffix);
  ctx.prefixLen = strlen(ctx.fullPrefix);
  ctx.suffixLen = suffixLen;

  printf("Searching for: %s...%s\n", ctx.fullPrefix, ctx.suffix);
  printf("Threads: %u\n", threadCount);

  WorkerArgs workers[MAX_THREADS];

#ifdef _WIN32
  HANDLE handles[MAX_THREADS];

  for (unsigned int i = 0; i < threadCount; i++) {
    workers[i].ctx = &ctx;
    workers[i].attempts = 0;
    handles[i] = (HANDLE)_beginthreadex(
      NULL,
      0,
      searchWorker,
      &workers[i],
      0,
      NULL
    );

    if (handles[i] == NULL) {
      fprintf(stderr, "Failed to start worker thread.\n");
      markFound(&ctx);

      for (unsigned int j = 0; j < i; j++) {
        WaitForSingleObject(handles[j], INFINITE);
        CloseHandle(handles[j]);
      }

#ifdef STELLAR_VANITY_PARTIALS
      destroyOutputLock(&ctx);
#endif
      return 1;
    }
  }

  WaitForMultipleObjects(threadCount, handles, TRUE, INFINITE);

  uint64_t totalAttempts = 0;
  for (unsigned int i = 0; i < threadCount; i++) {
    totalAttempts += workers[i].attempts;
    CloseHandle(handles[i]);
  }
#else
  pthread_t handles[MAX_THREADS];

  for (unsigned int i = 0; i < threadCount; i++) {
    workers[i].ctx = &ctx;
    workers[i].attempts = 0;

    if (pthread_create(&handles[i], NULL, searchWorker, &workers[i]) != 0) {
      fprintf(stderr, "Failed to start worker thread.\n");
      markFound(&ctx);

      for (unsigned int j = 0; j < i; j++) {
        pthread_join(handles[j], NULL);
      }

#ifdef STELLAR_VANITY_PARTIALS
      destroyOutputLock(&ctx);
#endif
      pthread_mutex_destroy(&ctx.foundMutex);
      return 1;
    }
  }

  uint64_t totalAttempts = 0;
  for (unsigned int i = 0; i < threadCount; i++) {
    pthread_join(handles[i], NULL);
    totalAttempts += workers[i].attempts;
  }
#endif

  if (ctx.found) {
    printf("\nKeypair found:\n");
    printf("Attempts:   %llu\n", (unsigned long long)totalAttempts);
    printf("Public Key: %s\n", ctx.foundPublic);
    printf("Secret Key: %s\n", ctx.foundSecret);

    sodium_memzero(ctx.foundSecret, sizeof(ctx.foundSecret));
#ifdef STELLAR_VANITY_PARTIALS
    destroyOutputLock(&ctx);
#endif
#ifndef _WIN32
    pthread_mutex_destroy(&ctx.foundMutex);
#endif
    return 0;
  }

  fprintf(stderr, "Search stopped without a result.\n");
#ifdef STELLAR_VANITY_PARTIALS
  destroyOutputLock(&ctx);
#endif
#ifndef _WIN32
  pthread_mutex_destroy(&ctx.foundMutex);
#endif
  return 1;
}
