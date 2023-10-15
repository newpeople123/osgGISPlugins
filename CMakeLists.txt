# CMake 最低版本号要求
cmake_minimum_required(VERSION 3.12)

# 设置CMAKE_TOLLCHAIN_FILE 将vcpkg.cmake的路径赋值给CMAKE_TOOLCHAIN_FILE变量。必须在project函数之前设置
set(CMAKE_TOOLCHAIN_FILE D:/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")


# 设置可执行文件输出目录(设置默认值)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_ROOT_DIR}/bin)
# # 设置动态库文件输出目录(设置默认值)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_ROOT_DIR}/bin)
# # 设置静态库文件输出目录(设置默认值)
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${PROJECT_ROOT_DIR}/lib)
# debug模式下输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug) #可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug) #动态库文件
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/build/output/Debug) #静态库文件
# release模式下输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release) #可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release) #动态库文件
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/build/output/Release) #静态库文件
# relwithdebinfo模式下输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo) #可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo) #动态库文件
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_ROOT_DIR}/build/output/RelWithDebInfo) #静态库文件
# minsizerel模式下输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel) #可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel) #动态库文件
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_ROOT_DIR}/build/output/MinSizeRel) #静态库文件

set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rel")
set(CMAKE_MINSIZEREL_POSTFIX "_min")

#全局设置，让 add_library() 生成 .dll 动态库，对应 SHARED
set(BUILD_SHARED_LIBS ON)

message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
# 项目名称
project(osgGISPlugins)


# 添加子目录
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(tools)
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
