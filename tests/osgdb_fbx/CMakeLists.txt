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
        include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
        if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(${TOOL_TEST_NAME}
            ${OPENSCENEGRAPH_LIBRARIES}
            )
        else()
            target_link_libraries(${TOOL_TEST_NAME}
            ${OPENSCENEGRAPH_LIBRARIES}
            )
        endif()
else()
    message(FATAL_ERROR ${TOOL_TEST_NAME}+":osg not found!")
endif()
install(TARGETS ${TOOL_TEST_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
