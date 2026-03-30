# 1. 动态获取文件夹名 (texturepacker)
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

# 2. 搜集源文件与跨模块引用
aux_source_directory(. TOOL_SRCS)

# 引用公共的 TexturePacker 工具类
set(TEXTURE_PACKER_FILES_H
    ${PROJECT_ROOT_DIR}/include/utils/TexturePacker.h
)
set(TEXTURE_PACKER_FILES_SRC
    ${PROJECT_ROOT_DIR}/src/utils/TexturePacker.cpp
)

# 3. 定义可执行文件
add_executable(${TOOL_NAME} ${TOOL_SRCS} ${TEXTURE_PACKER_FILES_H} ${TEXTURE_PACKER_FILES_SRC})

# 4. 编译属性设置 (统一使用 C++17)
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)

# 5. 设置 IDE 分组
source_group("src" FILES ${TOOL_SRCS})
source_group("src/utils" FILES ${TEXTURE_PACKER_FILES_SRC})
source_group("include/utils" FILES ${TEXTURE_PACKER_FILES_H})

# 6. 查找依赖
find_package(meshoptimizer CONFIG REQUIRED)
find_package(Stb REQUIRED)

# 7. 包含路径 (使用 target 作用域，包含 Stb 和项目全局 include)
target_include_directories(${TOOL_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${Stb_INCLUDE_DIR}
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 8. 链接库
target_link_libraries(${TOOL_NAME} PRIVATE
    ${OPENSCENEGRAPH_LIBRARIES}
    meshoptimizer::meshoptimizer
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
