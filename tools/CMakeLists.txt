SET(TARGET_DEFAULT_LABEL_PREFIX "Tools")
#查找OpenSceneGraph库的核心组件
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgUtil osgGA osgText osgAnimation osgViewer)
add_subdirectory(b3dm2gltf)
add_subdirectory(model23dtiles)
add_subdirectory(simplifier)
add_subdirectory(texturepacker)

set_target_properties(b3dm2gltf PROPERTIES FOLDER "Tools")
set_target_properties(model23dtiles PROPERTIES FOLDER "Tools")
set_target_properties(simplifier PROPERTIES FOLDER "Tools")
set_target_properties(texturepacker PROPERTIES FOLDER "Tools")

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
	add_subdirectory(gui)
	set_target_properties(gui PROPERTIES FOLDER "Tools")
endif()
