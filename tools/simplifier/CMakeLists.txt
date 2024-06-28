set(TOOL_NAME "simplifier")

# 查找源文件
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/optimizer/*.cpp)
source_group("src" FILES ${DIR_SRCS})
source_group("src/optimizer/mesh" FILES ${SOURCE_FILES})
source_group("src/optimizer/texture" FILES 
${PROJECT_ROOT_DIR}/src/utils/TextureOptimizer.cpp 
${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp
)

# 定义可执行文件
file(GLOB_RECURSE HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/optimizer/*.h)
add_executable(${TOOL_NAME} ${DIR_SRCS} ${HEADER_FILES} ${SOURCE_FILES} ${UTILS_HEADER_FILES} 
${PROJECT_ROOT_DIR}/include/utils/TextureOptimizer.h ${PROJECT_ROOT_DIR}/src/utils/TextureOptimizer.cpp 
${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h ${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp
)
source_group("include/optimizer/mesh" FILES ${HEADER_FILES}) 
source_group("include/optimizer/texture" FILES 
${PROJECT_ROOT_DIR}/include/utils/TextureOptimizer.h 
${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h
) 
# 包含外部工具链文件
include(${CMAKE_TOOLCHAIN_FILE})

# 设置全局包含路径，必须放在添加子目录之前
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${PROJECT_ROOT_DIR}/include)
# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)

# 链接库
target_link_libraries(${TOOL_NAME} 
    meshoptimizer::meshoptimizer
)
# 如果是 Linux 系统，链接 pthread 库
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread)
endif()

# 查找 OpenSceneGraph 包
find_package(OpenSceneGraph REQUIRED)
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
