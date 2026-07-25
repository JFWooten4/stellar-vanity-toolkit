from stellar_sdk import Keypair, exceptions
import random, sys, time

BASE_32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

def getINPUT(prompt):
  return input(prompt).strip().upper()

def showSearching(startTime):
  minElapsed = int(time.time() - startTime) // 60
  _ = random.choice(["|", "/", "-", "\\"])
  sys.stdout.write("\rSearching " + _ + f" ({minElapsed} min elapsed)")
  sys.stdout.flush()

