set(TOOL_NAME "model23dtiles")

# 设置编译标志
# set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
# set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# 查找源文件
aux_source_directory(. DIR_SRCS)
source_group("src" FILES ${DIR_SRCS})

# 定义可执行文件
file(GLOB_RECURSE THREED_TILES_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
file(GLOB_RECURSE THREED_TILES_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
ADD_EXECUTABLE(${TOOL_NAME} ${DIR_SRCS} 
${UTILS_SOURCE_FILES} ${UTILS_SOURCE_FILES} 
${THREED_TILES_HEADER_FILES} ${THREED_TILES_SOURCE_FILES}
${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
)
# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_NAME} osgdb_gltf)
add_dependencies(${TOOL_NAME} osgdb_fbx)
add_dependencies(${TOOL_NAME} osgdb_ktx)
add_dependencies(${TOOL_NAME} osgdb_webp)
source_group("include/3dtiles" FILES ${THREED_TILES_HEADER_FILES}  ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h)
source_group("include/util" FILES ${UTILS_HEADER_FILES})

source_group("src" FILES ${DIR_SRCS})
source_group("src/3dtiles" FILES ${THREED_TILES_SOURCE_FILES})
source_group("src/util" FILES ${UTILS_SOURCE_FILES} ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp)


# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
target_link_libraries(${TOOL_NAME} 
    meshoptimizer::meshoptimizer
    TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
)

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
