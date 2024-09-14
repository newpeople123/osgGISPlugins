set(TOOL_TEST_NAME "test_osgdb_gltf")
aux_source_directory(. DIR_SRCS)

set(B3DM_HADER_FILE ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h)
set(B3DM_SOURCE_FILE ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)

# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)

ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS} 
${B3DM_HADER_FILE} ${B3DM_SOURCE_FILE} 
${UTILS_HEADER_FILES} ${UTILS_SOURCE_FILES} 
${TEST_HEADER_FILES} ${TEST_SOURCE_FILES} 
)

# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_TEST_NAME} osgdb_gltf)
add_dependencies(${TOOL_TEST_NAME} osgdb_fbx)
add_dependencies(${TOOL_TEST_NAME} osgdb_ktx)
add_dependencies(${TOOL_TEST_NAME} osgdb_webp)

# 链接库
target_link_libraries(${TOOL_TEST_NAME} 
    meshoptimizer::meshoptimizer 
)

source_group("include" FILES ${B3DM_HADER_FILE}) 
source_group("include/utils" FILES ${UTILS_HEADER_FILES}) 
source_group("src" FILES ${B3DM_SOURCE_FILE}) 
source_group("src/utils" FILES ${UTILS_SOURCE_FILES})  

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
