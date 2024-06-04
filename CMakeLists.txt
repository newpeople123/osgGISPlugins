cmake_minimum_required(VERSION 3.12)
# 打开允许创建文件夹的开关
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    add_compile_options("/utf-8")
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
else()
    set(VCPKG_TARGET_TRIPLET "x64-linux-dynamic" CACHE STRING "Vcpkg target triplet")
endif()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
        set(CMAKE_TOOLCHAIN_FILE D:/vcpkg/scripts/buildsystems/vcpkg.cmake)
    else()
        set(CMAKE_TOOLCHAIN_FILE /root/vcpkg/scripts/buildsystems/vcpkg.cmake)
    endif()
endif()
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

message("CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG /usr/local/lib64)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG /usr/local/lib64)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG /usr/local/lib64)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE /usr/local/lib64)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE /usr/local/lib64)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE /usr/local/lib64)
endif()

set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rel")
set(CMAKE_MINSIZEREL_POSTFIX "_min")


set(BUILD_SHARED_LIBS ON)


project(osgGISPlugins)

add_subdirectory(src)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()
add_subdirectory(tools)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/out/build/x64-Debug/vcpkg_installed/x64-windows/debug/plugins/osgPlugins-3.6.5/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/build/output/Debug)
        file(COPY ${PROJECT_ROOT_DIR}/out/build/x64-Debug/vcpkg_installed/x64-windows/bin/jpeg62.dll DESTINATION ${PROJECT_ROOT_DIR}/build/output/Debug)
    else()
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/out/build/x64-Release/vcpkg_installed/x64-windows/plugins/osgPlugins-3.6.5/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/build/output/Release)
        file(COPY ${PROJECT_ROOT_DIR}/out/build/x64-Release/vcpkg_installed/x64-windows/bin/jpeg62.dll DESTINATION ${PROJECT_ROOT_DIR}/build/output/Release)
    endif()
endif()

message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
