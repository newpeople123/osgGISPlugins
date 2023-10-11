SET(TARGET_DEFAULT_LABEL_PREFIX "Plugins")
#find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
#target_include_directories(main PRIVATE ${TINYGLTF_INCLUDE_DIRS})
# 这个很关键，解决vcpkg安装了包但是find_package命令找不到包的问题
include(${CMAKE_TOOLCHAIN_FILE})
#查找OpenSceneGraph库的核心组件
#-----
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgViewer osgText osgAnimation)
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${CMAKE_CURRENT_SOURCE_DIR}/../include)
add_subdirectory(osgdb_fbx)
add_subdirectory(osgdb_ktx)
add_subdirectory(osgdb_webp)
add_subdirectory(osgdb_gltf)
add_subdirectory(osgdb_b3dm)
# 获取所有子目录
#file(GLOB SUBDIRECTORIES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
#foreach(SUBDIR ${SUBDIRECTORIES})
    # 判断是否是目录
 #   if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        # 添加子目录
#        add_subdirectory(${SUBDIR})
 #   endif()
#endforeach()
