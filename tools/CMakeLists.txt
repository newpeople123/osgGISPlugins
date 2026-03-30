# ==============================================================================
# 1. 查找依赖
# ==============================================================================
find_package(OpenSceneGraph 3.6.5 REQUIRED 
    osg osgDB osgUtil osgGA osgText osgAnimation osgViewer
)

# ==============================================================================
# 2. 自动遍历并添加子目录
# ==============================================================================
# 获取当前路径下所有的文件和文件夹
file(GLOB ALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

foreach(item ${ALL_FILES})
    # 检查是否为目录，并且该目录下存在 CMakeLists.txt 文件
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${item} AND 
       EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${item}/CMakeLists.txt)
        
        message(STATUS "Found Tool Subdirectory: ${item}")
        add_subdirectory(${item})
        
        # 顺便为该子目录下的所有 target 设置 VS 文件夹分组
        # 注意：此处的属性设置仅对在 add_subdirectory 运行期间定义的 target 有效
        get_directory_property(sub_targets DIRECTORY ${item} BUILDSYSTEM_TARGETS)
        foreach(target ${sub_targets})
            set_target_properties(${target} PROPERTIES FOLDER "Tools")
        endforeach()

    endif()
endforeach()

# ==============================================================================
# 3. 兜底属性设置 (确保所有子项目目标都在 Tools 分组下)
# ==============================================================================
# 如果子目录内部还有嵌套，可以在这里统一再次刷新
get_directory_property(CURRENT_TOOLS_TARGETS BUILDSYSTEM_TARGETS)
foreach(target ${CURRENT_TOOLS_TARGETS})
    set_target_properties(${target} PROPERTIES FOLDER "Tools")
endforeach()