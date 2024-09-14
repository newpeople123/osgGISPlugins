set(TOOL_NAME "texturepacker")

# 查找源文件
aux_source_directory(. DIR_SRCS)
source_group("src" FILES ${DIR_SRCS})
source_group("src/optimizer/texture" FILES 
${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp
)

# 定义可执行文件
add_executable(${TOOL_NAME} ${DIR_SRCS} 
${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h ${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp
)

# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_NAME} osgdb_gltf)
add_dependencies(${TOOL_NAME} osgdb_fbx)
add_dependencies(${TOOL_NAME} osgdb_ktx)
add_dependencies(${TOOL_NAME} osgdb_webp)
source_group("include/optimizer/texture" FILES 
${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h
) 

# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(Stb REQUIRED)

target_include_directories(${TOOL_NAME} PRIVATE ${Stb_INCLUDE_DIR})

# 链接库
target_link_libraries(${TOOL_NAME} 
    meshoptimizer::meshoptimizer
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
