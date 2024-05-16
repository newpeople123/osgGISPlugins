set(TOOL_NAME "model23dtiles")
aux_source_directory(. DIR_SRCS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
ADD_EXECUTABLE(${TOOL_NAME} ${DIR_SRCS})

include(${CMAKE_TOOLCHAIN_FILE})
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${CMAKE_CURRENT_SOURCE_DIR}/../include)

find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TOOL_NAME} meshoptimizer::meshoptimizer)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} -pthread)
endif()

if (OPENSCENEGRAPH_FOUND)
    message("OPENSCENEGRAPH_INCLUDE_DIRS:${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message("OPENSCENEGRAPH_LIBRARIES:${OPENSCENEGRAPH_LIBRARIES}")

    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(${TOOL_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    else()
        target_link_libraries(${TOOL_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    endif()
else()
    message(FATAL_ERROR ${TOOL_NAME}+":osg not found!")
endif()
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})