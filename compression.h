#include "hash_table.h"

#include <cstdint>
#include <vector>
#include <iostream>

namespace lz77 {

	using std::vector;
	using std::cout, std::endl;

	typedef vector<unsigned char> bytes;

	int64_t getInt(bytes& buf, int64_t& pos) {
		if(pos >= buf.size()) {
			return -1;
		}

		int64_t b = buf[pos++];
		int64_t n = 0;
		int64_t i = 0;

		while(b & 0b10000000) {
			n |= (b & 0b01111111) << (7 * i);

			if(pos >= buf.size()) {
				return -1;
			}

			b = buf[pos++];
			i++;
		}

		return n | (b << (7 * i));
	}

	void putInt(bytes& buf, int64_t& pos, int64_t n) {
		while(n > 0b01111111) {
			buf[pos++] = 0b10000000 | n;
			n >>= 7;
		}

		buf[pos++] = n;
	}

	int64_t getLength(bytes& buf, int64_t& pos, int64_t b) {
		if(b & 0b01000000) {
			int64_t n = getInt(buf, pos);

			if(n == -1) {
				return n;
			}

			return (n << 6) | (b & 0b00111111);
		}

		return b & 0b00111111;
	}

	void putLength(bytes& buf, int64_t& pos, int64_t n, int64_t mask) {
		if(n > 0b00111111) {
			buf[pos++] = 0b01000000 | mask | (n & 0b00111111);
			putInt(buf, pos, n >> 6);
		} else {
			buf[pos++] = mask | n;
		}
	}

	//copy length bytes from src at srcPos to dst at dstPos and increment srcPos and dstPos
	//copying one byte at a time is REQUIRED in some cases, otherwise decompression will not work
	void copyBytes(bytes& src, int64_t& srcPos, bytes& dst, int64_t& dstPos, int64_t length) {
		for(int64_t i = 0; i < length; i++) {
			dst[dstPos++] = src[srcPos++];
		}
	}

	//compresses src, returns an empty vector if compression fails
	//ideally the failure scenario would never occur
	bytes compress(bytes& src) {
		bytes dst = bytes(6 + src.size() + src.size() / 63 + 1);
		Table table = Table(src);

		int64_t srcPos = 0;
		int64_t dstPos = 3;

		int64_t i = 1;
		int64_t lastMatchLocation = 0;

		while(i <= src.size() - MIN_LENGTH) {
			Match match = table.getLongestMatch(i, lastMatchLocation);

			if(match.length >= MIN_LENGTH) {
				//copy bytes from src to dst until the location of the match is reached
				int64_t lit = match.location - srcPos;

				//literal copy
				//i.e. no compression/no match found
				if(lit > 0) {
					putLength(dst, dstPos, lit, 0b00000000);
					copyBytes(src, srcPos, dst, dstPos, lit);
				}
				
				//offset copy
				putLength(dst, dstPos, match.length, 0b10000000);
				putInt(dst, dstPos, match.offset);
				
				//DEBUG
				//cout << dstPos << " " << match.location << " " << match.length << " " << match.offset << endl;
				
				srcPos += match.length;

				i = match.location + match.length;
				lastMatchLocation = i;

			} else {
				i++;
			}
		}

		//copy the remaining bytes at the end
		int64_t lit = src.size() - srcPos;

		if(lit > 0) {
			putLength(dst, dstPos, lit, 0b00000000);
			copyBytes(src, srcPos, dst, dstPos, lit);
		}

		//write the uncompressed size at the start
		int64_t uncompressedSize = src.size();
		dst[0] = uncompressedSize >> 16;
		dst[1] = uncompressedSize >> 8;
		dst[2] = uncompressedSize;

		dst.resize(dstPos);
		return dst;
	}

	//decompresses src, returns an empty vector if decompression fails
	//it should only fail on broken compressed files
	bytes decompress(bytes& src) {
		int64_t srcPos = 3;
		int64_t dstPos = 0;

		bytes dst = bytes(((int64_t) src[0] << 16) | ((int64_t) src[1] << 8) | src[2]);

		while(srcPos < src.size()) {
			int64_t b = src[srcPos++];

			//offset copy
			if(b & 0b10000000) {
				int64_t length = getLength(src, srcPos, b);
				int64_t offset = getInt(src, srcPos);

				//DEBUG
				//cout << dstPos << " " << length << " " << offset << endl;

				if(length == -1 || offset == -1 || dstPos + length > dst.size() || offset > dstPos)  {
					return bytes();
				}

				//copy bytes from an earlier location in the decompressed output
				int64_t fromOffset = dstPos - offset;
				copyBytes(dst, fromOffset, dst, dstPos, length);

			//literal copy
			} else {
				int64_t lit = getLength(src, srcPos, b);
				if(lit == -1 || srcPos + lit > src.size() || dstPos + lit > dst.size())  {
					return bytes();
				}

				copyBytes(src, srcPos, dst, dstPos, lit);
			}
		}

		return dst;
	}

}
