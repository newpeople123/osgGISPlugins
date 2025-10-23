set(TOOL_NAME "texturepacker")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
endif()
# 查找当前目录源文件
aux_source_directory(. TOOL_SRCS)

# 工具相关头文件和源文件
set(TEXTURE_PACKER_HEADER ${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h)
set(TEXTURE_PACKER_SOURCE ${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp)

# 所有源文件
set(SOURCE_FILES
    ${TOOL_SRCS}
    ${TEXTURE_PACKER_HEADER}
    ${TEXTURE_PACKER_SOURCE}
)

# 添加可执行文件
add_executable(${TOOL_NAME} ${SOURCE_FILES})

# 添加依赖
add_dependencies(${TOOL_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 查找依赖库
find_package(meshoptimizer CONFIG REQUIRED)
find_package(Stb REQUIRED)

# 包含头文件目录（仅本目标）
target_include_directories(${TOOL_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/include
    ${Stb_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${TOOL_NAME}
    meshoptimizer::meshoptimizer
)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread)
endif()

# OpenSceneGraph
if (OPENSCENEGRAPH_FOUND)
    message(STATUS "Found OpenSceneGraph")
    message(STATUS "OPENSCENEGRAPH_INCLUDE_DIRS: ${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message(STATUS "OPENSCENEGRAPH_LIBRARIES: ${OPENSCENEGRAPH_LIBRARIES}")

    target_include_directories(${TOOL_NAME} PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: OpenSceneGraph not found!")
endif()

# Visual Studio 文件分组
source_group("src" FILES ${TOOL_SRCS} ${TEXTURE_PACKER_SOURCE})
source_group("include" FILES ${TEXTURE_PACKER_HEADER})

# 安装目标
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
