#include <cstdint>
#include <vector>
#include <iostream>

namespace lz77 {

    using std::vector;
    using std::cout, std::endl;

    typedef vector<unsigned char> bytes;

    const uint32_t MIN_LENGTH = 4;
    const uint32_t MAX_LENGTH = 127;
    const uint32_t MAX_LITERAL = 127;
    const uint32_t MAX_OFFSET = 65535;

    //generic min
    template<typename T1, typename T2>
    T1 getMin(T1 a, T2 b) {
        return a <= b ? a : b;
    }

    struct Match {
        uint32_t location;
        uint32_t length;
        uint32_t offset;
    };

    class Table {
        private:
            bytes& buf;
            uint32_t lastPos = 0;

            vector<uint32_t> map = vector<uint32_t>(131072, -1);

            uint32_t getHash(bytes& buf, uint32_t pos) {
                return ((uint32_t) buf[pos++] << 9) ^ ((uint32_t) buf[pos++] << 6) ^ ((uint32_t) buf[pos++] << 3) ^ buf[pos];
            };

        public:
            Table(bytes& buffer): buf(buffer) {}

            //add all bytes between lastPos and pos to the hash table
            uint32_t addTo(uint32_t pos) {
                uint32_t prevPos = 0;

                for(lastPos; lastPos <= pos; lastPos++) {
                    uint32_t hash = getHash(buf, lastPos);
                    prevPos = map[hash];
                    map[hash] = lastPos;
                }

                return prevPos;
            }

            //find the longest matching pattern
            Match getLongestMatch(uint32_t pos, uint32_t lastLocation) {
                Match longestMatch = Match{0, 0, 0};
                uint32_t prevPos = addTo(pos);
                uint32_t offset = pos - prevPos;

                if(prevPos != -1 && offset <= MAX_OFFSET) {
                    //search to the right
                    uint32_t rlen = 0;
                    uint32_t rPrevPos = prevPos;
                    uint32_t rPos = pos;
                    uint32_t maxLen = buf.size() - pos;

                    while(rlen < maxLen && buf[rPrevPos++] == buf[rPos++]) {
                        rlen++;
                    }

                    //optimization: search to the left of the current position
                    //improves the compression ratio
                    uint32_t llen = 0;

                    if(rlen > 0 && prevPos > 0 && pos > lastLocation) {
                        //left match possible range: [0, prevPos)
                        //right match possible range: [lastLocation, pos)
                        uint32_t max_llen = getMin(prevPos - 0, pos - lastLocation);
                        uint32_t lPrevPos = prevPos - 1;
                        uint32_t lPos = pos - 1;
                        while(llen < max_llen && buf[lPrevPos--] == buf[lPos--]) {
                            llen++;
                        }
                    }

                    uint32_t length = llen + rlen;
                    uint32_t location = pos - llen;

                    if(length > MAX_LENGTH) {
                        length -= length % MIN_LENGTH;
                    }

                    if(length >= MIN_LENGTH) {
                        longestMatch = Match{location, length, offset};
                    }
                }

                return longestMatch;
            }
    };

}
