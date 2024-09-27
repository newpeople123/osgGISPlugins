@echo off
if not exist "build" (
    mkdir build
)
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=E:/SDK/vcpkg/scripts/buildsystems/vcpkg.cmake -S . -B build
cmake --build build --config Release

