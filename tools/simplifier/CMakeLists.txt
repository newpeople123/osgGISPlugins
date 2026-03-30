# 1. 动态获取文件夹名 (simplifier)
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

# 2. 搜集源文件与跨模块引用
aux_source_directory(. DIR_SRCS)

# 引用公共的 Utils 模块
file(GLOB_RECURSE UTILS_H "${PROJECT_ROOT_DIR}/include/utils/*.h")
file(GLOB_RECURSE UTILS_SRC "${PROJECT_ROOT_DIR}/src/utils/*.cpp")

# 引用共享的 GLTF 材质逻辑
set(GLTF_MATERIAL_COMPONENTS_H
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)
set(GLTF_MATERIAL_COMPONENTS_SRC
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

# 3. 定义可执行文件
add_executable(${TOOL_NAME} 
    ${DIR_SRCS} 
    ${UTILS_H} 
    ${UTILS_SRC} 
    ${GLTF_MATERIAL_COMPONENTS_H}
    ${GLTF_MATERIAL_COMPONENTS_SRC}
)

# 4. 编译属性设置
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)

# 5. 设置 IDE 分组 (使用正斜杠以获得更好的兼容性)
source_group("src" FILES ${DIR_SRCS})
source_group("src/utils" FILES ${UTILS_SRC})
source_group("src/gltf" FILES ${UTILS_SRC})
source_group("include/utils" FILES ${GLTF_MATERIAL_COMPONENTS_SRC})
source_group("include/gltf" FILES ${GLTF_MATERIAL_COMPONENTS_H})

# 6. 查找依赖
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

# 7. 包含路径 (使用 PRIVATE 作用域，确保路径不外泄)
target_include_directories(${TOOL_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 8. 链接库
target_link_libraries(${TOOL_NAME} PRIVATE
    ${OPENSCENEGRAPH_LIBRARIES}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
)

# 9. 平台特定逻辑 (Linux)
if(UNIX AND NOT APPLE)
    target_compile_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_libraries(${TOOL_NAME} PRIVATE pthread)
endif()

# 10. 明确构建依赖顺序
add_dependencies(${TOOL_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 11. 安装逻辑 (规范安装到 bin 目录)
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION bin)
