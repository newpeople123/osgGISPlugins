set(TOOL_TEST_NAME "test_3dtiles")

# 查找源文件
aux_source_directory(. DIR_SRCS)
source_group("src" FILES ${DIR_SRCS})

# 定义可执行文件
file(GLOB_RECURSE THREED_TILES_HEADER_FILES ${PROJECT_ROOT_DIR}/include/3dtiles/*.h)
file(GLOB_RECURSE THREED_TILES_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/3dtiles/*.cpp)
file(GLOB UTILS_HEADER_FILES ${PROJECT_ROOT_DIR}/include/utils/*.h)
file(GLOB UTILS_SOURCE_FILES ${PROJECT_ROOT_DIR}/src/utils/*.cpp)
ADD_EXECUTABLE(${TOOL_TEST_NAME} ${DIR_SRCS} 
${UTILS_SOURCE_FILES} ${UTILS_SOURCE_FILES} 
${THREED_TILES_HEADER_FILES} ${THREED_TILES_SOURCE_FILES}
${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h
${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp
)
# 编译当前程序时会先编译这些项目
add_dependencies(${TOOL_TEST_NAME} osgdb_gltf)
add_dependencies(${TOOL_TEST_NAME} osgdb_fbx)
add_dependencies(${TOOL_TEST_NAME} osgdb_ktx)
add_dependencies(${TOOL_TEST_NAME} osgdb_webp)
source_group("include/3dtiles" FILES 
${THREED_TILES_HEADER_FILES}  
${PROJECT_ROOT_DIR}/include/osgdb_gltf/b3dm/BatchIdVisitor.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfMaterial.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrMRMaterial.h 
${PROJECT_ROOT_DIR}/include/osgdb_gltf/material/GltfPbrSGMaterial.h)
source_group("include/util" FILES ${UTILS_HEADER_FILES})

source_group("src" FILES ${DIR_SRCS})
source_group("src/3dtiles" FILES 
${THREED_TILES_SOURCE_FILES} 
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrMRMaterial.cpp 
${PROJECT_ROOT_DIR}/src/osgdb_gltf/material/GltfPbrSGMaterial.cpp)
source_group("src/util" FILES ${UTILS_SOURCE_FILES} ${PROJECT_ROOT_DIR}/src/osgdb_gltf/b3dm/BatchIdVisitor.cpp)


# 查找 meshoptimizer 包
find_package(meshoptimizer CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(PROJ CONFIG REQUIRED)
target_link_libraries(${TOOL_TEST_NAME} 
    meshoptimizer::meshoptimizer
    TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
    PROJ::proj
)

# 如果是 Linux 系统，链接 pthread 库
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_TEST_NAME} pthread)
endif()

if (OPENSCENEGRAPH_FOUND)
    message(STATUS "Found OpenSceneGraph")
    message(STATUS "OPENSCENEGRAPH_INCLUDE_DIRS: ${OPENSCENEGRAPH_INCLUDE_DIRS}")
    message(STATUS "OPENSCENEGRAPH_LIBRARIES: ${OPENSCENEGRAPH_LIBRARIES}")

    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_TEST_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_TEST_NAME}: OpenSceneGraph not found!")
endif()
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/share/proj/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/Debug/share/proj)
    else()
        file(GLOB THIRD_PARTY_LIBS_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows/share/proj/*")
        file(COPY ${THIRD_PARTY_LIBS_DIR} DESTINATION ${PROJECT_ROOT_DIR}/Release/share/proj)
    endif()
endif()
# 安装目标
install(TARGETS ${TOOL_TEST_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
