# Cross-compile a Windows x64 .vst3 (DLL) from Linux/macOS with MinGW-w64:
#   cmake -S . -B build_win -G Ninja -DCMAKE_TOOLCHAIN_FILE=scripts/mingw-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
