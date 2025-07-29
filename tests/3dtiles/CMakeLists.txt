cmake_minimum_required(VERSION 3.12)

set(TOOL_TEST_NAME "test_3dtiles")

# 收集源文件
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE THREED_TILES_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
file(GLOB_RECURSE THREED_TILES_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)

# B3DM 源码和头文件
set(B3DM_HEADER_FILES
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
)

set(B3DM_SOURCE_FILES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
)

# 构建目标
add_executable(${TOOL_TEST_NAME}
    ${DIR_SRCS}
    ${UTILS_SOURCE_FILES}
    ${THREED_TILES_HEADER_FILES}
    ${THREED_TILES_SOURCE_FILES}
    ${UTILS_HEADER_FILES}
    ${B3DM_HEADER_FILES}
    ${B3DM_SOURCE_FILES}
)

# 构建依赖
add_dependencies(${TOOL_TEST_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 分组：让 VS 中更清晰（可选 GLOB_RECURSE 结果包含路径）
source_group("src" FILES ${DIR_SRCS})

source_group("include\\3dtiles" FILES ${THREED_TILES_HEADER_FILES})
source_group("include\\utils"  FILES ${UTILS_HEADER_FILES})
source_group("include\\gltf\\material" FILES
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)
source_group("include\\gltf\\b3dm" FILES
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
)

source_group("src\\3dtiles" FILES ${THREED_TILES_SOURCE_FILES})
source_group("src\\utils"  FILES ${UTILS_SOURCE_FILES})
source_group("src\\gltf\\material" FILES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)
source_group("src\\gltf\\b3dm" FILES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
)

# 查找依赖库
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

# 链接第三方库
target_link_libraries(${TOOL_TEST_NAME}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
)

# OpenSceneGraph
if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_TEST_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_TEST_NAME}: osg not found!")
endif()

# 安装路径
install(TARGETS ${TOOL_TEST_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
