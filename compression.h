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
	//ideally the failure scenario would never occur
	bytes compress(bytes& src) {
		bytes dst = bytes(6 + src.size() + src.size() / MAX_LITERAL + 1); //likely the maximum possible size of the resulting compressed block
		Table table = Table(src);
		
		uint srcPos = 0;
		uint dstPos = 6; //leave room for the uncompressed size and the compressed size
		
		//lit = number of bytes to copy from src with no compression
		//nCopy = number of bytes to compress
		//offset = offset from the current location in the decompressed input to where the pattern could be found
		
		uint i = 1;
		uint lastMatchLocation = 0;
		
		while(i <= src.size() - MIN_LENGTH) {
			Match match = table.getLongestMatch(i, lastMatchLocation);
			
			if(match.length >= MIN_LENGTH) {
				i = match.location + match.length;
				lastMatchLocation = i;
				
			} else {
				i++;
				continue;
			}
			
			//copy bytes from src to dst until the location of the match is reached
			uint lit = match.location - srcPos;
			
			//literal
			//i.e. no compression/no match found
			while(lit > 0) {
				//max possible value is MAX_LITERAL
				lit = getMin(lit, MAX_LITERAL);
				
				//don't overflow the buffer
				if(dstPos + lit + 1 > dst.size()) {
					return bytes();
				}
				
				//00pppppp
				dst[dstPos++] = lit - 1;
				
				copyBytes(src, srcPos, dst, dstPos, lit);
				
				lit = match.location - srcPos;
			}
			
			uint nCopy = match.length;
			uint offset = match.offset - 1; //subtraction is a part of the encoding for the offset
			
			//short
			//shorts enable 3 byte matches which improves the compression ratio
			if(nCopy <= 6 && offset < 4096) {
				if(dstPos + 2 > dst.size())  {
					return bytes();
				}
				
				//01ccoooo oooooooo
				dst[dstPos++] = 0b01000000 | ((nCopy - 3) << 4) | (offset >> 8);
				dst[dstPos++] = offset;
				
			//medium
			} else if(nCopy <= 67 && offset < 65536) {
				if(dstPos + 3 > dst.size())  {
					return bytes();
				}
				
				//10cccccc oooooooo oooooooo
				dst[dstPos++] = 0b10000000 | (nCopy - 4);
				dst[dstPos++] = offset >> 8;
				dst[dstPos++] = offset;
				
			//long
			//useful for files with lots of redundancies
			} else {
				if(dstPos + 4 > dst.size())  {
					return bytes();
				}
				
				//11cccccc ccccoooo oooooooo oooooooo
				dst[dstPos++] = 0b11000000 | ((nCopy - 5) >> 4);
				dst[dstPos++] = ((nCopy - 5) << 4) | (offset >> 16);
				dst[dstPos++] = offset >> 8;
				dst[dstPos++] = offset;
			}
			
			srcPos += nCopy;
		}
		
		//copy the remaining bytes at the end
		uint lit = src.size() - srcPos;
		while(lit > 0) {
			//max possible value is MAX_LITERAL
			lit = getMin(lit, MAX_LITERAL);
			
			if(dstPos + lit + 1 > dst.size()) {
				return bytes();
			}
			
			//00pppppp
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
	//it should only fail on broken compressed files
	bytes decompress(bytes& src) {
		uint srcPos = 0;
		uint dstPos = 0;
		
		uint uncompressedSize = ((uint) src[srcPos++] << 16) + ((uint) src[srcPos++] << 8) + ((uint) src[srcPos++]);
		bytes dst = bytes(uncompressedSize);
		
		uint b0, b1, b2, b3; //control characters
		
		while(srcPos < src.size()) {
			b0 = src[srcPos++];
			
			//offset copy
			if(b0 & 0b11000000) {
				uint nCopy;
				uint offset;
				
				//short
				if((b0 & 0b10000000) && (b0 & 0b01000000)) {
					if(srcPos + 3 > src.size()) {
						return bytes();
					}
					
					b1 = src[srcPos++];
					b2 = src[srcPos++];
					b3 = src[srcPos++];
					
					//11cccccc ccccoooo oooooooo oooooooo
					nCopy = (((b0 & 0b00111111) << 4) | ((b1 & 0b11110000) >> 4)) + 5; //5-1028
					offset = (((b1 & 0b00001111) << 16) | (b2 << 8) | b3) + 1; //1-1048576
					
				//medium
				} else if(b0 & 0b10000000) {
					if(srcPos + 2 > src.size()) {
						return bytes();
					}
					
					b1 = src[srcPos++];
					b2 = src[srcPos++];
					
					//10cccccc oooooooo oooooooo
					nCopy = (b0 & 0b00111111) + 4; //4-131
					offset = ((b1 << 8) | b2) + 1; //1-65536
					
				//long
				} else {
					if(srcPos + 1 > src.size()) {
						return bytes();
					}
					
					b1 = src[srcPos++];
					
					//01ccoooo oooooooo
					nCopy = ((b0 & 0b00110000) >> 4) + 3; //3-6
					offset = (((b0 & 0b00001111) << 8) | b1) + 1; //1-4096
				}
				
				if(dstPos + nCopy > dst.size() || offset > dstPos)  {
					return bytes();
				}
				
				//copy bytes from an earlier location in the decompressed output
				uint fromOffset = dstPos - offset;
				copyBytes(dst, fromOffset, dst, dstPos, nCopy);
				
			//literal copy
			} else {
				//00pppppp
				uint lit = b0 + 1; //1-64
				
				if(srcPos + lit > src.size() || dstPos + lit > dst.size())  {
					return bytes();
				}
				
				copyBytes(src, srcPos, dst, dstPos, lit);
			}
		}
		
		return dst;
	}

}