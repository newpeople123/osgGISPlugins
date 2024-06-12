set(TOOL_TEST_NAME "test_osgdb_gltf")
aux_source_directory(. DIR_SRCS)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS})
source_group("src" FILES ${DIR_SRCS})
include(${CMAKE_TOOLCHAIN_FILE})
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${PROJECT_ROOT_DIR}/include)

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
