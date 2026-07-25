# Stellar Vanity Address Toolkit

Generate Stellar public addresses with a chosen prefix, suffix, or embedded
phrase. The toolkit includes an accessible Python implementation and faster C
searchers for single-core and parallel workloads.

## What this toolkit includes

### Python

| Tool | Purpose |
| --- | --- |
| `generateVanityKeypair.py` | Interactive prefix and suffix search in Python |
| `generateVanityPublicKey.py` | Inserts a phrase into a checksum-valid public address |
| `configureVanitySigners.py` | Reconfigures signers on a funded Stellar account |

### C

| Tool | Purpose |
| --- | --- |
| `stellarVanity.c` | Faster single-threaded prefix and suffix search |
| `stellarVanityParallel.c` | Multi-threaded prefix and suffix search |
| `stellarVanityParallelPartials.c` | Multi-threaded search with live partial matches |

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

`configureVanitySigners.py` signs and submits a transaction directly to the
Stellar public network; it is not configured for testnet or offline use. It
changes account authorization and can permanently remove access if a signer is
incorrect, unavailable, or not backed up. Review the source, inspect the
generated XDR, and understand Stellar signer weights and thresholds before
considering it. It is not part of the quick-start workflow below.

## Network and offline behavior

The vanity-generation tools do not require a network connection after their
dependencies are available locally.

| Component | Network behavior |
| --- | --- |
| Python keypair and public-key generators | No runtime network access |
| C search executables | No runtime network access |
| C build scripts | Never download; use only a local libsodium installation or archive |
| `configureVanitySigners.py` | Requires public Horizon to load the account, fetch a fee, and submit the transaction |

Documentation links and package-install examples may point to external sites,
but the search programs themselves do not contact those sites. The signer
configuration workflow is the one intentional exception and cannot complete
offline because it modifies an account on the Stellar network.

### Prepare Python packages for an offline computer

On a connected computer with the same operating system, architecture, and
Python version as the offline computer, download the pinned requirement and
its dependencies:

```bash
python -m pip download --dest offline-packages -r requirements.txt
```

Copy this repository and the resulting `offline-packages` directory to the
offline computer. Install without consulting a package index:

```bash
python -m pip install --no-index --find-links offline-packages -r requirements.txt
```

The `--no-index` option makes the offline guarantee explicit: installation
fails instead of attempting a network request when a required package is
missing.

## Python quick start

### Requirements

- Python 3.10 or newer
- [`stellar-sdk`](https://stellar-sdk.readthedocs.io/)

Use a virtual environment to keep the dependency isolated from the rest of
your system.

### Install

<details>
<summary><strong>Windows — PowerShell</strong></summary>

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

If local policy blocks virtual-environment activation, allow scripts only for
the current PowerShell process:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.venv\Scripts\Activate.ps1
```

</details>

<details>
<summary><strong>macOS or Linux — bash/zsh</strong></summary>

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

</details>

### Generate a vanity keypair

<details>
<summary><strong>Windows — PowerShell</strong></summary>

```powershell
python .\generateVanityKeypair.py
```

</details>

<details>
<summary><strong>macOS or Linux — bash/zsh</strong></summary>

```bash
python generateVanityKeypair.py
```

</details>

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

<details>
<summary><strong>Windows — PowerShell</strong></summary>

```powershell
python .\generateVanityPublicKey.py
```

</details>

<details>
<summary><strong>macOS or Linux — bash/zsh</strong></summary>

```bash
python generateVanityPublicKey.py
```

</details>

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

### Platform instructions

<details>
<summary><strong>Windows — PowerShell and Visual Studio</strong></summary>

#### Requirements

- 64-bit Windows
- Visual Studio 2022 Build Tools with the C++ build tools installed
- PowerShell and `tar.exe`

The build scripts never access the network. Before disconnecting, download the
official `libsodium-1.0.21-stable-msvc.zip` archive and place it in the
repository root as `libsodium-msvc.zip`. The first build extracts it into the
ignored `vendor` directory. You can instead copy an already extracted
`vendor/libsodium` directory with the repository.

The scripts do not authenticate the archive. Verify the download using the
signature information published with the official libsodium release before
moving it into a trusted offline environment.

#### Parallel build

```powershell
.\buildStellarVanityParallel.ps1
.\stellarVanityParallel.exe DRS DUNA
```

The parallel search uses the detected logical CPU count, capped at 64 threads.
Pass a number from 1 through 64 to override it:

```powershell
.\stellarVanityParallel.exe DRS DUNA 16
```

#### Parallel build with partial matches

The partial-match variant is a separate executable, so the default parallel
search remains focused on maximum throughput:

```powershell
.\buildStellarVanityParallel.ps1 -Partials
.\stellarVanityParallelPartials.exe DRS DUNA
```

It accepts the same optional worker count:

```powershell
.\stellarVanityParallelPartials.exe DRS DUNA 16
```

Worker output is synchronized to keep partial results readable. Terminal I/O
still adds overhead, especially for short patterns that match frequently.
Each partial line shows a public key matching the longer requested side
(prefix when the two sides are equal). Only the final full match includes a
secret key.

#### Serial build

```powershell
.\buildStellarVanity.ps1
.\stellarVanity.exe DRS DUNA
```

Enable partial-match output with:

```powershell
.\stellarVanity.exe DRS DUNA --partials
```

</details>

<details>
<summary><strong>macOS — zsh and Homebrew</strong></summary>

Install the compiler and dependencies:

```zsh
xcode-select --install
brew install libsodium pkg-config
```

Build and run:

```zsh
./buildStellarVanityParallel.sh
./stellarVanityParallel DRS DUNA
```

Specify a worker count when needed:

```zsh
./stellarVanityParallel DRS DUNA 16
```

Build the separate partial-match variant with:

```zsh
./buildStellarVanityParallel.sh --partials
./stellarVanityParallelPartials DRS DUNA
```

</details>

<details>
<summary><strong>Linux — bash and system packages</strong></summary>

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libsodium-dev pkg-config
./buildStellarVanityParallel.sh
./stellarVanityParallel DRS DUNA
```

Pass `--partials` to the build script to produce
`stellarVanityParallelPartials` instead.

For other distributions, install a C compiler, POSIX threads, libsodium
development headers, and `pkg-config`, then run the same build script.

</details>

### Manual parallel build

```bash
cc -O3 -pthread stellarVanityParallel.c -o stellarVanityParallel \
  $(pkg-config --cflags --libs libsodium)
```

For the separate partial-match executable:

```bash
cc -O3 -pthread stellarVanityParallelPartials.c \
  -o stellarVanityParallelPartials \
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
