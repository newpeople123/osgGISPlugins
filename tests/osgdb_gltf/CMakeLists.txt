set(TOOL_TEST_NAME "test_osgdb_gltf")
aux_source_directory(. DIR_SRCS)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Extensions.h ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Osg2Gltf.h)
file(GLOB COMPRESS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/compress/*.h)
file(GLOB MATERIAL_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/*.h)
file(GLOB MERGE_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/merge/*.h)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB COMPRESS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/compress/*.cpp)
file(GLOB MATERIAL_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/*.cpp)
file(GLOB MERGE_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/merge/*.cpp)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
set(SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/Osg2Gltf.cpp)

file(GLOB_RECURSE TEST_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/optimizer/*.h)
file(GLOB_RECURSE TEST_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/optimizer/*.cpp)

ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS} 
${HEADER_FILES} ${SOURCE_FILES}
${COMPRESS_HEADER_FILES} ${COMPRESS_SOURCE_FILES}
${MATERIAL_HEADER_FILES} ${MATERIAL_SOURCE_FILES}
${MERGE_HEADER_FILES} ${MERGE_SOURCE_FILES}
${UTILS_HEADER_FILES} ${UTILS_SOURCE_FILES}
${TEST_HEADER_FILES} ${TEST_SOURCE_FILES}
)
# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)

# 链接库
target_link_libraries(${TOOL_TEST_NAME} 
    meshoptimizer::meshoptimizer
)

source_group("include" FILES ${HEADER_FILES})
source_group("include/compress" FILES ${COMPRESS_HEADER_FILES})
source_group("include/material" FILES ${MATERIAL_HEADER_FILES})
source_group("include/merge" FILES ${MERGE_HEADER_FILES})
source_group("include/utils" FILES ${UTILS_HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS} ${SOURCE_FILES})
source_group("src/compress" FILES ${COMPRESS_SOURCE_FILES})
source_group("src/material" FILES ${MATERIAL_SOURCE_FILES})
source_group("src/merge" FILES ${MERGE_SOURCE_FILES})
source_group("src/utils" FILES ${UTILS_SOURCE_FILES}) 

include(${CMAKE_TOOLCHAIN_FILE})
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${PROJECT_ROOT_DIR}/include ${TEST_HEADER_FILES})

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
