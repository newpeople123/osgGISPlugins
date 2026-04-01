# 1. 动态获取文件夹名 (model23dtiles)
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

# 2. 搜集源文件与跨模块引用
aux_source_directory(. DIR_SRCS)

# 显式引用 gltf/b3dm 和 material 相关源码 (因其逻辑与插件共享)
set(GLTF_COMPONENTS_H
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)
set(GLTF_COMPONENTS_SRC
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

# 搜集 3dtiles 和 utils 模块
file(GLOB_RECURSE THREED_TILES_FILES_H 
    "${PROJECT_ROOT_DIR}/include/3dtiles/*.h" 
)
file(GLOB_RECURSE THREED_TILES_FILES_SRC
    "${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp"
)
file(GLOB_RECURSE UTILS_FILES_H
    "${PROJECT_ROOT_DIR}/include/utils/*.h" 
)
file(GLOB_RECURSE UTILS_FILES_SRC
    "${PROJECT_ROOT_DIR}/src/utils/*.cpp"
)

# 3. 定义可执行文件
add_executable(${TOOL_NAME}
    ${DIR_SRCS}
    ${GLTF_COMPONENTS_H}
    ${GLTF_COMPONENTS_SRC}
    ${THREED_TILES_FILES_H}
    ${THREED_TILES_FILES_SRC}
    ${UTILS_FILES_H}
    ${UTILS_FILES_SRC}
)

# 调用根目录定义的函数，传入你的目标名称
copy_osg_dependencies(${TOOL_NAME})

# 4. 编译属性设置
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)
# 启用并行定义
target_compile_definitions(${TOOL_NAME} PRIVATE USE_TBB_PARALLEL=1)

# 5. 设置 IDE 分组 (使用单反斜杠或正斜杠，CMake 推荐正斜杠)
source_group("include/3dtiles" FILES ${THREED_TILES_FILES_H})
source_group("include/utils" FILES ${UTILS_FILES_H})
source_group("include/gltf" FILES ${GLTF_COMPONENTS_H})
source_group("src" FILES ${DIR_SRCS})
source_group("src/3dtiles" FILES ${THREED_TILES_FILES_SRC})
source_group("src/utils" FILES ${UTILS_FILES_SRC})
source_group("src/gltf" FILES ${GLTF_COMPONENTS_SRC})

# 6. 查找依赖
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)

# 7. 包含路径 (PRIVATE 作用域)
target_include_directories(${TOOL_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 8. 链接库
target_link_libraries(${TOOL_NAME} PRIVATE
    ${OPENSCENEGRAPH_LIBRARIES}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
    PROJ::proj
)

# 9. 平台特定逻辑
if(UNIX AND NOT APPLE)
    # 修正：-no-pie 的作用是关闭“位置无关可执行程序”，通常用于解决某些静态库链接时的符号冲突
    # 它并不会把可执行文件“识别为 .so”，.so 是共享库(SHARED)
    target_compile_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_libraries(${TOOL_NAME} PRIVATE pthread)
endif()

# 10. 资源/数据拷贝 (PROJ 数据库文件)
if(WIN32)
    # 使用 POST_BUILD 确保每次构建后同步数据，而不是仅在配置阶段
    set(PROJ_DATA_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/share/proj")
    set(TARGET_DATA_DEST "$<TARGET_FILE_DIR:${TOOL_NAME}>/share/proj")

    add_custom_command(TARGET ${TOOL_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TARGET_DATA_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJ_DATA_DIR}" "${TARGET_DATA_DEST}"
        COMMENT "Syncing PROJ data files..."
    )
endif()

# 11. 安装
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION bin)
