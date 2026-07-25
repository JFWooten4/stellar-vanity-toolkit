# Stellar Vanity Address Toolkit

Generate Stellar public addresses with a chosen prefix, suffix, or embedded
phrase. The toolkit includes an accessible Python implementation and faster C
searchers for single-core and parallel workloads.

> [!CAUTION]
> This project handles cryptographic secret keys and has not undergone an
> independent security audit. Use it for experimentation and education. Do not
> use generated keys to protect funds you cannot afford to lose.

## What this toolkit includes

| Tool | Purpose | Produces a usable secret key? |
| --- | --- | --- |
| `generateVanityKeypair.py` | Interactive prefix and suffix search in Python | Yes |
| `stellar_vanity.c` | Faster single-threaded prefix and suffix search | Yes |
| `stellar_vanity_parallel.c` | Multi-threaded prefix and suffix search | Yes |
| `stellar_vanity_parallel_partials.c` | Multi-threaded search with live partial matches | Yes |
| `generateVanityPublicKey.py` | Inserts a phrase into a checksum-valid public address | **No** |
| `configureVanitySigners.py` | Reconfigures signers on a funded Stellar account | Uses an existing key; advanced and high risk |

The keypair searchers repeatedly generate complete random keypairs and discard
the ones that do not match. They never construct a secret key from the chosen
text. At ordinary Base32 positions, every additional constrained character
makes the search approximately 32 times more expensive.

Stellar account IDs use the Base32 alphabet:

```text
ABCDEFGHIJKLMNOPQRSTUVWXYZ234567
```

The leading `G` is part of every Stellar account ID and is not included in the
prefix argument. The next character can only be `A`, `B`, `C`, or `D`.

## Security guidance

Use the following precautions whenever a command produces a secret key:

- Run the search on a trusted, malware-free computer.
- Prefer an offline environment for keys intended to hold value.
- Never paste a secret key into a website, chat, issue, log, or screenshot.
- Treat terminal history, scrollback, recordings, and redirected output as
  sensitive because the tools print the secret key.
- Back up the secret securely before funding the account.
- Independently verify that the secret derives the displayed public key.
- Start with a test account and a minimal balance.

`generateVanityPublicKey.py` is fundamentally different from the keypair
searchers. It creates a checksum-valid public address without finding its
corresponding secret key. No one is expected to be able to sign for that
address. Use its output only for non-funded demonstrations.

`configureVanitySigners.py` signs and can submit a transaction directly to the
Stellar public network; it is not configured for testnet. It changes account
authorization and can permanently remove access if a signer is incorrect,
unavailable, or not backed up. Review the source, inspect the generated XDR,
and understand Stellar signer weights and thresholds before considering it. It
is not part of the quick-start workflow below.

The Windows C build scripts download a pinned libsodium archive over HTTPS when
the dependency is not already present. They do not currently verify the
archive with a published checksum. Review the scripts and dependency source if
your threat model requires supply-chain verification.

## Python quick start

### Requirements

- Python 3.9 or newer
- [`stellar-sdk`](https://stellar-sdk.readthedocs.io/)

Use a virtual environment to keep the dependency isolated from the rest of
your system.

### Windows PowerShell

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install stellar-sdk
```

If local policy blocks virtual-environment activation, allow scripts only for
the current PowerShell process:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.venv\Scripts\Activate.ps1
```

### macOS or Linux

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install stellar-sdk
```

### Generate a vanity keypair

```powershell
python .\generateVanityKeypair.py
```

On macOS or Linux:

```bash
python generateVanityKeypair.py
```

The script asks for:

1. A prefix after the standard leading `G`
2. A suffix
3. Whether to display partial matches

For example, the prefix `DRS` and suffix `DUNA` search for an address shaped
like:

```text
GDRS...DUNA
```

Press `Ctrl+C` to stop a long-running search.

### Generate a public address without a signer

```powershell
python .\generateVanityPublicKey.py
```

This utility places a requested phrase inside a checksum-valid Stellar account
ID. It does not produce or know the secret key. Its output is suitable only
for non-funded demonstrations where the address will never need to sign.

## Faster C search

The C searchers use
[`libsodium`](https://doc.libsodium.org/) for random bytes and Ed25519 key
generation. Choose the parallel version for normal use; choose the serial
version when you want one worker, or choose the separate parallel-partials
target when you want live partial-match output from multiple workers.

Both programs accept the desired prefix without the leading `G`, followed by
the suffix.

### Windows requirements

- 64-bit Windows
- Visual Studio 2022 Build Tools with the C++ build tools installed
- PowerShell, `curl.exe`, and `tar.exe`

The build scripts download and unpack libsodium into the ignored `vendor`
directory when necessary.

### Windows parallel build

```powershell
.\build_stellar_vanity_parallel.ps1
.\stellar_vanity_parallel.exe DRS DUNA
```

The parallel search uses the detected logical CPU count, capped at 64 threads.
Pass a number from 1 through 64 to override it:

```powershell
.\stellar_vanity_parallel.exe DRS DUNA 16
```

### Windows parallel build with partial matches

The partial-match variant is a separate executable, so the default parallel
search remains focused on maximum throughput:

```powershell
.\build_stellar_vanity_parallel.ps1 -Partials
.\stellar_vanity_parallel_partials.exe DRS DUNA
```

It accepts the same optional worker count:

```powershell
.\stellar_vanity_parallel_partials.exe DRS DUNA 16
```

Worker output is synchronized to keep partial results readable. Terminal I/O
still adds overhead, especially for short patterns that match frequently.
Each partial line shows a public key matching the longer requested side
(prefix when the two sides are equal). Only the final full match includes a
secret key.

### Windows serial build

```powershell
.\build_stellar_vanity.ps1
.\stellar_vanity.exe DRS DUNA
```

Enable partial-match output with:

```powershell
.\stellar_vanity.exe DRS DUNA --partials
```

### macOS parallel build

Install the compiler and dependencies:

```zsh
xcode-select --install
brew install libsodium pkg-config
```

Build and run:

```zsh
./build_stellar_vanity_parallel.sh
./stellar_vanity_parallel DRS DUNA
```

Specify a worker count when needed:

```zsh
./stellar_vanity_parallel DRS DUNA 16
```

Build the separate partial-match variant with:

```zsh
./build_stellar_vanity_parallel.sh --partials
./stellar_vanity_parallel_partials DRS DUNA
```

### Linux parallel build

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libsodium-dev pkg-config
./build_stellar_vanity_parallel.sh
./stellar_vanity_parallel DRS DUNA
```

Pass `--partials` to the build script to produce
`stellar_vanity_parallel_partials` instead.

For other distributions, install a C compiler, POSIX threads, libsodium
development headers, and `pkg-config`, then run the same build script.

### Manual parallel build

```bash
cc -O3 -pthread stellar_vanity_parallel.c -o stellar_vanity_parallel \
  $(pkg-config --cflags --libs libsodium)
```

For the separate partial-match executable:

```bash
cc -O3 -pthread stellar_vanity_parallel_partials.c \
  -o stellar_vanity_parallel_partials \
  $(pkg-config --cflags --libs libsodium)
```

## Choosing a search pattern

Short patterns are dramatically more practical than long ones. The following
table assumes ordinary positions with 32 possible Base32 characters:

| Constrained characters | Relative expected work |
| ---: | ---: |
| 1 | 32 attempts |
| 2 | 1,024 attempts |
| 3 | 32,768 attempts |
| 4 | 1,048,576 attempts |
| 5 | 33,554,432 attempts |
| 6 | 1,073,741,824 attempts |

These values describe the statistical expectation, not a deadline. A search
may finish on its first attempt or run much longer than the expected value.
Hardware, operating system, implementation, and background workload all affect
elapsed time.

The first character after Stellar's fixed leading `G` is a special case: only
`A`, `B`, `C`, or `D` can occur there. Constraining that position therefore
adds a factor of 4 rather than 32. Each later prefix character and each suffix
character adds the usual factor of approximately 32.

## Project status

This is an experimental command-line toolkit. It is provided as-is, without a
warranty of correctness, fitness, or security. Review the implementation and
test the complete workflow before relying on generated material.

Licensed under the [MIT License](LICENSE).
