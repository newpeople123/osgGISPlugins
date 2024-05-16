set(TOOL_NAME "gltfpack")
aux_source_directory(. DIR_SRCS)

ADD_EXECUTABLE(${TOOL_NAME} ${DIR_SRCS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${CMAKE_TOOLCHAIN_FILE})
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TOOL_NAME} meshoptimizer::meshoptimizer)
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})