set(TOOL_NAME "gltfpack")
project(${TOOL_NAME})
aux_source_directory(. DIR_SRCS)

ADD_EXECUTABLE(${TOOL_NAME} ${DIR_SRCS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${CMAKE_TOOLCHAIN_FILE})
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TOOL_NAME} meshoptimizer::meshoptimizer)
