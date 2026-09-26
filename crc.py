import zlib
import sys

with open(sys.argv[1], 'rb') as file:
    print(zlib.crc32(file.read()))

with open(sys.argv[2], 'rb') as file2:
    print(zlib.crc32(file2.read()))