set(TOOL_TEST_NAME "test_3dtiles")
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE THREED_TILES_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
file(GLOB_RECURSE THREED_TILES_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS} ${UTILS_SOURCE_FILES} ${UTILS_SOURCE_FILES} ${THREED_TILES_HEADER_FILES} ${THREED_TILES_SOURCE_FILES})
source_group("include" FILES ${UTILS_HEADER_FILES} ${THREED_TILES_HEADER_FILES})
source_group("src" FILES ${DIR_SRCS} ${UTILS_SOURCE_FILES} ${UTILS_SOURCE_FILES} )
include(${CMAKE_TOOLCHAIN_FILE})
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${PROJECT_ROOT_DIR}/include)
# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
# 链接库
target_link_libraries(${TOOL_TEST_NAME} 
    meshoptimizer::meshoptimizer
    TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
)
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
