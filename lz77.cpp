#include "compression.h"
#include "omp.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace filesystem = std::filesystem;

using std::vector, std::string;
using std::fstream, std::ios;
using std::cout, std::endl, std::fixed, std::setprecision;

typedef vector<unsigned char> bytes;

const uint64_t BLOCK_SIZE = 1e7;

struct Index {
	uint32_t order;
	uint64_t location;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
};

uint64_t getInt(bytes& buf, uint64_t& pos, uint8_t size) {
	uint64_t n = buf[pos++];
	for(uint8_t i = 1; i < size; i++) {
		n |= buf[pos++] << i * 8;
	}
	
	return n;
}

void putInt(bytes& buf, uint64_t& pos, uint64_t n, uint8_t size) {
	for(uint8_t i = 0; i < size; i++) {
		buf[pos++] = n >> i * 8;
	}
}

//get the size of a file
uint64_t getFileSize(fstream& file) {
	uint64_t pos = file.tellg();
	file.seekg(0, ios::end);
	uint64_t size = file.tellg();
	file.seekg(pos, ios::beg);
	return size;
}

//read size bytes from the file and place them in a vector of unsigned chars
//if what's left in the file is less than size, then read what's left in the file only
bytes readFile(fstream& file, uint64_t size) {
	uint64_t fileSize = getFileSize(file);
	size = fileSize - file.tellg() < size ? fileSize - file.tellg() : size;
	bytes buf = bytes(size);
	file.read(reinterpret_cast<char*>(buf.data()), size);
	return buf;
}

//write buf to the file
void writeFile(fstream& file, bytes& buf) {
	file.write(reinterpret_cast<char*>(buf.data()), buf.size());
}

//try to delete a file, fails silently
void tryDelete(string fileName) {
	try { filesystem::remove(fileName); }
	catch(filesystem::filesystem_error) {}
}

int main(int argc, char* argv[]) {
	//parse args
	if(argc == 1) {
		cout << "No arguments provided" << endl;
		return 0;
	}
	
	string arg = argv[1];
	
	if(arg == "help") {
		cout << "lz77 -arg file" << endl;
		cout << "   -d  decompress" << endl;
		cout << endl;
		return 0;
	}
	
	bool decompress = false;
	int fileArgIndex = 1;
	
	if(arg == "-d") {
		decompress = true;
		fileArgIndex = 2;
	}
	
	if(fileArgIndex > argc - 1) {
		cout << "No file path provided" << endl;
		return 0;
	}
	
	string filePath = argv[fileArgIndex];
	string newFilePath;
	
	if(decompress) {
		if(!filePath.ends_with(".lz77")) {
			cout << "The file provided is not compressed with this too" << endl;
			return 0;
		}
		
		newFilePath = std::string(filePath.begin(), filePath.end() - 5);
		
	} else {
		newFilePath = filePath + ".lz77";
	}
	
	fstream file = fstream(filePath, ios::in | ios::binary);
	
	if(!file.is_open()) {
		cout << filePath << ": Failed to open file" << endl;
		return 0;
	}
	
	fstream newFile = fstream(newFilePath, ios::in | ios::out | ios::binary | ios::trunc);
	
	if(!newFile.is_open()) {
		cout << newFilePath << ": Failed to create new file" << endl;
		return 1;
	}
	
	vector<Index> index = vector<Index>();
	uint64_t fileSize = getFileSize(file);
	bool failed = false;
	
	omp_lock_t r_lock;
	omp_lock_t w_lock;
	
	omp_init_lock(&r_lock);
	omp_init_lock(&w_lock);
	
	if(!decompress) {
		//leave space for the header
		bytes zeros = bytes(12);
		writeFile(newFile, zeros);
		
		//read 10 MB at a time and compress it
		int64_t loops = fileSize / BLOCK_SIZE;
		if(fileSize % BLOCK_SIZE != 0) {
			loops++;
		}
		
		#pragma omp parallel for if(fileSize > BLOCK_SIZE)
		for(int64_t i = 0; i < loops; i++) {
			if(!failed) {
				omp_set_lock(&r_lock);
				file.seekg(i * BLOCK_SIZE);
				bytes src = readFile(file, BLOCK_SIZE);
				omp_unset_lock(&r_lock);
				
				bytes dst = lz77::compress(src);
				
				if(dst.size() > 0) {
					omp_set_lock(&w_lock);
					index.push_back(Index{(uint32_t) i, (uint64_t) newFile.tellp(), (uint32_t) dst.size(), (uint32_t) src.size()});
					writeFile(newFile, dst);
					omp_unset_lock(&w_lock);

				} else {
					#pragma omp critical
					failed = true;
				}
			}
		}
		
		if(failed) {
			cout << "Compression failed" << endl;
			tryDelete(newFilePath);
			return 1;
		}
		
		//write the index
		uint64_t index_location = newFile.tellp();
		
		bytes buffer = bytes(index.size() * 18);
		uint64_t pos = 0;
		
		for(auto& entry: index) {
			putInt(buffer, pos, entry.order, 4);
			putInt(buffer, pos, entry.location, 8);
			putInt(buffer, pos, entry.compressedSize, 3);
			putInt(buffer, pos, entry.uncompressedSize, 3);
		}
		
		writeFile(newFile, buffer);
		
		//write the header
		newFile.seekp(0);
		
		buffer = bytes(12);
		pos = 0;
		
		putInt(buffer, pos, index_location, 8);
		putInt(buffer, pos, index.size(), 4);
		
		writeFile(newFile, buffer);
		
	} else {
		//read the header
		if(fileSize <= 12) {
			cout << "File size is smaller than expected" << endl;
			tryDelete(newFilePath);
			return 1;
		}
		
		bytes buffer = readFile(file, 12);
		uint64_t pos = 0;
		uint64_t index_location = getInt(buffer, pos, 8);
		uint64_t index_count = getInt(buffer, pos, 4);
		
		//read the index
		if(index_location + index_count * 18 > fileSize) {
			cout << "Index out of bounds" << endl;
			tryDelete(newFilePath);
			return 1;
		}
		
		file.seekg(index_location);
		buffer = readFile(file, index_count * 18);
		pos = 0;
		
		for(uint64_t i = 0; i < index_count; i++) {
			index.push_back(Index{(uint32_t) getInt(buffer, pos, 4), (uint64_t) getInt(buffer, pos, 8), (uint32_t) getInt(buffer, pos, 3), (uint32_t) getInt(buffer, pos, 3)});
		}
		
		std::sort(index.begin(), index.end(), [](Index e1, Index e2) {return e1.order < e2.order;});
		
		//decompress the blocks
		#pragma omp parallel for ordered if(index.size() > 1)
		for(int64_t i = 0; i < index.size(); i++) {
			if(!failed) {
				auto& entry = index[i];
				
				omp_set_lock(&r_lock);
				file.seekg(entry.location);
				bytes src = readFile(file, entry.compressedSize);
				omp_unset_lock(&r_lock);
				
				bytes dst = bytes(entry.uncompressedSize);
				lz77::decompress(src, dst);
				
				if(dst.size() == 0) {
					failed = true;
				}
				
				#pragma omp ordered
				writeFile(newFile, dst);
			}
		}
		
		if(failed) {
			cout << "Decompression failed" << endl;
			tryDelete(newFilePath);
			return 1;
		}
	}
	
	omp_destroy_lock(&r_lock);
	omp_destroy_lock(&w_lock);
	
	float old_size = filesystem::file_size(filePath) / 1024.0;
	float new_size = filesystem::file_size(newFilePath) / 1024.0;
	
	cout << filePath << " " << fixed << setprecision(2);
	
	if(old_size >= 1000) {
		cout << old_size / 1024.0 << " MB";
	} else {
		cout << old_size << " KB";
	}
	
	cout << " -> ";
	
	if(new_size >= 1000) {
		cout << new_size / 1024.0 << " MB";
	} else {
		cout << new_size << " KB";
	}
	
	cout << endl << endl;
	
	return 0;
}