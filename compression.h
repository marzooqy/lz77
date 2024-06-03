#include "hash_table.h"

#include <cstdint>
#include <vector>
#include <iostream>

namespace lz77 {

	using std::vector;
	using std::cout, std::endl;
	
	typedef vector<unsigned char> bytes;

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
		bytes dst = bytes(6 + src.size() + src.size() / MAX_LITERAL + 1); //likely the maximum possible size of the resulting compressed block
		Table table = Table(src);
		
		int64_t srcPos = 0;
		int64_t dstPos = 0;
		
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
			while(lit > 0) {
				lit = getMin(lit, MAX_LITERAL);
				
				if(dstPos + lit + 1 > dst.size()) {
					return bytes();
				}
				
				//0ppppppp
				dst[dstPos++] = lit - 1;
				
				copyBytes(src, srcPos, dst, dstPos, lit);
				
				lit = match.location - srcPos;
			}
			
			//offset copy
			int64_t nCopy = match.length;
			int64_t offset = match.offset - 1; //subtraction is a part of the encoding for the offset
			int64_t matchEnd = match.location + match.length;
			
			while(nCopy > 0) {
				nCopy = getMin(nCopy, MAX_LENGTH);
				
				if(dstPos + 3 > dst.size())  {
					return bytes();
				}

				//1ccccccc oooooooo oooooooo
				dst[dstPos++] = 0b10000000 | (nCopy - 4);
				dst[dstPos++] = offset >> 8;
				dst[dstPos++] = offset;
				
				srcPos += nCopy;
				nCopy = matchEnd - srcPos;
			} 
		}
		
		//copy the remaining bytes at the end
		int64_t lit = src.size() - srcPos;
		
		while(lit > 0) {
			lit = getMin(lit, MAX_LITERAL);
			
			if(dstPos + lit + 1 > dst.size()) {
				return bytes();
			}
			
			//0ppppppp
			dst[dstPos++] = lit - 1;
			
			copyBytes(src, srcPos, dst, dstPos, lit);
			
			lit = src.size() - srcPos;
		}
		
		dst.resize(dstPos);
		return dst;
	}

	//decompresses src, returns an empty vector if decompression fails
	//it should only fail on broken compressed files
	bytes decompress(bytes& src, bytes& dst) {
		int64_t srcPos = 0;
		int64_t dstPos = 0;
		
		while(srcPos < src.size()) {
			int64_t b = src[srcPos++];
			
			//offset copy
			if(b & 0b10000000) {
				if(srcPos + 2 > src.size()) {
					return bytes();
				}
				
				//1ccccccc oooooooo oooooooo
				int64_t nCopy = (b & 0b01111111) + 4; //4-131
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
				//0ppppppp
				int64_t lit = b + 1; //1-128
				
				if(srcPos + lit > src.size() || dstPos + lit > dst.size())  {
					return bytes();
				}
				
				copyBytes(src, srcPos, dst, dstPos, lit);
			}
		}
		
		return dst;
	}

}
