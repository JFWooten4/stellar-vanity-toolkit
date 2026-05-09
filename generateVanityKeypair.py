from globals import *
import os

clean = 49 * " "

def main():
  try:
    # Inputs #
    prefix = getINPUT("Enter the desired prefix: ")
    firstChar = prefix[0] if prefix else ""
    if firstChar not in ["", "A", "B", "C", "D"]:
      sys.exit("Try a prefix starting with A/B/C/D.")
    suffix = getINPUT("Enter the desired suffix: ")
    
    # Input Validation #
    if any(chars not in BASE_32_ALPHABET for chars in prefix + suffix):
      sys.exit("Try base32 inputs.")
    prefixLen = len(prefix)
    suffixLen = len(suffix)
    validateSearchSpan(prefixLen + suffixLen)
    searchingMoreForPrefix = prefixLen > suffixLen
    prefix = f"G{prefix}"
    
    # Main Keygen #
    partials = getINPUT("Show partial matches? (Y/n): ") == "Y"

    startTime = time.time()
    n = 0
    while True:
      if not n % 3200:
        showSearching(startTime)
      n += 1
      keypair = Keypair.random()
      PK = keypair.public_key
      prefixMatch = PK.startswith(prefix)
      suffixMatch = PK.endswith(suffix)
      if prefixMatch and suffixMatch:
        result = f"""\n
        \tKeypair found in {n:,} attempts:
        \tPublic Key: {PK}
        \tSecret Key: {keypair.secret}\n
        """
        sys.exit(result)
      if partials:
        if prefixLen == suffixLen:
          if prefixMatch:
            partialMatch = [
              f"\r Partial Fit: {clean}{PK[:6]}—",
              f"\n  Public Key: {PK}",
              f"\n  Secret Key: {keypair.secret}\n"
            ]
            sys.stdout.write("".join(partialMatch))
          if suffixMatch:
            partialMatch = [
              f"\r Partial Fit: {clean}—{PK[-6:]}",
              f"\n  Public Key: {PK}",
              f"\n  Secret Key: {keypair.secret}\n"
            ]
            sys.stdout.write("".join(partialMatch))
        elif searchingMoreForPrefix:
          if prefixMatch:
            partialMatch = [
              f"\r Partial Fit: {clean}{PK[:6]}—",
              f"\n  Public Key: {PK}",
              f"\n  Secret Key: {keypair.secret}\n"
            ]
            sys.stdout.write("".join(partialMatch))
        elif suffixMatch:
          partialMatch = [
            f"\r Partial Fit: {clean}—{PK[-6:]}",
            f"\n  Public Key: {PK}",
            f"\n  Secret Key: {keypair.secret}\n"
          ]
          sys.stdout.write("".join(partialMatch))
  except KeyboardInterrupt:
    sys.exit("\nUser ended keypair search.")

def validateSearchSpan(totalInputLen):
  if totalInputLen > 10:
    sys.exit("Try shorter inputs.")
  if totalInputLen == 5:
    print("Be advised: >30 min to compute.")
  if totalInputLen == 6:
    print("Be advised: >2 hrs to compute.")
  if totalInputLen == 7:
    print("Be advised: >10 hrs to compute.")
  if totalInputLen == 8:
    print("Be advised: >30 hrs to compute.")
  if totalInputLen == 9:
    print("Be advised: >3 days to compute.")
  if totalInputLen == 10:
    print("Be advised: >9 days to compute.")

def generateVanityContractAddress(deriveContractAddress, prefix="", suffix="", partials=True):
  """
  Brute-force a Soroban contract address vanity match by varying 32-byte salts.
  `deriveContractAddress` must be a callable: salt_bytes -> contract_address ("C...")
  """
  prefix = prefix.strip().upper()
  suffix = suffix.strip().upper()
  if any(chars not in BASE_32_ALPHABET for chars in prefix + suffix):
    sys.exit("Try base32 inputs.")

  prefixLen = len(prefix)
  suffixLen = len(suffix)
  validateSearchSpan(prefixLen + suffixLen)
  searchingMoreForPrefix = prefixLen > suffixLen
  prefix = f"C{prefix}"

  startTime = time.time()
  n = 0
  while True:
    if not n % 3200:
      showSearching(startTime)
    n += 1
    salt = os.urandom(32)
    contractAddress = deriveContractAddress(salt).strip().upper()

    prefixMatch = contractAddress.startswith(prefix)
    suffixMatch = contractAddress.endswith(suffix)
    if prefixMatch and suffixMatch:
      return {
        "attempts": n,
        "contract_address": contractAddress,
        "salt_hex": salt.hex(),
      }

    if partials:
      if prefixLen == suffixLen:
        if prefixMatch:
          sys.stdout.write(
            "".join(
              [
                f"\r Partial Fit: {clean}{contractAddress[:6]}—",
                f"\n  Contract Address: {contractAddress}",
                f"\n  Salt (hex): {salt.hex()}\n",
              ]
            )
          )
        if suffixMatch:
          sys.stdout.write(
            "".join(
              [
                f"\r Partial Fit: {clean}—{contractAddress[-6:]}",
                f"\n  Contract Address: {contractAddress}",
                f"\n  Salt (hex): {salt.hex()}\n",
              ]
            )
          )
      elif searchingMoreForPrefix and prefixMatch:
        sys.stdout.write(
          "".join(
            [
              f"\r Partial Fit: {clean}{contractAddress[:6]}—",
              f"\n  Contract Address: {contractAddress}",
              f"\n  Salt (hex): {salt.hex()}\n",
            ]
          )
        )
      elif (not searchingMoreForPrefix) and suffixMatch:
        sys.stdout.write(
          "".join(
            [
              f"\r Partial Fit: {clean}—{contractAddress[-6:]}",
              f"\n  Contract Address: {contractAddress}",
              f"\n  Salt (hex): {salt.hex()}\n",
            ]
          )
        )

main()
