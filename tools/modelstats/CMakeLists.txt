# modelstats - read-only load/coordinate forensic probe (see main.cpp for the stage map).
# Mirrors tools/model23dtiles/CMakeLists.txt so it compiles against the same production
# sources; it is an additive tool target and links nothing new.
get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TOOL_NAME ${DIR_NAME})

aux_source_directory(. DIR_SRCS)

# 与 model23dtiles 相同的跨模块引用：src/3dtiles 里的 ProductionPipeline/Tile/Utils
# 依赖这些 gltf 源码，缺了会 LNK2019/LNK2011。
set(GLTF_COMPONENTS_H
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
    ${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
)
set(GLTF_COMPONENTS_SRC
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
    ${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)

file(GLOB_RECURSE THREED_TILES_FILES_H CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/include/3dtiles/*.h"
)
file(GLOB_RECURSE THREED_TILES_FILES_SRC CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp"
)
file(GLOB_RECURSE UTILS_FILES_H CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/include/utils/*.h"
)
file(GLOB_RECURSE UTILS_FILES_SRC CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/utils/*.cpp"
)

add_executable(${TOOL_NAME}
    ${DIR_SRCS}
    ${GLTF_COMPONENTS_H}
    ${GLTF_COMPONENTS_SRC}
    ${THREED_TILES_FILES_H}
    ${THREED_TILES_FILES_SRC}
    ${UTILS_FILES_H}
    ${UTILS_FILES_SRC}
)

target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)
if (DEFINED OSGGISPLUGINS_USE_TBB)
    if (OSGGISPLUGINS_USE_TBB)
        target_compile_definitions(${TOOL_NAME} PRIVATE USE_TBB_PARALLEL=1)
    endif()
else()
    target_compile_definitions(${TOOL_NAME} PRIVATE USE_TBB_PARALLEL=1)
endif()

source_group("include/3dtiles" FILES ${THREED_TILES_FILES_H})
source_group("include/utils" FILES ${UTILS_FILES_H})
source_group("include/gltf" FILES ${GLTF_COMPONENTS_H})
source_group("src" FILES ${DIR_SRCS})
source_group("src/3dtiles" FILES ${THREED_TILES_FILES_SRC})
source_group("src/utils" FILES ${UTILS_FILES_SRC})
source_group("src/gltf" FILES ${GLTF_COMPONENTS_SRC})

find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)

target_include_directories(${TOOL_NAME} PRIVATE
    .
    ${PROJECT_ROOT_DIR}/include
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

target_link_libraries(${TOOL_NAME} PRIVATE
    ${OPENSCENEGRAPH_LIBRARIES}
    meshoptimizer::meshoptimizer
    TBB::tbb
    TBB::tbbmalloc
    TBB::tbbmalloc_proxy
    PROJ::proj
)

if(UNIX AND NOT APPLE)
    target_compile_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_options(${TOOL_NAME} PRIVATE "-no-pie")
    target_link_libraries(${TOOL_NAME} PRIVATE pthread)
endif()

install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION bin)
