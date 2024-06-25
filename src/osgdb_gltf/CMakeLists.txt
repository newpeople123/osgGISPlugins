set(LIB_NAME osgdb_gltf)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/*.h)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
file(GLOB_RECURSE SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/*.cpp)

add_library(${LIB_NAME} ${DIR_SRCS} ${HEADER_FILES} ${SOURCE_FILES})
source_group("include" FILES ${HEADER_FILES})
source_group("include/utils" FILES ${UTILS_HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS} ${SOURCE_FILES})
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
target_link_libraries(${LIB_NAME} meshoptimizer::meshoptimizer)
find_package(draco CONFIG REQUIRED)
target_link_libraries(${LIB_NAME} draco::draco)
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
)
