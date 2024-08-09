set(LIB_NAME osgdb_gltf)

set(HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Osg2Gltf.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/ReaderWriterGLTF.h)
set(SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/Osg2Gltf.cpp 
${PROJECT_ROOT_DIR}/src/osgdb_gltf/ReaderWriterGLTF.cpp)
set(OPTIMIZE_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Extensions.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfMerger.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfOptimizer.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfOptimizerManager.h 
)
set(OPTIMIZE_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/GltfMerger.cpp 
${PROJECT_ROOT_DIR}/src/osgdb_gltf/GltfOptimizer.cpp 
)
file(GLOB COMPRESS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/compress/*.h)
file(GLOB MATERIAL_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/*.h)
file(GLOB B3DM_HEADER_FILES ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/*.h)
file(GLOB COMPRESS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/compress/*.cpp)
file(GLOB MATERIAL_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/*.cpp)
file(GLOB B3DM_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/*.cpp)

add_library(${LIB_NAME}
${HEADER_FILES} ${SOURCE_FILES} 
${COMPRESS_HEADER_FILES} ${COMPRESS_SOURCE_FILES} 
${MATERIAL_HEADER_FILES} ${MATERIAL_SOURCE_FILES} 
${B3DM_HEADER_FILES} ${B3DM_SOURCE_FILES}
${OPTIMIZE_HEADER_FILES} ${OPTIMIZE_SOURCE_FILES}
)
source_group("include" FILES ${HEADER_FILES})
source_group("include/optimizer" FILES ${OPTIMIZE_HEADER_FILES}) 
source_group("include/optimizer/compress" FILES ${COMPRESS_HEADER_FILES})
source_group("include/optimizer/material" FILES ${MATERIAL_HEADER_FILES})
source_group("include/b3dm" FILES ${B3DM_HEADER_FILES}) 

source_group("src" FILES ${SOURCE_FILES})
source_group("src/optimizer/" FILES ${OPTIMIZE_SOURCE_FILES}) 
source_group("src/optimizer/compress" FILES ${COMPRESS_SOURCE_FILES})
source_group("src/optimizer/material" FILES ${MATERIAL_SOURCE_FILES})
source_group("src/b3dm" FILES ${B3DM_SOURCE_FILES}) 

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
