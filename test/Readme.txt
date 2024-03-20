Assuming you have ninja and gcc installed:

cmake -DCMAKE_MAKE_PROGRAM=C:/Ninja/ninja.exe -DCMAKE_C_COMPILER=C:/MySource/gcc-13.2.0-no-debug/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/MySource/gcc-13.2.0-no-debug/bin/g++.exe  -G Ninja -B ./build ./build
cmake --build ./build
./build/test_action_scheduler.exe