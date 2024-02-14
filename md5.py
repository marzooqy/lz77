import hashlib
import sys

with open(sys.argv[1], 'rb') as file:
    with open(sys.argv[2], 'rb') as file2:
        print(hashlib.md5(file.read()).hexdigest())
        print(hashlib.md5(file2.read()).hexdigest())