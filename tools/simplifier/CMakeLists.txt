set(TOOL_NAME "simplifier")

# 查找源文件
aux_source_directory(. DIR_SRCS)

file(GLOB_RECURSE UTILS_HEADERS ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB_RECURSE UTILS_SOURCES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)

# GLTF 材质相关
set(GLTF_MATERIAL_HEADERS
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)

set(GLTF_MATERIAL_SOURCES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

# 汇总所有源文件
set(SOURCE_FILES
    ${DIR_SRCS}
    ${UTILS_SOURCES}
    ${UTILS_HEADERS}
    ${GLTF_MATERIAL_HEADERS}
    ${GLTF_MATERIAL_SOURCES}
)

# 添加可执行文件
add_executable(${TOOL_NAME} ${SOURCE_FILES})

# 添加依赖
add_dependencies(${TOOL_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 查找并链接第三方库
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

target_link_libraries(${TOOL_NAME}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread)
endif()

# 添加头文件目录（仅限当前目标）
target_include_directories(${TOOL_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/include
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf
)

# OpenSceneGraph 配置
if (OPENSCENEGRAPH_FOUND)
    message(STATUS "Found OpenSceneGraph")
    message(STATUS "OPENSCENEGRAPH_INCLUDE_DIRS: ${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message(STATUS "OPENSCENEGRAPH_LIBRARIES: ${OPENSCENEGRAPH_LIBRARIES}")

    target_include_directories(${TOOL_NAME} PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: OpenSceneGraph not found!")
endif()

# 设置 Visual Studio 的分组显示（source_group）
source_group("include\\utils" FILES ${UTILS_HEADERS})
source_group("src\\utils"     FILES ${UTILS_SOURCES})
source_group("include\\material" FILES ${GLTF_MATERIAL_HEADERS})
source_group("src\\material"     FILES ${GLTF_MATERIAL_SOURCES})
source_group("src" FILES ${DIR_SRCS})

# 安装目标
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
