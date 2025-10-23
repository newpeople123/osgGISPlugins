cmake_minimum_required(VERSION 3.12)
set(TOOL_NAME "model23dtiles")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
endif()
# 查找测试目录下源文件
aux_source_directory(. DIR_SRCS)

# 路径统一变量
set(B3DM_HEADER ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h)
set(B3DM_SOURCE ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp)

set(MATERIAL_HEADERS
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)

set(MATERIAL_SOURCES
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

file(GLOB_RECURSE THREED_TILES_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
file(GLOB_RECURSE THREED_TILES_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp)
file(GLOB_RECURSE UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB_RECURSE UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)

# 添加可执行文件
add_executable(${TOOL_NAME}
    ${DIR_SRCS}
    ${THREED_TILES_HEADER_FILES}
    ${THREED_TILES_SOURCE_FILES}
    ${UTILS_HEADER_FILES}
    ${UTILS_SOURCE_FILES}
    ${B3DM_HEADER} ${B3DM_SOURCE}
    ${MATERIAL_HEADERS} ${MATERIAL_SOURCES}
)
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)

# 启用并行处理
target_compile_definitions(${TOOL_NAME} PRIVATE USE_TBB_PARALLEL=1)

# 添加依赖
add_dependencies(${TOOL_NAME}
    osgdb_gltf
    osgdb_fbx
    osgdb_ktx
    osgdb_webp
)

# 分组（VS用）
source_group("src" FILES ${DIR_SRCS})

source_group("include\\3dtiles" FILES ${THREED_TILES_HEADER_FILES})
source_group("include\\utils" FILES ${UTILS_HEADER_FILES})
source_group("include\\gltf\\b3dm" FILES ${B3DM_HEADER})
source_group("include\\gltf\\material" FILES ${MATERIAL_HEADERS})

source_group("src\\3dtiles" FILES ${THREED_TILES_SOURCE_FILES})
source_group("src\\utils" FILES ${UTILS_SOURCE_FILES})
source_group("src\\gltf\\b3dm" FILES ${B3DM_SOURCE})
source_group("src\\gltf\\material" FILES ${MATERIAL_SOURCES})

# 查找依赖库
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)

# 链接库
target_link_libraries(${TOOL_NAME}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
    PROJ::proj
)

# Linux 平台添加 pthread
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread)
endif()

# OpenSceneGraph 处理
if (OPENSCENEGRAPH_FOUND)
    message(STATUS "Found OpenSceneGraph")
    message(STATUS "OPENSCENEGRAPH_INCLUDE_DIRS: ${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message(STATUS "OPENSCENEGRAPH_LIBRARIES: ${OPENSCENEGRAPH_LIBRARIES}")
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: OpenSceneGraph not found!")
endif()

# Windows 下拷贝 PROJ 数据文件（仅一次 GLOB）
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    set(PROJ_SHARE_SRC "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/share/proj/*")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(PROJ_SHARE_DEST "${PROJECT_ROOT_DIR}/Debug/share/proj")
    else()
        set(PROJ_SHARE_DEST "${PROJECT_ROOT_DIR}/Release/share/proj")
    endif()
    file(GLOB PROJ_FILES ${PROJ_SHARE_SRC})
    file(COPY ${PROJ_FILES} DESTINATION ${PROJ_SHARE_DEST})
endif()

# 安装
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
