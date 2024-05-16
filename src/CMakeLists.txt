SET(TARGET_DEFAULT_LABEL_PREFIX "Plugins")
# 这个很关键，解决vcpkg安装了包但是find_package命令找不到包的问题
include(${CMAKE_TOOLCHAIN_FILE})
#查找OpenSceneGraph库的核心组件
#-----
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgViewer osgText osgAnimation)
#设置全局包含路径,必须放在添加子目录之前，不然子目录读取不到
include_directories(${PROJECT_ROOT_DIR}/3rdparty/include ${CMAKE_CURRENT_SOURCE_DIR}/../include)
#设置动态库的前缀为空，windows环境下默认为空，但是linux环境下默认为"lib"，但是osg查找插件的动态库时动态库不能有前缀
set(CMAKE_SHARED_LIBRARY_PREFIX "") 


add_subdirectory(osgdb_fbx)
add_subdirectory(osgdb_ktx)
add_subdirectory(osgdb_webp)
add_subdirectory(osgdb_gltf)
add_subdirectory(osgdb_b3dm)

