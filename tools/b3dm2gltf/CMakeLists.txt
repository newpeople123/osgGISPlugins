# 1. 动态获取当前文件夹名称作为项目/工具名称
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

# 2. 搜集源文件
aux_source_directory(. DIR_SRCS)

# 3. 定义可执行文件
add_executable(${TOOL_NAME} ${DIR_SRCS})

# 调用根目录定义的函数，传入你的目标名称
copy_osg_dependencies(${TOOL_NAME})

# 4. 设置 IDE 分组
source_group("src" FILES ${DIR_SRCS})

# 5. 包含路径 (使用 target 作用域)
target_include_directories(${TOOL_NAME} PRIVATE 
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 6. 平台特定编译选项 (Linux 下禁用 PIE)
if(UNIX AND NOT APPLE)
    target_compile_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_options(${TOOL_NAME} PRIVATE "-no-pie")
endif()

# 7. 链接 OSG
if (OPENSCENEGRAPH_FOUND)
    target_link_libraries(${TOOL_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: OpenSceneGraph not found!")
endif()

# 8. 安装逻辑
install(TARGETS ${TOOL_NAME} 
    RUNTIME DESTINATION bin
)