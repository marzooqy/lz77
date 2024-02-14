#include "hash_table.h"

#include <cstdint>
#include <vector>
#include <iostream>

namespace lz77 {

	using std::vector;
	using std::cout, std::endl;
	
	typedef vector<uint8_t> bytes;

	//copy length bytes from src at srcPos to dst at dstPos and increment srcPos and dstPos
	//copying one byte at a time is REQUIRED in some cases, otherwise decompression will not work
	void copyBytes(bytes& src, int64_t& srcPos, bytes& dst, int64_t& dstPos, int64_t length) {
		for(int64_t i = 0; i < length; i++) {
			dst[dstPos++] = src[srcPos++];
		}
	}
	
	//variable length encoding of integers
	//a new byte is added if the previous byte is 0b11111111 (255)
	//the only exception is the first byte where the first bit is a flag
	
	//get a variable length integer from the buffer
	int64_t getCount(bytes& src, int64_t& srcPos) {
		int64_t n = 0;
		int64_t b = 0;
		
		if(srcPos >= src.size()) {
			return -1;
		}
		
		b = src[srcPos++];
		n = b & 0b01111111;
		
		if(n == 127) {
			if(srcPos >= src.size()) {
				return -1;
			}
			
			b = src[srcPos++];
			
			while(b == 255) {
				n += 255;
				
				if(srcPos >= src.size()) {
					return -1;
				}
				
				b = src[srcPos++];
			}
			
			n += b;
		}
		
		n++;
		
		return n;
	}
	
	//put a variable length integer in the buffer
	bool putCount(bytes& dst, int64_t& dstPos, int64_t n, uint64_t mask) {
		if(dstPos >= dst.size()) {
			return false;
		}
		
		n--;
		
		if(n < 127) {
			dst[dstPos++] = mask | n;
			
		} else {
			dst[dstPos++] = mask | 0b01111111;
			n -= 127;
			
			while(n > 255) {
				if(dstPos >= dst.size()) {
					return false;
				}
				
				dst[dstPos++] = 255;
				n -= 255;
			}
			
			if(dstPos >= dst.size()) {
				return false;
			}
			
			dst[dstPos++] = n;
		}
		
		return true;
	}

	//compresses src, returns an empty vector if compression fails
	//ideally the failure scenario would never occur
	bytes compress(bytes& src) {
		bytes dst = bytes(6 + src.size() + src.size() / 255 + 1); //likely the maximum possible size of the resulting compressed block
		Table table = Table(src);
		
		int64_t srcPos = 0;
		int64_t dstPos = 6; //leave room for the uncompressed size and the compressed size
		
		//lit = number of bytes to copy from src with no compression
		//nCopy = number of bytes to compress
		//offset = offset from the current location in the decompressed input to where the pattern could be found
		
		int64_t i = 1;
		int64_t lastMatchLocation = 0;
		
		while(i <= src.size() - MIN_LENGTH) {
			Match match = table.getLongestMatch(i, lastMatchLocation);
			
			if(match.length >= MIN_LENGTH) {
				i = match.location + match.length;
				lastMatchLocation = i;
				
			} else {
				i++;
				continue;
			}
			
			//DEBUG
			//cout << match.location << " " << match.length << " " << match.offset << endl;
			
			//copy bytes from src to dst until the location of the match is reached
			int64_t lit = match.location - srcPos;
			
			//literal copy
			//i.e. no compression/no match found
			
			if(lit > 0) {
				//0ppppppp...
				bool ok = putCount(dst, dstPos, lit, 0b00000000);
				
				if(!ok) {
					return bytes();
				}
				
				copyBytes(src, srcPos, dst, dstPos, lit);
			}
			
			//offset copy
			int64_t nCopy = match.length;
			int64_t offset = match.offset - 1; //subtraction is a part of the encoding for the offset
			
			//1ccccccc... oooooooo oooooooo
			bool ok = putCount(dst, dstPos, nCopy, 0b10000000);
			
			if(!ok || dstPos + 2 > dst.size())  {
				return bytes();
			}
			
			dst[dstPos++] = offset >> 8;
			dst[dstPos++] = offset;
			
			srcPos += match.length;
		}
		
		//copy the remaining bytes at the end
		int64_t lit = src.size() - srcPos;
		
		if(lit > 0) {
			//0ppppppp...
			bool ok = putCount(dst, dstPos, lit, 0b00000000);
			
			if(!ok) {
				return bytes();
			}
			
			copyBytes(src, srcPos, dst, dstPos, lit);
		}
		
		//make compression header
		int64_t compressedSize = dstPos - 3;
		int64_t uncompressedSize = src.size();
		
		dst[0] = compressedSize >> 16;
		dst[1] = compressedSize >> 8;
		dst[2] = compressedSize;
		dst[3] = uncompressedSize >> 16;
		dst[4] = uncompressedSize >> 8;
		dst[5] = uncompressedSize;
		
		dst.resize(dstPos);
		return dst;
	}

	//decompresses src, returns an empty vector if decompression fails
	//it should only fail on broken compressed files
	bytes decompress(bytes& src) {
		int64_t srcPos = 0;
		int64_t dstPos = 0;
		
		int64_t uncompressedSize = ((int64_t) src[srcPos++] << 16) + ((int64_t) src[srcPos++] << 8) + ((int64_t) src[srcPos++]);
		bytes dst = bytes(uncompressedSize);
		
		while(srcPos < src.size()) {
			int64_t b = src[srcPos];
			
			//offset copy
			if(b & 0b10000000) {
				//1ccccccc... oooooooo oooooooo
				int64_t nCopy = getCount(src, srcPos); //1-128
				
				if(nCopy == -1 || srcPos + 2 > src.size()) {
					return bytes();
				}
				
				int64_t offset = ((src[srcPos++] << 8) | src[srcPos++]) + 1; //1-65536
				
				//DEBUG
				//cout << dstPos << " " << nCopy << " " << offset << endl;
				
				if(dstPos + nCopy > dst.size() || offset > dstPos)  {
					return bytes();
				}
				
				//copy bytes from an earlier location in the decompressed output
				int64_t fromOffset = dstPos - offset;
				copyBytes(dst, fromOffset, dst, dstPos, nCopy);
				
			//literal copy
			} else {
				//0ppppppp...
				int64_t lit = getCount(src, srcPos);
				
				if(lit == -1 || srcPos + lit > src.size() || dstPos + lit > dst.size())  {
					return bytes();
				}
				
				copyBytes(src, srcPos, dst, dstPos, lit);
			}
		}
		
		return dst;
	}

}