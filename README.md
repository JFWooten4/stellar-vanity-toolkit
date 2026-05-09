# Vanity Stellar Address Toolkit

## Python Search

### Requirements

- 🐍 Python 3.9+
- 📦 `stellar-sdk`

### Install

Recommended (virtual environment, PowerShell on Windows):

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install stellar-sdk
```

If PowerShell blocks activation, run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.venv\Scripts\Activate.ps1
```

Recommended (virtual environment, bash/zsh on macOS/Linux):

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install stellar-sdk
```

Alternative (global install):

```bash
python3 -m pip install --upgrade pip
python3 -m pip install stellar-sdk
```

Alternative (global install, Windows PowerShell):

```powershell
py -3 -m pip install --upgrade pip
py -3 -m pip install stellar-sdk
```

### Usage

1. Run the script by executing the following command.

If you activated a virtual environment, use `python` so the script runs with the packages installed in `.venv`:

```powershell
python .\generateVanity{FUNCTION}.py
```

If you did not activate a virtual environment and installed packages globally, use:

```bash
python3 generateVanity{FUNCTION}.py
```

On Windows PowerShell with a global install:

```powershell
py -3 .\generateVanity{FUNCTION}.py
```

- `Keypair` generates a standard pre- or suffix vanity public key
- `PublicKey` generate a valid vanity public key without a signer

2. Enter your desired inputs.

3. View the result.

## C Search

The C searcher is separate from the Python scripts. It does not use
`stellar-sdk`; it uses `libsodium`.

The C searcher uses this argument format:

```text
<prefix-after-G> <suffix>
```

For example, this searches for a public key like `GDRS...DUNA`:

```text
DRS DUNA
```

### Windows Parallel Build

The parallel C version is Windows-only and uses all detected CPU cores by
default, up to 64 threads.

From PowerShell:

```powershell
.\build_stellar_vanity_parallel.ps1
.\stellar_vanity_parallel.exe DRS DUNA
```

To force a specific thread count:

```powershell
.\stellar_vanity_parallel.exe DRS DUNA 16
```

### macOS Build

The checked-in parallel C file uses Windows threading APIs, so on macOS build
the portable single-process C searcher and run one worker per logical CPU core.

Install dependencies:

```zsh
brew install libsodium pkg-config
```

Build:

```zsh
clang -O3 stellar_vanity.c -o stellar_vanity $(pkg-config --cflags --libs libsodium)
```

Run one search:

```zsh
./stellar_vanity DRS DUNA
```

Use all logical CPU cores on macOS:

```zsh
rm -f vanity-*.log
pids=()
for i in $(seq 1 $(sysctl -n hw.logicalcpu)); do
  ./stellar_vanity DRS DUNA > "vanity-$i.log" &
  pids+=($!)
done

wait -n
kill $pids 2>/dev/null
grep -h -A5 "Keypair found" vanity-*.log
```

The first worker to find a matching keypair prints the result to its log, then
the command stops the remaining workers and displays the found keypair.

## Disclaimer

These scripts are for demonstration purposes only. Generating vanity public keys and using them for real-world applications can have security implications. Always exercise caution and follow best practices when working with cryptographic keys. 

It is highly recommended that you avoid using vanity keys in production or sensitive environments. I provide `configureVanitySigners.py` without any warranties or representations to create a transaction replacing a vanity account's signers with your own public keys. [More info](https://www.reddit.com/r/Stellar/comments/166bbqi/comment/jyod9ht/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button).
