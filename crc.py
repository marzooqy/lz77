import binascii
import sys

with open(sys.argv[1], 'rb') as file:
    with open(sys.argv[2], 'rb') as file2:
        print(binascii.crc32(file.read()))
        print(binascii.crc32(file2.read()))