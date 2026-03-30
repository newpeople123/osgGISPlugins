set(TOOL_TEST_NAME "test_osgdb_fbx")
aux_source_directory(. DIR_SRCS)
add_executable(${TOOL_TEST_NAME} ${DIR_SRCS})

# 构建依赖，确保 osgdb 插件先编译
add_dependencies(${TOOL_TEST_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

source_group("src" FILES ${DIR_SRCS})

if (OPENSCENEGRAPH_FOUND)
    # 链接路径列表
    target_link_libraries(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
    # 添加头文件包含路径
    target_include_directories(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "OSG not found for ${LIB_NAME}!")
endif()

