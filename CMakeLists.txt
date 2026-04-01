cmake_minimum_required(VERSION 3.12)

# ==============================================================================
# 1. 预项目配置 (Pre-Project Configuration)
# 设置工具链必须在 project() 之前，以便 CMake 正确初始化编译器环境
# ==============================================================================

# 自动处理 vcpkg 工具链路径
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (WIN32)
        set(DEFAULT_TOOLCHAIN_PATH "E:/SDK/vcpkg/scripts/buildsystems/vcpkg.cmake")
    else()
        set(DEFAULT_TOOLCHAIN_PATH "/home/wty/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()

    if (EXISTS "${DEFAULT_TOOLCHAIN_PATH}")
        set(CMAKE_TOOLCHAIN_FILE "${DEFAULT_TOOLCHAIN_PATH}" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

# ==============================================================================
# 2. 项目声明 (Project Declaration)
# 调用 project() 会触发内部系统检测，自动填充 CMAKE_SYSTEM_NAME 等变量
# ==============================================================================
project(osgGISPlugins LANGUAGES C CXX)

# ==============================================================================
# 3. 全局编译选项与标准
# ==============================================================================
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 启用 compile_commands.json 文件生成，该文件用于代码分析
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS ON)

# Visual Studio 文件夹分组支持
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 项目根目录缩写
set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ==============================================================================
# 4. 平台相关参数判定
# ==============================================================================
if (WIN32)
    # Windows 特有配置
    add_compile_options("/utf-8")
    # 开启 MSVC 的多处理器编译
    add_compile_options("/MP")
    if (MSVC)
        # 开启 MSVC 异常支持
        add_compile_options("/EHsc")
    endif()
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
    
    # 定义 Windows 下的输出目录逻辑（通常用于拷贝依赖）
    set(VCPKG_INSTALL_DIR "${PROJECT_ROOT_DIR}/build/vcpkg_installed/x64-windows")
    message(STATUS "Build Platform: Windows")
else()
    # Linux/Unix 特有配置
    set(VCPKG_TARGET_TRIPLET "x64-linux-dynamic" CACHE STRING "Vcpkg target triplet")
    message(STATUS "Build Platform: Linux/Unix")
endif()

# ==============================================================================
# 5. 设置输出目录 (Artifact Output Directories)
# 针对不同配置（Debug/Release等）统一设置输出路径
# ==============================================================================
set(CONFIGS Debug Release RelWithDebInfo MinSizeRel)
foreach(cfg ${CONFIGS})
    string(TOUPPER ${cfg} CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} "${PROJECT_ROOT_DIR}/${cfg}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG} "${PROJECT_ROOT_DIR}/${cfg}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG} "${PROJECT_ROOT_DIR}/${cfg}")
endforeach()

# 设置生成文件后缀
set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rel")
set(CMAKE_MINSIZEREL_POSTFIX "_min")

# ==============================================================================
# 6. 包含路径与静态检查
# ==============================================================================
include_directories(
    ${PROJECT_ROOT_DIR}/3rdparty/include
    ${PROJECT_ROOT_DIR}/include
)

# 启用 Clang-Tidy (如果环境中存在)
# find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
# if(CLANG_TIDY_EXE)
#     set(CMAKE_CXX_CLANG_TIDY 
#         "${CLANG_TIDY_EXE};--extra-arg=-fexceptions;--extra-arg=-fms-compatibility;--extra-arg=-DWINGDIAPI="
#     )
# endif()

# --- 处理 3rdparty 文件夹 ---
file(GLOB_RECURSE THIRDPARTY_FILES 
    "${PROJECT_ROOT_DIR}/3rdparty/*.h"
    "${PROJECT_ROOT_DIR}/3rdparty/*.cpp"
    "${PROJECT_ROOT_DIR}/3rdparty/*.cmake"
    "${PROJECT_ROOT_DIR}/3rdparty/*.txt"
)

if(THIRDPARTY_FILES)
    # 创建一个不编译的目标
    add_custom_target(3rdparty SOURCES ${THIRDPARTY_FILES})
    # 自动保持 3rdparty 内部的物理目录结构 (需要 CMake 3.8+)
    source_group(TREE "${PROJECT_ROOT_DIR}/3rdparty" FILES ${THIRDPARTY_FILES})
    # 放入 VS 的虚拟文件夹
    set_target_properties(3rdparty PROPERTIES FOLDER "Configurations")
endif()

# --- 处理 assets 资源文件夹 ---
file(GLOB_RECURSE ASSET_FILES 
    "${PROJECT_ROOT_DIR}/assets/*"
)

if(ASSET_FILES)
    add_custom_target(Assets SOURCES ${ASSET_FILES})
    # 在 VS 中按 assets 路径分组
    source_group(TREE "${PROJECT_ROOT_DIR}/assets" FILES ${ASSET_FILES})
    set_target_properties(Assets PROPERTIES FOLDER "Configurations")
endif()

# ==============================================================================
# 定义拷贝函数 (放在项目根目录)
# ==============================================================================
function(copy_osg_dependencies TARGET_NAME)
    if (WIN32)
        set(PLUGIN_SRC "${VCPKG_INSTALL_DIR}/$<$<CONFIG:Debug>:debug/>plugins/osgPlugins-3.6.5")
        set(PLUGIN_DST "${PROJECT_ROOT_DIR}/$<CONFIG>/osgPlugins-3.6.5")

        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            # 创建目录
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_DST}"
            
            # 拷贝整个文件夹
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLUGIN_SRC}" "${PLUGIN_DST}"
            
            # 删除特定文件 (注意：这里必须使用双引号包裹完整路径)
            COMMAND ${CMAKE_COMMAND} -E rm -f "${PLUGIN_DST}/osgdb_ktx.dll"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${PLUGIN_DST}/osgdb_ktxd.dll"
            
            # 拷贝依赖 DLL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALL_DIR}/$<$<CONFIG:Debug>:debug/>bin/jpeg62.dll"
                "${PLUGIN_DST}/"
                
            COMMENT "Deploying OSG dependencies and filtering KTX..."
            VERBATIM # 强烈建议加上这个参数，确保转义正确
        )
    endif()
endfunction()

# ==============================================================================
# 7. 添加子模块
# ==============================================================================
add_subdirectory(src)
add_subdirectory(tools)

# 仅在 Debug 模式下添加测试
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()

# ==============================================================================
# 8. 关键信息打印
# ==============================================================================
message(STATUS "------------------------------------------------------------")
# CMAKE_HOST_SYSTEM_NAME：当前编译代码的机器的操作系统
message(STATUS "Host System:    ${CMAKE_HOST_SYSTEM_NAME}")
# CMAKE_SYSTEM_NAME：想将代码编译为哪种操作系统上可运行的程序
message(STATUS "Target System:  ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Toolchain File: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "------------------------------------------------------------")
