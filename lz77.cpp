#include "compression.h"

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

typedef vector<uint8_t> bytes;

const int64_t BLOCK_SIZE = 1e7;

//get the size of a file
int64_t getFileSize(fstream& file) {
	int64_t pos = file.tellg();
	file.seekg(0, ios::end);
	int64_t size = file.tellg();
	file.seekg(pos, ios::beg);
	return size;
}

//read size bytes from the file and place them in a vector of unsigned chars
//if what's left in the file is less than size, then read what's left in the file only
bytes readFile(fstream& file, int64_t size) {
	int64_t fileSize = getFileSize(file);
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
	
	if(newFile.is_open()) {
		int64_t fileSize = getFileSize(file);
		bool failed = false;
		
		if(!decompress) {
			//read 10 MB at a time and compress it
			#pragma omp parallel for ordered if(fileSize > BLOCK_SIZE)
			for(int i = 0; i < fileSize; i += BLOCK_SIZE) {
				if(!failed) {
					bytes src;
					
					#pragma omp ordered
					{ src = readFile(file, BLOCK_SIZE); }
					
					bytes dst = lz77::compress(src);
					
					if(dst.size() == 0) {
						#pragma omp critical
						failed = true;
					}
					
					#pragma omp ordered
					writeFile(newFile, dst);
				}
			}
			
			if(failed) {
				cout << "Compression failed" << endl;
				file.close();
				newFile.close();
				tryDelete(newFilePath);
				return 0;
			}
			
		} else {
			//read the size of the compressed block (first 3 bytes), then read the compressed block and decompress it
			while(file.tellg() < fileSize) {
				bytes buf = readFile(file, 3);
				int64_t size = ((int64_t) buf[0] << 16) + ((int64_t) buf[1] << 8) + ((int64_t) buf[2]);
				
				bytes src = readFile(file, size);
				bytes dst = lz77::decompress(src);
				
				if(dst.size() == 0) {
					cout << "Decompression failed" << endl;
					file.close();
					newFile.close();
					tryDelete(newFilePath);
					return 0;
				}
				
				writeFile(newFile, dst);
			}
		}
		
	} else {
		cout << newFilePath << ": Failed to create new file" << endl;
		file.close();
		return 0;
	}
	
	file.close();
	newFile.close();
	
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