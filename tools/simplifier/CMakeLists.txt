set(TOOL_NAME "simplifier")

# 查找源文件
aux_source_directory(. DIR_SRCS)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
source_group("include" FILES ${DIR_SRCS} 
${UTILS_HEADER_FILES} 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)

source_group("src" FILES ${DIR_SRCS} 
${UTILS_SOURCE_FILES} 
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

# 定义可执行文件
add_executable(${TOOL_NAME} ${DIR_SRCS} 
${UTILS_HEADER_FILES} ${UTILS_SOURCE_FILES} 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)
# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_NAME} osgdb_gltf)
add_dependencies(${TOOL_NAME} osgdb_fbx)
add_dependencies(${TOOL_NAME} osgdb_ktx)
add_dependencies(${TOOL_NAME} osgdb_webp)

# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

# 链接库
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
