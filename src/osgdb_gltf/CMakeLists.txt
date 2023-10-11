set(LIB_NAME osgdb_gltf)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
add_library(${LIB_NAME} ${DIR_SRCS})
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

#将库安装到bin文件夹中
# 将目标安装到不同的目录中，具体取决于构建模式
install(TARGETS ${LIB_NAME}
    RUNTIME DESTINATION ${PROJECT_ROOT_DIR}/bin/$<CONFIG>
    LIBRARY DESTINATION ${PROJECT_ROOT_DIR}/lib/$<CONFIG>
    ARCHIVE DESTINATION ${PROJECT_ROOT_DIR}/lib/$<CONFIG>
)
