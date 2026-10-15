#!/bin/bash
# WorkBuddy 生产级改造项目的可复现构建脚本 (Windows / Git Bash / MSVC 2022)
#
# 用法:
#   bash scripts/wb_build.sh            # 配置 + 编译 model23dtiles (Release)
#   bash scripts/wb_build.sh all        # 编译全部目标
#   bash scripts/wb_build.sh test_3dtiles
#   bash scripts/wb_build.sh reconfigure
#
# 说明:
#   1) MSVC 编译器通过环境变量 INCLUDE/LIB 显式提供，避免依赖 vcvars64.bat。
#   2) 必须 unset 大小写重复的同名 proxy 变量 (https_proxy / HTTPS_PROXY)：
#      MSBuild 的 ToolTask 用区分大小写判等的 Hashtable 收集环境变量，
#      同名大小写不同的键会导致 System.ArgumentException (MSB6001)，
#      表现为 "No CMAKE_CXX_COMPILER could be found"。
#   3) VCPKG_MANIFEST_INSTALL=OFF + VCPKG_INSTALLED_DIR 指向仓库内已安装好的
#      vcpkg_installed，避免联网重新执行 vcpkg install。

set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VS="C:/Program Files/Microsoft Visual Studio/2022/Community"
MSVCVER="14.44.35207"
KITVER="10.0.26100.0"
KIT="C:/Program Files (x86)/Windows Kits/10"

export INCLUDE="$KIT/Include/$KITVER/ucrt;$KIT/Include/$KITVER/um;$KIT/Include/$KITVER/shared;$VS/VC/Tools/MSVC/$MSVCVER/include"
export LIB="$KIT/Lib/$KITVER/ucrt/x64;$KIT/Lib/$KITVER/um/x64;$VS/VC/Tools/MSVC/$MSVCVER/lib/x64"
export LIBPATH="$LIB"

# MSB6001 规避：去掉大小写重复的 proxy 变量
unset https_proxy http_proxy all_proxy ftp_proxy
unset HTTPS_PROXY HTTP_PROXY ALL_PROXY FTP_PROXY

# 注意：不能设置 MSYS_NO_PATHCONV=1。cmake.exe 是原生 Windows 程序，
# 依赖 Git Bash 把 "/e/..." 转换成 "E:/..."。改用 --parallel 而不是 "-- /m:8"
# 来避免 MSYS 把 "/m:8" 当路径转换后报 MSB1008。

BUILD_DIR="$ROOT/build_wb"
CFG="Release"
TARGET="${1:-model23dtiles}"

configure() {
    cmake -G "Visual Studio 17 2022" -A x64 \
        -DCMAKE_BUILD_TYPE="$CFG" \
        -DCMAKE_TOOLCHAIN_FILE=E:/SDK/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_MANIFEST_INSTALL=OFF \
        -DVCPKG_INSTALLED_DIR="$ROOT/vcpkg_installed" \
        -DVCPKG_TARGET_TRIPLET=x64-windows \
        -DOSGGISPLUGINS_BUILD_TESTS=ON \
        -S "$ROOT" -B "$BUILD_DIR"
}

if [ "$TARGET" = "reconfigure" ]; then
    rm -rf "$BUILD_DIR"
    configure || exit 1
    TARGET="model23dtiles"
fi

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    configure || exit 1
fi

if [ "$TARGET" = "all" ]; then
    cmake --build "$BUILD_DIR" --config "$CFG" --parallel 8
else
    cmake --build "$BUILD_DIR" --config "$CFG" --target "$TARGET" --parallel 8
fi
