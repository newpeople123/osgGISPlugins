set(TOOL_NAME "model23dtiles")

# 设置编译标志
# set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
# set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# 查找源文件
aux_source_directory(. DIR_SRCS)
source_group("src" FILES ${DIR_SRCS})

# 定义可执行文件
file(GLOB_RECURSE HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
add_executable(${TOOL_NAME} ${DIR_SRCS} ${HEADER_FILES})
# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_NAME} osgdb_gltf)
add_dependencies(${TOOL_NAME} osgdb_fbx)
add_dependencies(${TOOL_NAME} osgdb_ktx)
add_dependencies(${TOOL_NAME} osgdb_webp)
source_group("include" FILES ${HEADER_FILES}) 

# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TOOL_NAME} meshoptimizer::meshoptimizer)

# 如果是 Linux 系统，链接 pthread 库
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread)
endif()

if (OPENSCENEGRAPH_FOUND)
    message(STATUS "Found OpenSceneGraph")
    message(STATUS "OPENSCENEGRAPH_INCLUDE_DIRS: ${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message(STATUS "OPENSCENEGRAPH_LIBRARIES: ${OPENSCENEGRAPH_LIBRARIES}")

    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: OpenSceneGraph not found!")
endif()

# 安装目标
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
