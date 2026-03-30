get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(LIB_NAME ${DIR_NAME})

# 1. 搜集源文件
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES "${PROJECT_ROOT_DIR}/include/${LIB_NAME}/*.h")

# 2. 定义目标 (建议使用 MODULE 模式)
add_library(${LIB_NAME} MODULE ${DIR_SRCS} ${HEADER_FILES})

# 3. 设置 IDE 分组
source_group("include" FILES ${HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS})

# 4. 包含路径 (使用 target 作用域)
target_include_directories(${LIB_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 5. 链接 OSG
if (OPENSCENEGRAPH_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "OSG not found in ${LIB_NAME}")
endif()

# 6. 查找并链接 WebP (Vcpkg 现代 Target 模式)
find_package(WebP CONFIG REQUIRED)
# vcpkg 的 WebP 目标通常会自动处理其内部依赖（如 webp, webpdecoder, webpdemux）
target_link_libraries(${LIB_NAME} PRIVATE 
    WebP::webp 
    WebP::webpdecoder 
    WebP::webpdemux
)

# 7. 安装逻辑
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)