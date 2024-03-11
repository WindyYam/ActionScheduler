Assuming you have ninja and gcc installed:

mkdir build
cd build
cmake -DCMAKE_MAKE_PROGRAM=C:/Ninja/ninja.exe -DCMAKE_C_COMPILER=C:/MySource/gcc-13.2.0-no-debug/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/MySource/gcc-13.2.0-no-debug/bin/g++.exe  -G Ninja ..
C:\Ninja\ninja.exe all
test_action_scheduler.exe