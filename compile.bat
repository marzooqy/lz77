call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cl /EHsc /std:c++20 /openmp lz77.cpp

del lz77.obj

pause