#!/usr/bin/env sh
set -eu

cc=${CC:-cc}

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libsodium; then
  sodium_flags=$(pkg-config --cflags --libs libsodium)
elif command -v brew >/dev/null 2>&1; then
  sodium_prefix=$(brew --prefix libsodium)
  if [ ! -f "${sodium_prefix}/include/sodium.h" ]; then
    echo "libsodium headers were not found. Run: brew install libsodium pkg-config" >&2
    exit 1
  fi
  sodium_flags="-I${sodium_prefix}/include -L${sodium_prefix}/lib -lsodium"
else
  sodium_flags="-lsodium"
fi

"$cc" -O3 -pthread stellar_vanity_parallel.c -o stellar_vanity_parallel $sodium_flags
