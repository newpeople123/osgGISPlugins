cmake_minimum_required(VERSION 3.12)

# 项目名和C++标准
set(TOOL_NAME "gui")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
endif()

# 查找当前目录下所有源文件（cpp/c等）
aux_source_directory(. DIR_SRCS)

# 头文件列表，必须添加进 add_executable 才能在 IDE 中显示
set(HEADER_FILES
    ${PROJECT_ROOT_DIR}/3rdparty/include/ImGuiFileDialog/ImGuiFileDialog.h
    ${PROJECT_ROOT_DIR}/3rdparty/include/ImGuiFileDialog/stb/stb_image.h
)

# 添加可执行文件，包含源文件和头文件
add_executable(${TOOL_NAME}
    ${DIR_SRCS}
    ${HEADER_FILES}
    ${PROJECT_ROOT_DIR}/3rdparty/lib/ImGuiFileDialog/ImGuiFileDialog.cpp
)
target_compile_features(${TOOL_NAME} PRIVATE cxx_std_17)

# 通过 source_group 按分组显示
source_group("include" FILES
    ${PROJECT_ROOT_DIR}/3rdparty/include/ImGuiFileDialog/ImGuiFileDialog.h
    ${PROJECT_ROOT_DIR}/3rdparty/include/ImGuiFileDialog/stb/stb_image.h
)

source_group("src" FILES
    ${DIR_SRCS}
    ${PROJECT_ROOT_DIR}/3rdparty/lib/ImGuiFileDialog/ImGuiFileDialog.cpp
)

# 查找依赖库
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# 添加构建依赖，保证顺序
add_dependencies(${TOOL_NAME}
    b3dm2gltf
    model23dtiles
    simplifier
    texturepacker
)

# 链接库
target_link_libraries(${TOOL_NAME}
    imgui::imgui
    glfw
    OpenGL::GL
)

# OpenSceneGraph 支持
if (OPENSCENEGRAPH_FOUND)
    include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
    target_link_libraries(${TOOL_NAME} ${OPENSCENEGRAPH_LIBRARIES})
else()
    message(FATAL_ERROR "${TOOL_NAME}: osg not found!")
endif()

# Linux 平台添加 pthread、dl
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TOOL_NAME} pthread dl)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OUT_DIR "${PROJECT_ROOT_DIR}/Debug")
else()
    set(OUT_DIR "${PROJECT_ROOT_DIR}/Release")
endif()
file(COPY ${PROJECT_ROOT_DIR}/assets/favicon.png DESTINATION ${OUT_DIR})
file(COPY ${PROJECT_ROOT_DIR}/assets/msyh.ttc DESTINATION ${OUT_DIR})

# 安装配置
install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
