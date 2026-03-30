get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(LIB_NAME ${DIR_NAME})

# 1. 定义源文件列表 (手动列出核心文件以保持严谨)
set(HEADER_FILES 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Osg2Gltf.h 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/ReaderWriterGLTF.h
)
set(SOURCE_FILES 
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/Osg2Gltf.cpp 
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/ReaderWriterGLTF.cpp
)

set(OPTIMIZE_HEADER_FILES 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/Extensions.h 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfMerger.h 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfProcessor.h 
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/GltfProcessorManager.h 
)
set(OPTIMIZE_SOURCE_FILES 
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/GltfMerger.cpp 
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/GltfProcessor.cpp 
)

# 使用 GLOB 搜集子模块
file(GLOB COMPRESS_HEADER_FILES "${PROJECT_ROOT_DIR}/include/osgdb_gltf/compress/*.h")
file(GLOB MATERIAL_HEADER_FILES "${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/*.h")
file(GLOB B3DM_HEADER_FILES     "${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/*.h")

file(GLOB COMPRESS_SOURCE_FILES "${PROJECT_ROOT_DIR}/src/osgdb_gltf/compress/*.cpp")
file(GLOB MATERIAL_SOURCE_FILES "${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/*.cpp")
file(GLOB B3DM_SOURCE_FILES     "${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/*.cpp")

# 引入通用的 Utils (建议作为公共源文件加入)
set(UTILS_FILES
    ${PROJECT_ROOT_DIR}/include/utils/Utils.h 
    ${PROJECT_ROOT_DIR}/src/utils/Utils.cpp
)

# 2. 创建目标
add_library(${LIB_NAME} MODULE
    ${HEADER_FILES} ${SOURCE_FILES} 
    ${COMPRESS_HEADER_FILES} ${COMPRESS_SOURCE_FILES} 
    ${MATERIAL_HEADER_FILES} ${MATERIAL_SOURCE_FILES} 
    ${B3DM_HEADER_FILES} ${B3DM_SOURCE_FILES}
    ${OPTIMIZE_HEADER_FILES} ${OPTIMIZE_SOURCE_FILES}
    ${UTILS_FILES}
)

# 3. 设置 IDE 源文件分组
source_group("include" FILES ${HEADER_FILES})
source_group("include/optimizer" FILES ${OPTIMIZE_HEADER_FILES}) 
source_group("include/optimizer/compress" FILES ${COMPRESS_HEADER_FILES})
source_group("include/optimizer/material" FILES ${MATERIAL_HEADER_FILES})
source_group("include/b3dm" FILES ${B3DM_HEADER_FILES}) 
source_group("include/utils" FILES ${PROJECT_ROOT_DIR}/include/utils/Utils.h) 

source_group("src" FILES ${SOURCE_FILES})
source_group("src/optimizer" FILES ${OPTIMIZE_SOURCE_FILES}) 
source_group("src/optimizer/compress" FILES ${COMPRESS_SOURCE_FILES})
source_group("src/optimizer/material" FILES ${MATERIAL_SOURCE_FILES})
source_group("src/b3dm" FILES ${B3DM_SOURCE_FILES}) 
source_group("src/utils" FILES ${PROJECT_ROOT_DIR}/src/utils/Utils.cpp) 

# 4. 包含路径 (使用 target_include 避免全局污染)
target_include_directories(${LIB_NAME} PRIVATE 
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# 5. 链接 OSG
if (OPENSCENEGRAPH_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "OSG not found in ${LIB_NAME}")
endif()

# 6. 查找并链接第三方依赖 (Vcpkg 现代 Target 模式)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(draco CONFIG REQUIRED)

target_link_libraries(${LIB_NAME} PRIVATE
    meshoptimizer::meshoptimizer
    draco::draco
)

# 7. 安装逻辑
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)