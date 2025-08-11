cmake_minimum_required(VERSION 3.12)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 打开 Visual Studio 文件夹分组支持
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 设置项目根目录变量
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "目标操作系统(target system name): ${CMAKE_SYSTEM_NAME}")
# 设置平台相关参数
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_compile_options("/utf-8")
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
    set(DEFAULT_TOOLCHAIN_PATH "E:/SDK/vcpkg/scripts/buildsystems/vcpkg.cmake")
else()
    set(VCPKG_TARGET_TRIPLET "x64-linux-dynamic" CACHE STRING "Vcpkg target triplet")
    set(DEFAULT_TOOLCHAIN_PATH "/home/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

# 设置 toolchain
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${DEFAULT_TOOLCHAIN_PATH}")
endif()

# 引入 Vcpkg 工具链
include(${CMAKE_TOOLCHAIN_FILE})

# 设置头文件全局包含路径（如不推荐全局 include，可转为 target_include_directories）
include_directories(
    ${PROJECT_ROOT_DIR}/3rdparty/include
    ${PROJECT_ROOT_DIR}/include
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/Debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/Debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/Debug)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/Release)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/RelWithDebInfo)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/RelWithDebInfo)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/RelWithDebInfo)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/MinSizeRel)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/MinSizeRel)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/MinSizeRel)

# 设置生成文件后缀（可选）
set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rel")
set(CMAKE_MINSIZEREL_POSTFIX "_min")

# 构建共享库
set(BUILD_SHARED_LIBS ON)

project(osgGISPlugins)

# 添加子模块
add_subdirectory(src)
add_subdirectory(tools)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
# 拷贝插件和资源文件（仅 Windows）
    set(VCPKG_INSTALL_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows")
    set(PLUGINS_DIR "${VCPKG_INSTALL_DIR}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB THIRD_PARTY_PLUGINS "${PLUGINS_DIR}/debug/plugins/osgPlugins-3.6.5/*")
        set(OUT_DIR "${PROJECT_ROOT_DIR}/Debug")
    else()
        file(GLOB THIRD_PARTY_PLUGINS "${PLUGINS_DIR}/plugins/osgPlugins-3.6.5/*")
        set(OUT_DIR "${PROJECT_ROOT_DIR}/Release")
    endif()

    file(COPY ${THIRD_PARTY_PLUGINS} DESTINATION ${OUT_DIR})
    file(COPY ${VCPKG_INSTALL_DIR}/bin/jpeg62.dll DESTINATION ${OUT_DIR})
    file(COPY ${PROJECT_ROOT_DIR}/assets/favicon.png DESTINATION ${OUT_DIR})
    file(COPY ${PROJECT_ROOT_DIR}/assets/msyh.ttc DESTINATION ${OUT_DIR})
elseif(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
endif()

# 打印关键路径
message(STATUS "本机操作系统(current system name): ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "目标操作系统(target system name): ${CMAKE_SYSTEM_NAME}")
message(STATUS "查找依赖包路径(cmake prefix path): ${CMAKE_PREFIX_PATH}")
message(STATUS "自定义模块路径(camke module path): ${CMAKE_MODULE_PATH}")
message(STATUS "程序安装路径(install directory): ${CMAKE_INSTALL_PREFIX}")
