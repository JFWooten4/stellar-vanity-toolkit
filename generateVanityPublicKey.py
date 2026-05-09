from globals import *

def main():
  phrase = input("Enter the desired vanity phrase: ").strip().upper()
  if len(phrase) >= 54:
    sys.exit("Try a shorter phrase.")
  if any(chars not in BASE_32_ALPHABET for chars in phrase):
    sys.exit("Try a base32 phrase.")
  while True:
    pubKey = Keypair.random().public_key
    PKnoChecksum = pubKey[:-2]
    phraseInsertStartIndex = random.randint(1, len(PKnoChecksum) - len(phrase))
    phraseInsertEndIndex = phraseInsertStartIndex + len(phrase)
    PKnoChecksumWithPhrase = (
      PKnoChecksum[:phraseInsertStartIndex] +
      phrase +
      PKnoChecksum[phraseInsertEndIndex:]
    )
    PK = getValidStellarPubKeyIfExists(PKnoChecksumWithPhrase)
    if PK:
      sys.exit(f"\n\n\tPublic key found:\n\t{PK}\n")

def getValidStellarPubKeyIfExists(pubKeyNoChecksum):
  for char1 in BASE_32_ALPHABET:
    for char2 in BASE_32_ALPHABET:
      try:
        trying = f"{pubKeyNoChecksum}{char1}{char2}"
        keypair = Keypair.from_public_key(trying)
        return trying
      except exceptions.Ed25519PublicKeyInvalidError:
        continue
  return 0

main()
