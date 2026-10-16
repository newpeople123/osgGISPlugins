cmake_minimum_required(VERSION 3.12)

set(PROD_TEST_NAME "test_production")

# 包含目录必须在 add_executable 之前设置（CMake 的 INCLUDE_DIRECTORIES
# 目录属性是在 target 创建时读取的，放在后面不生效）
include_directories(
    "${PROJECT_ROOT_DIR}/include"
    "${PROJECT_ROOT_DIR}/vcpkg_installed/x64-windows/include"
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

file(GLOB_RECURSE PROD_SRC "${PROJECT_ROOT_DIR}/src/3dtiles/production/*.cpp")
file(GLOB_RECURSE PROD_HDR "${PROJECT_ROOT_DIR}/include/3dtiles/production/*.h")
file(GLOB UTILS_SOURCE_FILES "${PROJECT_ROOT_DIR}/src/utils/*.cpp")
file(GLOB UTILS_HEADER_FILES "${PROJECT_ROOT_DIR}/include/utils/*.h")
file(GLOB TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 生产管线会用到 glTF 侧的工具（材质比较、法线/MR 图生成、BatchIdVisitor），
# 这些符号不进来的话链接期会缺 6 个外部符号。
set(GLTF_COMPONENTS_SRC
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)
set(GLTF_COMPONENTS_H
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)

add_executable(${PROD_TEST_NAME} ${TEST_SRCS} ${PROD_SRC} ${PROD_HDR}
               ${UTILS_SOURCE_FILES} ${UTILS_HEADER_FILES}
               ${GLTF_COMPONENTS_SRC} ${GLTF_COMPONENTS_H})

target_compile_features(${PROD_TEST_NAME} PRIVATE cxx_std_17)

# 单元测试不需要 TBB 并行；源码里的 #ifdef USE_TBB_PARALLEL 分支保持关闭，
# 顺带验证"关掉 TBB 也能构建"（手册 13.2）。

source_group("production" FILES ${PROD_SRC} ${PROD_HDR})
source_group("utils" FILES ${UTILS_SOURCE_FILES} ${UTILS_HEADER_FILES})

find_package(meshoptimizer CONFIG REQUIRED)
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil)
# legacy 的 TreeBuilder 等单元无条件引用 TBB，链接上即可。
# 不依赖 find_package(TBB)：这里直接用仓库内 vcpkg_installed 的绝对路径，
# 避免 find_package 在 toolchain 下静默失败导致 tbb12.lib 找不到。
target_link_libraries(${PROD_TEST_NAME}
    "${PROJECT_ROOT_DIR}/vcpkg_installed/x64-windows/lib/tbb12.lib"
)

target_include_directories(${PROD_TEST_NAME} PRIVATE
    "${PROJECT_ROOT_DIR}/include"
    "${PROJECT_ROOT_DIR}/vcpkg_installed/x64-windows/include"
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

target_link_libraries(${PROD_TEST_NAME}
    meshoptimizer::meshoptimizer
    ${OPENSCENEGRAPH_LIBRARIES}
)

install(TARGETS ${PROD_TEST_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
