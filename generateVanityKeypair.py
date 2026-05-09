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
    if approxSearchTime:
      print(approxSearchTime)
    while True:
      keypair = Keypair.random()
      PK = keypair.public_key
      prefixMatch = PK.startswith(prefix)
      suffixMatch = PK.endswith(suffix)
      if prefixMatch and suffixMatch:
        result = f"""\n
        \tKeypair found:
        \tPublic Key: {PK}
        \tSecret Key: {keypair.secret}\n
        """
        sys.exit(result)
      if not partials: continue
      if searchingMoreForPrefix:
        if not prefixMatch: continue
        partial = f"{clean}—{PK[-6:]}"
      else:
        if not suffixMatch: continue
        partial = f"{clean}{PK[:6]}—"
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
    sys.exit("Try shorter inputs. Each extra character makes the search 32x harder.")

  averageAttempts = 32 ** totalInputLen // 2
  attempts = f"{averageAttempts:,}"
  warnings = {
    0: None,
    1: None,
    2: None,
    3: None,
    4: f"Be advised: average search is about {attempts} attempts.",
    5: f"Be advised: average search is about {attempts} attempts and may take a while.",
    6: f"Be advised: average search is about {attempts} attempts and may take hours or days.",
    7: f"Be advised: average search is about {attempts} attempts and may take days, weeks, or longer.",
    8: f"Be advised: average search is about {attempts} attempts and is probably impractical in Python.",
    9: f"Be advised: average search is about {attempts} attempts and is not practical for normal Python keygen.",
    10: f"Be advised: average search is about {attempts} attempts and is not practical for normal Python keygen.",
  }
  return warnings.get(totalInputLen)

print(main())
