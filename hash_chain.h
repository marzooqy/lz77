#include <vector>
#include <iostream>
#include <string>

namespace lz77 {

	using namespace std;

	typedef unsigned int uint;
	typedef vector<unsigned char> bytes;
	
	const uint MAX_LENGTH = 131;
	const uint MAX_OFFSET = 65536;
	const uint CHAIN_LENGTH = 0x10000;
	const uint CHAIN_MASK = 0xFFFF;
	
	const uint GOOD_LENGTH = 32;
	const uint MAX_LOOPS = 32;

	template<typename T1, typename T2>
	T1 getMin(T1 a, T2 b) {
		return a <= b ? a : b;
	}

	struct Match {
		uint location;
		uint length;
		uint offset;
	};
	
	class Table {
		private:
			bytes& buf;
			uint lastPos = 0;

			/*head is a two-dimensional array containing with the index of each array being a byte from the buffer
			and the value is the last position where the bytes could be found*/
			vector<vector<uint>> head = vector<vector<uint>>(256, vector<uint>(256, 0xFFFFFFFF));

			/*prev is a an array where the index is the position masked by the length of the sliding window
			and the value is the previous position that contains the same bytes*/
			vector<uint> prev = vector<uint>(CHAIN_LENGTH);
			
			//get the previous position that contains the same bytes, or 0xFFFFFFFF if the position is invalid
			uint getPrevPos(uint prevPos, uint pos) {
				uint nextPrevPos = prev[prevPos & CHAIN_MASK];
				if(pos - nextPrevPos > MAX_OFFSET || nextPrevPos >= prevPos) {
					return 0xFFFFFFFF;
				}
				
				return nextPrevPos;
			}
			
		public:
			Table(bytes& buffer): buf(buffer) {}
			
			//add all bytes between lastPos and pos to the hash chain
			void addTo(uint pos) {
				for(lastPos; lastPos <= pos; lastPos++) {
					uint& prevPos = head[buf[lastPos]][buf[lastPos + 1]];
					prev[lastPos & CHAIN_MASK] = prevPos;
					prevPos = lastPos;
				}
			}
			
			//check past values and find the longest matching pattern
			Match getLongestMatch(uint pos) {
				addTo(pos);
				Match longestMatch = Match{0, 0, 0};
				
				//it's important to limit the number of loops here for the sake of speed
				uint prevPos = getPrevPos(pos, pos);
				uint n_loops = 0;
				
				while(prevPos != 0xFFFFFFFF && n_loops < MAX_LOOPS) {
					uint length = 2;
					uint maxLen = getMin(buf.size() - pos, MAX_LENGTH);
					
					//find out how long the match is
					//depending on the hashing function you might also need to check the first 2-3 bytes here
					while(length < maxLen && buf[prevPos + length] == buf[pos + length]) {
						length++;
					}
					
					uint offset = pos - prevPos;
					
					//check if the length and offset are valid
					if(length > longestMatch.length && length >= 4) {
						longestMatch = Match{pos, length, offset};
						
						if(length >= GOOD_LENGTH) {
							break;
						}
					}
					
					prevPos = getPrevPos(prevPos, pos);
					n_loops++;
				}
				
				return longestMatch;
			}
	};

}