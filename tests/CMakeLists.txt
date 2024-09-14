SET(TARGET_DEFAULT_LABEL_PREFIX "Tests")
#查找OpenSceneGraph库的核心组件
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgViewer osgText osgAnimation)
add_subdirectory(osgdb_fbx)
add_subdirectory(osgdb_gltf)
add_subdirectory(osgdb_ktx)
add_subdirectory(3dtiles)

set_target_properties(test_osgdb_fbx PROPERTIES FOLDER "Tests/Plugins")
set_target_properties(test_osgdb_gltf PROPERTIES FOLDER "Tests/Plugins")
set_target_properties(test_osgdb_ktx PROPERTIES FOLDER "Tests/Plugins")
set_target_properties(test_3dtiles PROPERTIES FOLDER "Tests/Tools")
