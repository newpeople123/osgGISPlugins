# 1. 动态获取当前文件夹名称 (得到 "gui")
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

# 2. 搜集源文件
aux_source_directory(. DIR_SRCS)

# 3. 定义可执行文件
add_executable(${TOOL_NAME} ${DIR_SRCS})

# 4. 设置 C++ 标准 (GUI 通常需要更现代的特性，这里设为 17)
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)

# 5. 设置 IDE 分组
source_group("src" FILES ${DIR_SRCS})

# 6. 查找依赖库
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(Stb REQUIRED)

# 7. 包含路径与编译选项
target_include_directories(${TOOL_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
)

# Linux 平台特定选项
if(UNIX AND NOT APPLE)
    target_compile_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_options(${TOOL_NAME} PRIVATE "-no-pie")
endif()

# 8. 设置构建依赖顺序
# 确保 GUI 在其他工具编译完成后再构建
add_dependencies(${TOOL_NAME}
    b3dm2gltf
    model23dtiles
    simplifier
    texturepacker
)

# 9. 链接库
# 注意：vcpkg 引入的 glfw3 目标通常叫 glfw，OpenGL 是 OpenGL::GL
target_link_libraries(${TOOL_NAME} PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    tinyfiledialogs::tinyfiledialogs
    ${OPENSCENEGRAPH_LIBRARIES}
)

# Linux 补充链接
if(UNIX AND NOT APPLE)
    target_link_libraries(${TOOL_NAME} PRIVATE pthread dl)
endif()

# 10. 资源文件拷贝 (使用生成器表达式优化路径)
set(ASSETS_DIR "${PROJECT_ROOT_DIR}/assets")
set(TARGET_OUT_DIR "$<TARGET_FILE_DIR:${TOOL_NAME}>")

# 在构建完成后自动拷贝资源到生成目录下
add_custom_command(TARGET ${TOOL_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSETS_DIR}/favicon.png" "${TARGET_OUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSETS_DIR}/msyh.ttc" "${TARGET_OUT_DIR}"
    COMMENT "Copying assets to output directory..."
)

# 11. 安装逻辑
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION bin)
