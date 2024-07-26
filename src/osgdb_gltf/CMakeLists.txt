set(LIB_NAME osgdb_gltf)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
set(HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Extensions.h ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Osg2Gltf.h ${PROJECT_ROOT_DIR}/include/osgdb_gltf/ReaderWriterGLTF.h)
set(SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/Osg2Gltf.cpp ${PROJECT_ROOT_DIR}/src/osgdb_gltf/ReaderWriterGLTF.cpp)
file(GLOB COMPRESS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/compress/*.h)
file(GLOB MATERIAL_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/*.h)
file(GLOB MERGE_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/merge/*.h)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB COMPRESS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/compress/*.cpp)
file(GLOB MATERIAL_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/*.cpp)
file(GLOB MERGE_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/merge/*.cpp)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)

add_library(${LIB_NAME} ${DIR_SRCS} 
${HEADER_FILES} ${SOURCE_FILES} 
${COMPRESS_HEADER_FILES} ${COMPRESS_SOURCE_FILES} 
${MATERIAL_HEADER_FILES} ${MATERIAL_SOURCE_FILES} 
${MERGE_HEADER_FILES} ${MERGE_SOURCE_FILES} 
${UTILS_HEADER_FILES} ${UTILS_SOURCE_FILES}
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
find_package(draco CONFIG REQUIRED)
target_link_libraries(${LIB_NAME} 
    meshoptimizer::meshoptimizer
    draco::draco
)
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
)
