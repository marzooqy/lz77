#include "compression.h"

#include <fcntl.h>
#include <io.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

typedef unsigned int uint;
typedef vector<unsigned char> bytes;

uint getFileSize(fstream& file) {
	uint pos = file.tellg();
	file.seekg(0, ios::end);
	uint size = file.tellg();
	file.seekg(pos, ios::beg);
	return size;
}

bytes readFile(fstream& file, uint size) {
	uint fileSize = getFileSize(file);
	size = fileSize - file.tellg() < size ? fileSize - file.tellg() : size;
	bytes buf = bytes(size);
	file.read(reinterpret_cast<char*>(buf.data()), size);
	return buf;
}

void writeFile(fstream& file, bytes& buf) {
	file.write(reinterpret_cast<char*>(buf.data()), buf.size());
}

//trys to delete a file, fails silently
void tryDelete(wstring fileName) {
	try { filesystem::remove(fileName); }
	catch(filesystem::filesystem_error) {}
}

//using wide chars and wide strings to support UTF-16 file names
int wmain(int argc, wchar_t *argv[]) {
	_setmode(_fileno(stdout), _O_U16TEXT); //fix for wcout
	
	if(argc == 1) {
		wcout << L"No arguments provided" << endl;
		return 0;
	}
	
	//parse args
	wstring arg = argv[1];
	
	if(arg == L"help") {
		wcout << L"lz77.exe -args file" << endl;
		wcout << L"   -d  decompress" << endl;
		wcout << endl;
		return 0;
	}
	
	bool decompress = false;
	int fileArgIndex = 1;
	
	if(arg == L"-d") {
		decompress = true;
		fileArgIndex = 2;
	}
	
	if(fileArgIndex > argc - 1) {
		wcout << L"No file path provided" << endl;
		return 0;
	}
	
	wstring filePath = argv[fileArgIndex];
	wstring newFilePath;
	
	if(decompress) {
		if(!filePath.ends_with(L".lz77")) {
			wcout << L"The file provided is not compressed with this tool" << endl;
			return 0;
		}
		
		newFilePath = std::wstring(filePath.begin(), filePath.end() - 5);
		
	} else {
		newFilePath = filePath + L".lz77";
	}
	
	fstream file = fstream(filePath, ios::in | ios::binary);
	
	if(!file.is_open()) {
		wcout << filePath << L": Failed to open file" << endl;
		return 0;
	}
	
	fstream newFile = fstream(newFilePath, ios::in | ios::out | ios::binary | ios::trunc);
	
	if(newFile.is_open()) {
		uint fileSize = getFileSize(file);
		
		if(!decompress) {
			while(file.tellg() < fileSize) {
				bytes src = readFile(file, 1e7);
				bytes dst = lz77::compress(src);
				
				if(dst.size() == 0) {
					wcout << L"Compression failed" << endl;
					file.close();
					newFile.close();
					tryDelete(newFilePath);
					return 0;
				}
				
				writeFile(newFile, dst);
			}
			
		} else {
			while(file.tellg() < fileSize) {
				bytes buf = readFile(file, 3);
				uint size = ((uint) buf[0] << 16) + ((uint) buf[1] << 8) + ((uint) buf[2]);
				
				bytes src = readFile(file, size);
				bytes dst = lz77::decompress(src);
				
				if(dst.size() == 0) {
					wcout << L"Decompression failed" << endl;
					file.close();
					newFile.close();
					tryDelete(newFilePath);
					return 0;
				}
				
				writeFile(newFile, dst);
			}
		}
		
	} else {
		wcout << newFilePath << L": Failed to create new file" << endl;
		file.close();
		return 0;
	}
	
	file.close();
	newFile.close();
	
	float old_size = filesystem::file_size(filePath) / 1024.0;
	float new_size = filesystem::file_size(newFilePath) / 1024.0;
	
	wcout << filePath << L" " << fixed << setprecision(2);
	
	if(old_size >= 1000) {
		wcout << old_size / 1024.0 << L" MB";
	} else {
		wcout << old_size << L" KB";
	}
	
	wcout << " -> ";
	
	if(new_size >= 1000) {
		wcout << new_size / 1024.0 << L" MB";
	} else {
		wcout << new_size << L" KB";
	}
	
	wcout << endl << endl;
	return 0;
}