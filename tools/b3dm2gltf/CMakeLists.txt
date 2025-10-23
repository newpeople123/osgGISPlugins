set(TOOL_NAME "b3dm2gltf")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
endif()
aux_source_directory(. DIR_SRCS)
add_executable(${TOOL_NAME} ${DIR_SRCS})
source_group("src" FILES ${DIR_SRCS})

if (OPENSCENEGRAPH_FOUND)
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
