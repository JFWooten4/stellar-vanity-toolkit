from stellar_sdk import Keypair, exceptions
import random, sys

BASE_32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

def getINPUT(prompt):
  return input(prompt).strip().upper()

