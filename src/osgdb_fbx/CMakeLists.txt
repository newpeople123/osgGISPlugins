set(LIB_NAME osgdb_fbx)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
add_library(${LIB_NAME} ${DIR_SRCS})
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include/linux ${PROJECT_ROOT_DIR}/3rdparty/include/windows)
#find_package(Iconv)
if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(${LIB_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            target_link_libraries(${LIB_NAME} 
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/debug/libfbxsdk-md.lib 
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/debug/libxml2-md.lib
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/debug/zlib-md.lib
            )
        elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${LIB_NAME} 
            /usr/local/lib/gcc4/x64/debug/libfbxsdk.so)
        endif()
    else()
        target_link_libraries(${LIB_NAME}
        ${OPENSCENEGRAPH_LIBRARIES}
        )
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            target_link_libraries(${LIB_NAME}
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/release/libfbxsdk-md.lib 
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/release/libxml2-md.lib
            ${PROJECT_ROOT_DIR}/3rdparty/lib/windows/x64/release/zlib-md.lib
            )
        elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${LIB_NAME} 
            /usr/local/lib/gcc4/x64/release/libfbxsdk.so)
        endif()
    endif()
else()
    message(FATAL_ERROR "osg not found!")
endif()
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
)