set(LIB_NAME osgdb_gltf)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/*.h)
file(GLOB GLTF_HEADER_FILES ${PROJECT_ROOT_DIR}/include/gltf/*.h)
file(GLOB GLTF_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/gltf/*.cpp)
file(GLOB GLTF_MATERIAL_HEADER_FILES ${PROJECT_ROOT_DIR}/include/gltf/material/*.h)
file(GLOB GLTF_MATERIAL_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/gltf/material/*.cpp)
file(GLOB GLTF_COMPRESS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/gltf/compress/*.h)
file(GLOB GLTF_COMPRESS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/gltf/compress/*.cpp)
file(GLOB GLTF_MERGE_HEADER_FILES ${PROJECT_ROOT_DIR}/include/gltf/merge/*.h)
file(GLOB GLTF_MERGE_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/gltf/merge/*.cpp)
add_library(${LIB_NAME} ${DIR_SRCS} ${HEADER_FILES} 
${GLTF_MATERIAL_HEADER_FILES} ${GLTF_MATERIAL_SOURCE_FILES}
${GLTF_COMPRESS_HEADER_FILES} ${GLTF_COMPRESS_SOURCE_FILES}
${GLTF_MERGE_HEADER_FILES} ${GLTF_MERGE_SOURCE_FILES}
${GLTF_HEADER_FILES} ${GLTF_SOURCE_FILES}
)
source_group("include" FILES ${HEADER_FILES} ${GLTF_HEADER_FILES})
source_group("include/material" FILES ${GLTF_MATERIAL_HEADER_FILES}) 
source_group("include/compress" FILES ${GLTF_COMPRESS_HEADER_FILES}) 
source_group("include/merge" FILES ${GLTF_MERGE_HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS} ${GLTF_SOURCE_FILES})
source_group("src/material" FILES ${GLTF_MATERIAL_SOURCE_FILES}) 
source_group("src/compress" FILES ${GLTF_COMPRESS_SOURCE_FILES}) 
source_group("src/merge" FILES ${GLTF_MERGE_SOURCE_FILES}) 

if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(${LIB_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    else()
        target_link_libraries(${LIB_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
    endif()
else()
    message(FATAL_ERROR "osg not found!")
endif()
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${LIB_NAME} meshoptimizer::meshoptimizer)
find_package(draco CONFIG REQUIRED)
target_link_libraries(${LIB_NAME} draco::draco)
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
)
