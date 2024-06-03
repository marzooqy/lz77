import zlib
import sys

with open(sys.argv[1], 'rb') as file:
    with open(sys.argv[2], 'rb') as file2:
        print(zlib.crc32(file.read()))
        print(zlib.crc32(file2.read()))