from globals import *

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
    approxSearchTime = validateSearchSpan(prefixLen + suffixLen)
    searchingMoreForPrefix = prefixLen > suffixLen
    prefix = f"G{prefix}"
    
    # Main Keygen #
    partials = getINPUT("Show partial matches? (Y/n): ") == "Y"
    print(approxSearchTime)
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
        if searchingMoreForPrefix:
          if not prefixMatch: continue
          partial = f"{clean}{PK[:6]}—"
        else:
          if not suffixMatch: continue
          partial = f"{clean}—{PK[-6:]}"
        partialMatch = [
          f"\r Partial Fit: {partial}",
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
    return "Be advised: >30 min to compute."
  if totalInputLen == 6:
    return "Be advised: >2 hrs to compute."
  if totalInputLen == 7:
    return "Be advised: >10 hrs to compute."
  if totalInputLen == 8:
    return "Be advised: >30 hrs to compute."
  if totalInputLen == 9:
    return "Be advised: >3 days to compute."
  if totalInputLen == 10:
    return "Be advised: >9 days to compute."

print(main())
