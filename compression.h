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
    void copyBytes(bytes& src, uint32_t& srcPos, bytes& dst, uint32_t& dstPos, uint32_t length) {
        for(uint32_t i = 0; i < length; i++) {
            dst[dstPos++] = src[srcPos++];
        }
    }

    //compresse src
    bytes compress(bytes& src) {
        bytes dst = bytes(3 + src.size() + src.size() / MAX_LITERAL + 1);
        Table table = Table(src);

        uint32_t srcPos = 0;
        uint32_t dstPos = 3;

        uint32_t i = 0;
        uint32_t lastMatchLocation = 0;

        while(i <= src.size() - MIN_LENGTH) {
            Match match = table.getLongestMatch(i, lastMatchLocation);

            //DEBUG
            //cout << match.location << " " << match.length << " " << match.offset << endl;

            if(match.length >= MIN_LENGTH) {
                //copy bytes from src to dst until the location of the match is reached
                uint32_t lit = match.location - srcPos;

                //literal copy
                //i.e. no compression/no match found
                while(lit > MAX_LITERAL) {
                    dst[dstPos++] = 0x7f;
                    copyBytes(src, srcPos, dst, dstPos, MAX_LITERAL);

                    lit -= MAX_LITERAL;
                }

                if(lit > 0) {
                    dst[dstPos++] = lit;
                    copyBytes(src, srcPos, dst, dstPos, lit);
                }

                //offset copy
                uint32_t len = match.length;
                uint32_t offset = match.offset;

                while(len > MAX_LENGTH) {
                    dst[dstPos++] = 0xff;
                    dst[dstPos++] = offset >> 8;
                    dst[dstPos++] = offset;

                    len -= MAX_LENGTH;
                }

                if(len > 0) {
                    dst[dstPos++] = 0b10000000 | len;
                    dst[dstPos++] = offset >> 8;
                    dst[dstPos++] = offset;
                }

                srcPos += match.length;

                i = match.location + match.length;
                lastMatchLocation = i;

            } else {
                i++;
            }
        }

        //copy the remaining bytes at the end
        uint32_t lit = src.size() - srcPos;

        if(lit > 0) {
            dst[dstPos++] = lit;
            copyBytes(src, srcPos, dst, dstPos, lit);
        }

        //write the uncompressed size at the start
        uint32_t uncompressedSize = src.size();
        dst[0] = uncompressedSize >> 16;
        dst[1] = uncompressedSize >> 8;
        dst[2] = uncompressedSize;

        dst.resize(dstPos);
        return dst;
    }

    //decompress src, return an empty vector if decompression fails
    //it should only fail on broken compressed files
    bytes decompress(bytes& src) {
        uint32_t srcPos = 3;
        uint32_t dstPos = 0;

        bytes dst = bytes(((uint32_t) src[0] << 16) | ((uint32_t) src[1] << 8) | src[2]);

        while(srcPos < src.size()) {
            uint32_t b = src[srcPos++];

            //offset copy
            if(b & 0b10000000) {
                if(srcPos + 2 > src.size()) {
                    return bytes();
                }

                uint32_t len = b & 0b01111111;
                uint32_t offset = ((uint32_t) src[srcPos++] << 8) + src[srcPos++];

                //DEBUG
                //cout << dstPos << " " << len << " " << offset << endl;

                if(dstPos + len > dst.size() || offset > dstPos)  {
                    return bytes();
                }

                //copy bytes from an earlier location in the decompressed output
                uint32_t fromOffset = dstPos - offset;
                copyBytes(dst, fromOffset, dst, dstPos, len);

            //literal copy
            } else {
                uint32_t lit = b;

                if(srcPos + lit > src.size() || dstPos + lit > dst.size())  {
                    return bytes();
                }

                copyBytes(src, srcPos, dst, dstPos, lit);
            }
        }

        return dst;
    }

}
