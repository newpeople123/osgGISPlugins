set(LIB_NAME osgdb_webp)
#查找当前目录下的所有源文件
#并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
file(GLOB_RECURSE HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_webp/*.h)
add_library(${LIB_NAME} ${DIR_SRCS} ${HEADER_FILES})
source_group("include" FILES ${HEADER_FILES}) 
source_group("src" FILES ${DIR_SRCS})
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
find_package(WebP CONFIG REQUIRED)
if (WebP_FOUND)
    # basic usage
    target_link_libraries(${LIB_NAME} WebP::webp WebP::webpdecoder WebP::webpdemux)
else()
    message(FATAL_ERROR "libwenp not found!")
endif()

install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
)
