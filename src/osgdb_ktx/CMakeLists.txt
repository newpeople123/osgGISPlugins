get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(LIB_NAME ${DIR_NAME})

# 1. 搜集源文件
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES "${PROJECT_ROOT_DIR}/include/${LIB_NAME}/*.h")

# 2. 定义目标 (使用 MODULE 模式最符合 OSG 插件规范)
add_library(${LIB_NAME} MODULE ${DIR_SRCS} ${HEADER_FILES})

# 3. 设置 IDE 分组
source_group("include" FILES ${HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS})

# 4. 链接 OSG (使用你打印出的变量)
if (OPENSCENEGRAPH_FOUND)
    # 链接路径列表
    target_link_libraries(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
    # 添加头文件包含路径
    target_include_directories(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "OSG not found for ${LIB_NAME}!")
endif()

# 5. 链接 KTX (vcpkg 的 KTX 通常支持现代 Target)
find_package(Ktx CONFIG REQUIRED)
if (Ktx_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE KTX::ktx)
endif()

# 6. 安装逻辑 (根据 OSG 习惯建议安装到 bin)
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)