SET(TARGET_DEFAULT_LABEL_PREFIX "Plugins")
#查找OpenSceneGraph库的核心组件
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgViewer osgText osgAnimation)
#设置动态库的前缀为空，windows环境下默认为空，但是linux环境下默认为"lib"，但是osg查找插件的动态库时动态库不能有前缀
set(CMAKE_SHARED_LIBRARY_PREFIX "") 

add_subdirectory(osgdb_fbx)
add_subdirectory(osgdb_ktx)
add_subdirectory(osgdb_webp)
add_subdirectory(osgdb_gltf)

set_target_properties(osgdb_fbx PROPERTIES FOLDER "Plugins")
set_target_properties(osgdb_ktx PROPERTIES FOLDER "Plugins")
set_target_properties(osgdb_webp PROPERTIES FOLDER "Plugins")
set_target_properties(osgdb_gltf PROPERTIES FOLDER "Plugins")
