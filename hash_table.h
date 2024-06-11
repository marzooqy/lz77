#include <cstdint>
#include <vector>
#include <iostream>

namespace lz77 {

	using std::vector;
	using std::cout, std::endl;

	typedef vector<unsigned char> bytes;

	const int64_t MIN_LENGTH = 4;

	#define ONES(n) ((uint64_t) 1 << n) - 1
	const int64_t OFFSETS[12] = {0, 0, 0, ONES(7), ONES(14), ONES(21), ONES(28), ONES(35), ONES(42), ONES(49), ONES(56), ONES(63)};

	//generic min
	template<typename T1, typename T2>
	T1 getMin(T1 a, T2 b) {
		return a <= b ? a : b;
	}

	struct Match {
		int64_t location;
		int64_t length;
		int64_t offset;
	};

	class Table {
		private:
			bytes& buf;
			int64_t lastPos = 0;

			vector<int32_t> map = vector<int32_t>(131072, -1);

			int64_t getHash(bytes& buf, int64_t pos) {
				return ((int64_t) buf[pos++] << 9) ^ ((int64_t) buf[pos++] << 6) ^ ((int64_t) buf[pos++] << 3) ^ buf[pos];
			};

		public:
			Table(bytes& buffer): buf(buffer) {}

			//add all bytes between lastPos and pos to the hash table
			int64_t addTo(int64_t pos) {
				int64_t prevPos = 0;

				for(lastPos; lastPos <= pos; lastPos++) {
					int64_t hash = getHash(buf, lastPos);
					prevPos = map[hash];
					map[hash] = lastPos;
				}

				return prevPos;
			}

			//find the longest matching pattern
			Match getLongestMatch(int64_t pos, int64_t lastLocation) {
				Match longestMatch = Match{0, 0, 0};
				int64_t prevPos = addTo(pos);

				if(prevPos != -1) {
					//search to the right
					int64_t rlen = 0;
					int64_t rPrevPos = prevPos;
					int64_t rPos = pos;
					int64_t maxLen = buf.size() - pos;

					while(rlen < maxLen && buf[rPrevPos++] == buf[rPos++]) {
						rlen++;
					}

					//optimization: search to the left of the current position
					//improves the compression ratio
					int64_t llen = 0;
					int64_t lPrevPos = prevPos - 1;
					int64_t lPos = pos - 1;

					//left match possible range: 0...lPrevPos, including 0
					//right match possible range: lastLocation...lPos, including lastLocation
					int64_t max_llen = getMin(lPrevPos - 0, lPos - lastLocation) + 1;

					while(llen < max_llen && buf[lPrevPos--] == buf[lPos--]) {
						llen++;
					}

					int64_t length = llen + rlen;
					int64_t location = pos - llen;
					int64_t offset = pos - prevPos;

					if(length > 5 || offset <= OFFSETS[length]) {
						longestMatch = Match{location, length, offset};
					}
				}

				return longestMatch;
			}
	};

}
