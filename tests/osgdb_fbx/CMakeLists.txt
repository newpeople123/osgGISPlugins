set(TOOL_TEST_NAME "test_osgdb_fbx")
aux_source_directory(. DIR_SRCS)
ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS})

# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_TEST_NAME} osgdb_gltf)
add_dependencies(${TOOL_TEST_NAME} osgdb_fbx)
add_dependencies(${TOOL_TEST_NAME} osgdb_ktx)
add_dependencies(${TOOL_TEST_NAME} osgdb_webp)

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
