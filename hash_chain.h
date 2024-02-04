#include <vector>
#include <iostream>
#include <string>

namespace lz77 {

	using namespace std;

	typedef unsigned int uint;
	typedef vector<unsigned char> bytes;
	
	const uint MIN_LENGTH = 3; //minimum possible match length
	const uint MAX_LENGTH = 1028; //maximum possible match length
	const uint GOOD_LENGTH = 3; //stop searching if a match of this length is found
	const uint MAX_LITERAL = 64; //maximum possible number of literals
	
	const uint MAX_OFFSET = 1 << 20; //max possible offset
	const uint MAX_LOOPS = 1; //number of past places to search, set to 1 to disable chaining
	const uint CHAIN_LENGTH = 1; //hash chain size, usually the same as MAX_OFFSET, set to 1 << x to enable chaining, set to 1 disable
	const uint CHAIN_MASK = 0; //set to CHAIN_LENGTH - 1 to enable chaining, set to 0 to disable
	
	const uint HASH_AHEAD = 3; //number of bytes to hash
	const uint HASH_SHIFT = 4; //how many bits to shift in the hash function
	const uint HASH_SIZE = 1 << 16; //hash table size/hash chain head size
	const uint HASH_MASK = HASH_SIZE - 1; //how many bits to keep in the hash value, intended to prevent overflowing the hash table

	//generic min
	template<typename T1, typename T2>
	T1 getMin(T1 a, T2 b) {
		return a <= b ? a : b;
	}

	struct Match {
		uint location;
		uint length;
		uint offset;
	};
	
	//computes the hash value
	class RollingHash {
		private:
			bytes& buf;
			uint lastHash = 0;
			uint lastPos = 0;
			
		public:
			RollingHash(bytes& buffer): buf(buffer) {
				addTo(HASH_AHEAD);
			};
			
			uint addTo(uint pos) {
				pos += HASH_AHEAD;
				
				//shift the hash and XOR with the next byte
				//the result is a hash computed from multiple bytes
				for(lastPos; lastPos < pos; lastPos++) {
					lastHash = ((lastHash << HASH_SHIFT) ^ buf[lastPos]) & HASH_MASK;
				}
				
				return lastHash;
			}
	};
	
	class Table {
		private:
			bytes& buf;
			RollingHash hash;
			uint lastPos = 0;

			/*head is an array where the index is a hash created from a number of bytes from the buffer
			and the value is the last position where the bytes could be found*/
			vector<uint> head = vector<uint>(HASH_SIZE, 0xFFFFFFFF);

			/*prev is an array where the index is the position masked by the length of the sliding window
			and the value is the previous position that evaluates to the same hash*/
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
			Table(bytes& buffer): buf(buffer), hash(RollingHash(buffer)) {}
			
			//add all bytes between lastPos and pos to the hash chain
			uint addTo(uint pos) {
				uint prevPos = 0;
				
				for(lastPos; lastPos <= pos; lastPos++) {
					uint lastHash = hash.addTo(lastPos);
					prev[lastPos & CHAIN_MASK] = prevPos = head[lastHash];
					head[lastHash] = lastPos;
				}
				
				if(pos - prevPos > MAX_OFFSET || prevPos >= pos) {
					return 0xFFFFFFFF;
				}
				
				return prevPos;
			}
			
			//check past values and find the longest matching pattern
			Match getLongestMatch(uint pos, uint lastLocation) {
				Match longestMatch = Match{0, 0, 0};
				
				//it's important to limit the number of loops here for the sake of speed
				uint prevPos = addTo(pos);
				uint n_loops = 0;
				
				while(prevPos != 0xFFFFFFFF && n_loops < MAX_LOOPS) {
					uint length = 0;
					uint llen = 0;
					
					if(buf[prevPos] != buf[pos]) {
						prevPos = getPrevPos(prevPos, pos);
						n_loops++;
						continue;
					}
					
					length++;
					
					//optimization: search to the left of the current position
					//improves the compression ratio
					if(prevPos > 0) {
						uint lPrevPos = prevPos - 1;
						uint lPos = pos - 1;
						
						while(lPrevPos > 0 && lPos > lastLocation && buf[lPrevPos--] == buf[lPos--]) {
							llen++;
						}
					}
					
					//search to the right
					length += llen;
					uint rPrevPos = prevPos + 1;
					uint rPos = pos + 1;
					uint maxLen = getMin(buf.size() - pos, MAX_LENGTH - length);
					
					while(length < maxLen && buf[rPrevPos++] == buf[rPos++]) {
						length++;
					}
					
					length = length > MAX_LENGTH ? MAX_LENGTH : length; //don't overflow the max length
					uint location = pos - llen;
					uint offset = pos - prevPos;
					
					//check if the length and offset are valid
					if(length > longestMatch.length && ((length >= 3 && offset <= 4096) || (length >= 4 && offset < 65536) || length >= 5)) {
						longestMatch = Match{location, length, offset};
						
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