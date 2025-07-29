cmake_minimum_required(VERSION 3.12)

set(TOOL_TEST_NAME "test_osgdb_gltf")

# 查找当前目录下所有测试源文件
aux_source_directory(. DIR_SRCS)

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

# 工具类源文件和头文件
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)


# 查找依赖包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

# 定义可执行文件
add_executable(${TOOL_TEST_NAME}
    ${DIR_SRCS}
    ${B3DM_HEADER_FILES}
    ${B3DM_SOURCE_FILES}
    ${UTILS_HEADER_FILES}
    ${UTILS_SOURCE_FILES}
    ${TEST_HEADER_FILES}
    ${TEST_SOURCE_FILES}
)

# 构建依赖，确保 osgdb 插件先编译
add_dependencies(${TOOL_TEST_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 链接第三方库
target_link_libraries(${TOOL_TEST_NAME}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
)

# VS 分组结构（source_group 仅对 MSVC 等 IDE 有效）
source_group("include\\gltf\\material" FILES
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)
source_group("include\\gltf\\b3dm" FILES
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
)
source_group("include\\utils" FILES ${UTILS_HEADER_FILES})

source_group("src\\gltf\\material" FILES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)
source_group("src\\gltf\\b3dm" FILES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
)
source_group("src\\utils" FILES ${UTILS_SOURCE_FILES})
source_group("src" FILES ${DIR_SRCS})

# OpenSceneGraph 支持
if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_TEST_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_TEST_NAME}: osg not found!")
endif()

# 安装目标
install(TARGETS ${TOOL_TEST_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
