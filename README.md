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
- `PublicKey` generates a valid vanity public key without a signer

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

The Windows build uses all detected CPU cores by default, up to 64 threads.

From PowerShell:

```powershell
.\build_stellar_vanity_parallel.ps1
.\stellar_vanity_parallel.exe DRS DUNA
```

To force a specific thread count:

```powershell
.\stellar_vanity_parallel.exe DRS DUNA 16
```

### macOS Parallel Build

The parallel C version uses all detected logical CPU cores by default, up to 64
threads.

Install dependencies:

```zsh
brew install libsodium pkg-config
```

Build:

```zsh
./build_stellar_vanity_parallel.sh
```

Run with all detected logical CPU cores:

```zsh
./stellar_vanity_parallel DRS DUNA
```

To force a specific thread count:

```zsh
./stellar_vanity_parallel DRS DUNA 16
```

To build manually:

```zsh
cc -O3 -pthread stellar_vanity_parallel.c -o stellar_vanity_parallel $(pkg-config --cflags --libs libsodium)
```

## Disclaimer

These scripts are for demonstration purposes only. Generating vanity public keys and using them for real-world applications can have security implications. Always exercise caution and follow best practices when working with cryptographic keys.

It is highly recommended that you avoid using vanity keys in production or sensitive environments. I provide `configureVanitySigners.py` without any warranties or representations to create a transaction replacing a vanity account's signers with your own public keys. [More info](https://www.reddit.com/r/Stellar/comments/166bbqi/comment/jyod9ht/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button).
