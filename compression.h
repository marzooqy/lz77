#include "hash_chain.h"

#include <vector>
#include <iostream>
#include <string>

namespace lz77 {

	using namespace std;

	typedef unsigned int uint;
	typedef vector<unsigned char> bytes;

	//copy length bytes from src at srcPos to dst at dstPos and increment srcPos and dstPos
	//copying one byte at a time is REQUIRED in some cases, otherwise decompression will not work
	void copyBytes(bytes& src, uint& srcPos, bytes& dst, uint& dstPos, uint length) {
		for(uint i = 0; i < length; i++) {
			dst[dstPos++] = src[srcPos++];
		}
	}

	//compresses src, returns an empty vector if compression fails
	//it will fail if the compressed output >= decompressed input since it's better to store these assets uncompressed
	//this typically happens with very small assets
	bytes compress(bytes& src) {
		bytes dst = bytes(src.size() + src.size() / 128 + 1 + 6);
		Table table = Table(src);
		
		uint srcPos = 0;
		uint dstPos = 6;
		
		//lit = number of bytes to copy from src as is with no compression
		//nCopy = number of bytes to compress
		//offset = offset from current location in decompressed input to where the pattern could be found
		
		uint i = 0;
		while(i <= src.size() - 4) {
			Match match = table.getLongestMatch(i);
			
			if(match.length >= 4) {
				i = match.location + match.length;
			} else {
				i++;
				continue;
			}
			
			//copy bytes from src to dst until the location of the match is reached
			uint lit = match.location - srcPos;
			
			while(lit > 0) {
				//max possible value is 128
				lit = getMin(lit, 128);
				
				if(dstPos + lit + 1 > dst.size()) {
					return bytes();
				}
				
				//0ppppppp
				dst[dstPos++] = lit - 1;
				
				copyBytes(src, srcPos, dst, dstPos, lit);
				
				lit = match.location - srcPos;
			}
			
			uint nCopy = match.length;
			uint offset = match.offset - 1;
			
			if(dstPos + 3 > dst.size())  {
				return bytes();
			}
			
			//1ccccccc oooooooo oooooooo
			dst[dstPos++] = 0b10000000 + (nCopy - 4);
			dst[dstPos++] = offset >> 8;
			dst[dstPos++] = offset;
			
			srcPos += nCopy;
		}
		
		//copy the remaining bytes at the end
		uint lit = src.size() - srcPos;
		while(lit > 0) {
			//max possible value is 128
			lit = getMin(lit, 128);
			
			if(dstPos + lit + 1 > dst.size()) {
				return bytes();
			}
			
			//0ppppppp
			dst[dstPos++] = lit - 1;
			
			copyBytes(src, srcPos, dst, dstPos, lit);
			
			lit = src.size() - srcPos;
		}
		
		//make compression header
		uint compressedSize = dstPos - 3;
		uint uncompressedSize = src.size();
		
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
	//it should only fail on broken compressed assets
	bytes decompress(bytes& src) {
		uint srcPos = 0;
		uint dstPos = 0;
		
		uint uncompressedSize = ((uint) src[srcPos++] << 16) + ((uint) src[srcPos++] << 8) + ((uint) src[srcPos++]);
		bytes dst = bytes(uncompressedSize);
		
		uint b0, b1, b2; //control characters
		
		while(srcPos < src.size()) {
			b0 = src[srcPos++];
			
			if((b0 & 0b10000000) > 0) {
				if(srcPos + 2 > src.size()) {
					return bytes();
				}
				
				b1 = src[srcPos++];
				b2 = src[srcPos++];
				
				//1ccccccc oooooooo oooooooo
				uint nCopy = (b0 & 0b01111111) + 4; //4-131
				uint offset = (b1 << 8) + b2 + 1; //1-65536
				
				if(dstPos + nCopy > dst.size() || offset > dstPos)  {
					return bytes();
				}
				
				//copy bytes from an earlier location in the decompressed output
				uint fromOffset = dstPos - offset;
				copyBytes(dst, fromOffset, dst, dstPos, nCopy);
				
			} else {
				//0ppppppp
				uint lit = b0 + 1; //1-128
				
				if(srcPos + lit > src.size() || dstPos + lit > dst.size())  {
					return bytes();
				}
				
				copyBytes(src, srcPos, dst, dstPos, lit);
			}
		}
		
		return dst;
	}

}