set(TEST_NAME "Test")
project(${TEST_NAME})
aux_source_directory(. DIR_SRCS)
ADD_EXECUTABLE(${TEST_NAME} ${DIR_SRCS})

include(${CMAKE_TOOLCHAIN_FILE})
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${CMAKE_CURRENT_SOURCE_DIR}/../include)
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TEST_NAME} meshoptimizer::meshoptimizer)
#查找OpenSceneGraph库的核心组件
#-----
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgViewer osgText osgAnimation)
if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(${TEST_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    else()
        target_link_libraries(${TEST_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    endif()
else()
    message(FATAL_ERROR ${TEST_NAME}+":osg not found!")
endif()
