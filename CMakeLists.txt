cmake_minimum_required(VERSION 3.12)
# 打开允许创建文件夹的开关
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 设置C++ 标准版本,全局指定一次
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    add_compile_options("/utf-8")
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
else()
    set(VCPKG_TARGET_TRIPLET "x64-linux-dynamic" CACHE STRING "Vcpkg target triplet")
    # linux环境下c语言编译器路径，使用Dockerfile时需要注释掉
    # set(CMAKE_C_COMPILER /opt/rh/devtoolset-11/root/usr/bin/gcc)
    # linux环境下c++编译器路径，使用Dockerfile时需要注释掉
    # set(CMAKE_CXX_COMPILER /opt/rh/devtoolset-11/root/usr/bin/g++)
endif()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
        set(CMAKE_TOOLCHAIN_FILE C:/wty/work/test/vcpkg/scripts/buildsystems/vcpkg.cmake)
    else()
        set(CMAKE_TOOLCHAIN_FILE /home/vcpkg/scripts/buildsystems/vcpkg.cmake)
    endif()
endif()
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# 包含外部工具链文件,全局指定一次
include(${CMAKE_TOOLCHAIN_FILE})
# 设置全局包含路径，必须放在添加子目录之前,全局指定一次
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${PROJECT_ROOT_DIR}/include)
file(GLOB_RECURSE THREEDPARTY ${PROJECT_ROOT_DIR}/3rdparty/include/*.h)
source_group("3rdparty/include" FILES ${THREEDPARTY}) 

message("CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
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

set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rel")
set(CMAKE_MINSIZEREL_POSTFIX "_min")

set(BUILD_SHARED_LIBS ON)

project(osgGISPlugins)

add_subdirectory(src)
add_subdirectory(tools)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/debug/plugins/osgPlugins-3.6.5/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/Debug)
        file(COPY ${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/bin/jpeg62.dll DESTINATION ${PROJECT_ROOT_DIR}/Debug)
    else()
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/plugins/osgPlugins-3.6.5/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/Release)
        file(COPY ${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/bin/jpeg62.dll DESTINATION ${PROJECT_ROOT_DIR}/Release)
    endif()
endif()

message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
