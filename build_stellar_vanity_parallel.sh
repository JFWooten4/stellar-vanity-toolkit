#!/usr/bin/env sh
set -eu

cc=${CC:-cc}
variant=${1:-}

case "$variant" in
  "")
    source_file=stellar_vanity_parallel.c
    output_file=stellar_vanity_parallel
    ;;
  --partials)
    source_file=stellar_vanity_parallel_partials.c
    output_file=stellar_vanity_parallel_partials
    ;;
  *)
    echo "Usage: $0 [--partials]" >&2
    exit 1
    ;;
esac

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

"$cc" -O3 -pthread "$source_file" -o "$output_file" $sodium_flags
