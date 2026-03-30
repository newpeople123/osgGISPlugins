get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(LIB_NAME ${DIR_NAME})

# 1. 搜集源文件 (包含插件核心和 gltf 材质辅助类)
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES "${PROJECT_ROOT_DIR}/include/${LIB_NAME}/*.h")
file(GLOB_RECURSE GLTF_HEADER_MATERIAL_FILES "${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/*.h")
file(GLOB_RECURSE GLTF_SOURCE_MATERIAL_FILES "${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/*.cpp")

# 2. 定义目标 (建议使用 MODULE 模式)
add_library(${LIB_NAME} MODULE 
    ${DIR_SRCS} 
    ${HEADER_FILES} 
    ${GLTF_HEADER_MATERIAL_FILES} 
    ${GLTF_SOURCE_MATERIAL_FILES}
)

# 3. 设置 IDE 分组 (Visual Studio 目录结构)
source_group("include" FILES ${HEADER_FILES}) 
source_group("include/gltfMaterial" FILES ${GLTF_HEADER_MATERIAL_FILES}) 
source_group("src" FILES ${DIR_SRCS})
source_group("src/gltfMaterial" FILES ${GLTF_SOURCE_MATERIAL_FILES}) 

# 4. 包含路径设置 (使用 target 作用域避免污染)
target_include_directories(${LIB_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

if(WIN32)
    target_include_directories(${LIB_NAME} PRIVATE ${PROJECT_ROOT_DIR}/3rdparty/include/windows)
else()
    target_include_directories(${LIB_NAME} PRIVATE ${PROJECT_ROOT_DIR}/3rdparty/include/linux)
endif()

# 5. 链接库设置
if (OPENSCENEGRAPH_FOUND)
    # 链接 OSG 核心 (自动处理 Debug/Release)
    target_link_libraries(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "OSG not found for ${LIB_NAME}")
endif()

# 6. 链接 FBX SDK (手动处理平台和 Debug/Release)
if(WIN32)
    # 使用 generator expression 优雅地切换 Debug/Release 路径
    set(FBX_LIB_PATH "${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/$<IF:$<CONFIG:Debug>,debug,release>")
    
    target_link_libraries(${LIB_NAME} PRIVATE 
        "${FBX_LIB_PATH}/libfbxsdk-md.lib"
        "${FBX_LIB_PATH}/libxml2-md.lib"
        "${FBX_LIB_PATH}/zlib-md.lib"
    )
elseif(UNIX)
    # Linux 下通常是单一路径或通过系统路径查找
    set(FBX_LIB_TYPE "$<IF:$<CONFIG:Debug>,debug,release>")
    target_link_libraries(${LIB_NAME} PRIVATE "/usr/local/lib/gcc4/x64/${FBX_LIB_TYPE}/libfbxsdk.so")
endif()

# 7. 安装逻辑
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)