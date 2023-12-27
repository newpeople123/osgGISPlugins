**简体中文 | [English](#Introduction)**

# 简介
osg的gis插件，能够读取、显示3dmax导出的具有Pbr材质的fbx文件、导出gltf/glb文件、导出b3dm文件、读取/导出ktx2格式图片、读取/导出webp格式图片。  
同时提供了如下子工具：
- `b3dm转gltf/glb`
- `3D模型转3dtiles`
- `...`
## osgdb_fbx
在osg的fbx的插件的基础上，参考了[FBX2glTF](https://github.com/facebookincubator/FBX2glTF)项目，使得能够读取3dmax导出的带有Pbr材质的FBX文件，并能在osg中加载。

## osgdb_gltf
支持导出gltf/glb，暂不支持读取。在osgEarth的gltf的插件基础上，增加了纹理压缩和顶点压缩功能，支持webp、ktx2格式纹理；支持使用draco/meshopt对顶点、法线、纹理坐标进行压缩；同时支持对顶点绑定batchId。

## osgdb_b3dm
支持导出b3dm，暂不支持读取。b3dm插件具有gltf插件的所有功能，同时能够导出UserData中的属性到b3dm中。支持纹理图集优化。

## osgdb_webp
和osgEarth的webp插件一样。

## osgdb_ktx
在王锐大神的[osgVerse](https://github.com/xarray/osgverse)的ktx插件基础上进行了小改动，支持导出ktx2格式的纹理图片。

# 工具简介

## b3dm2gltf
将b3dm文件转换为gltf/glb文件。  
### 用法说明
#### 命令行格式
`b3dm2gltf.exe -i <path> -o <path>`
#### 示例命令
`b3dm2gltf.exe -i D:\test.b3dm -o D:\output.glb`
## model23dtiles
1、将3D模型转换为3dtiles；
2、支持四叉树和八叉树结构的3dtiles；
3、支持webp/ktx2纹理压缩；
4、支持draco和meshoptimizer压缩；
5、支持纹理合并(减少drawcall次数)；
6、导出的3dtiles会带有模型的用户属性；
7、导出的3dtiles是经过网格合并的；
8、支持纹理图集；
...
这里的3D模型指的是fbx、obj、3ds等osg能够读取的三维模型，但是不包括倾斜摄影模型，倾斜摄影模型目前建议使用[https://github.com/fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles)。  
### 用法说明
#### 命令行格式
``model23dtiles -i <path> -tf <jpg/png/webp/ktx2> -vf <draco/meshopt/none> -t <quad/oc> -max <Number> -ratio <Number> -o <DIR> -lat <Number> -lng <Number> -height <Number> -comporess_level <low/medium/high> -multi_threading <true/false>``
#### 示例命令
```sh
model23dtiles.exe -i D:\test.fbx -o D:\output -lat 30 -lng 116 -height 100
# 输出使用ktx2进行纹理压缩和使用draco进行顶点压缩的3dtiles
model23dtiles.exe -i D:\test.fbx -tf ktx2 -vf draco -o D:\output -lat 30 -lng 116 -height 100
# 设置3dtiles的每个节点所包含的三角面的最大数量为10万
model23dtiles.exe -i D:\test.fbx -max 100000 -o D:\output -lat 30 -lng 116 -height 100
# 设置3dtiles的中间节点的简化比例为0.6
model23dtiles.exe -i D:\test.fbx -ratio 0.6 -o D:\output -lat 30 -lng 116 -height 100
# 设置3dtiles的树结构为四叉树
model23dtiles.exe -i D:\test.fbx -t quad -o D:\output -lat 30 -lng 116 -height 100
```
#### 参数说明
`-tf` 纹理压缩格式，可选值有：png、jpg、webp、ktx2，默认值为：jpg。  
`-vf` 顶点压缩格式，可选的值有：draco、meshopt、none，默认值为：none，即不对顶点进行压缩。
`comporess_level` draco压缩级别，可选的值为：low、medium、high，默认值为：medium。
`-t` 3dtiles的组织结构，可以为四叉树或八叉树，可选的值有：quad、oc，默认值为：quad。  
`-max` b3dm文件所包含的三角面的最大数量，默认值为:40000。  
`-ratio` 3dtiles中间节点的简化比例，默认值为：0.5。  
`-lat` 纬度,默认30
`-lng` 经度，默认116
`-height` 高度，默认300
`-multi_threading` 是否启用多线程，默认false
# 编译说明
1、编译需要fbxsdk和修改后的tinygltf等库，但是文件太大无法上传，因此放在了百度网盘中(链接：https://pan.baidu.com/s/1tAy3tAEuAut5GDLODfCKtA?pwd=fgah 
提取码：fgah )，下载解压后放在和src同级目录下即可  
2、编译时需要修改根目录下的CMakeLists文件中CMAKE_TOOLCHAIN_FILE变量的值为本地vcpkg工具路径  
# 缺陷
1、当前不支持b3dm、gltf/glb文件的导入；  
2、ktx插件无法读取部分ktx 2.0版本的图片；  
...
# 未来计划支持
1、读取b3dm、gltf/glb；    
...  
**[简体中文](#简介) | English**

# Introduction
OSG's GIS plugin can read and display fbx files with Pbr material exported by 3dmax, export gltf/glb files, export b3dm files, read/export ktx2 format images, and read/export webp format images.  
At the same time, the following sub tools are provided:
- `convert b3dm to gltf/glb`
- `convert 3D model to 3dtiles`
- `...`
## osgdb_fbx
Based on the FBX2glTF plugin of OSG's FBX, reference was made to [FBX2glTF](https://github.com/facebookincubator/FBX2glTF) project to enable reading of FBX files with Pbr material exported from 3dmax and loading in OSG.
## osgdb_gltf
Supports exporting gltf/glb, but currently does not support reading. On the basis of osgEarth's gltf plugin, added texture compression and vertex compression functions, supporting webp and ktx2 format textures; Use Draco/Meshopt to compress vertices, normals, and texture coordinates; Simultaneously supports binding batchId to vertices.
## osgdb_b3dm
Supports exporting b3dm, but currently does not support reading. The b3DM plugin has all the functions of the gltf plugin, and can also export attributes from UserData to b3DM.Support textureAtlas.
## osgdb_webp
Same as the webp plugin of osgEarth.
## osgdb_ktx
In xarray's [osgVerse](https://github.com/xarray/osgverse) the project has made minor changes to the ktx plugin, which supports exporting texture images in ktx2 format.

# Tools Introduction

## b3dm2gltf
Convert b3dm files to gltf/glb.  
### Usage
#### Command Line
`b3dm2gltf.exe -i <path> -o <path>`
#### Examples
`b3dm2gltf.exe -i D:\test.b3dm -o D:\output.glb`
## model23dtiles
1. Convert 3D models into 3tiles;
2. 3D tiles that support quadtree and octree structures;
3. Support webp/ktx2 texture compression;
4. Support draco and meshopoptimizer compression;
5. Support texture merging (reduce drawcalls);
6. The exported 3dtiles will have the user attributes of the model;
7. The exported 3dtiles are merged through grids;
8. Support textureAtlas;
...
The 3D model here refers to 3D models that can be read by OSG such as FBX, OBJ, and 3ds, but does not include oblique photography models. Currently, it is recommended to use oblique photography models [https://github.com/fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles).
### Usage
#### Command Line
`model23dtiles -i <path> -tf <jpg/png/webp/ktx2> -vf <draco/meshopt/none> -t <quad/oc> -max <Number> -ratio <Number> -o <DIR> -lat <Number> -lng <Number> -height <Number> -compress_level <low/medium/high> -multi_threading <true/false>`
#### Examples
```sh
# output 3dtiles by texture format ktx2 and vertex format draco
model23dtiles.exe -i D:\test.fbx -tf ktx2 -vf draco -o D:\output -lat 30 -lng 116 -height 100
# The maximum number of triangular faces for each b3dm node of the output 3dtiles is 10w
model23dtiles.exe -i D:\test.fbx -max 100000 -o D:\output -lat 30 -lng 116 -height 100
# Set the simplification ratio of the intermediate nodes of the output 3dtiles to 0.6
model23dtiles.exe -i D:\test.fbx -ratio 0.6 -o D:\output -lat 30 -lng 116 -height 100
# Set the tree structure of 3dtiles to a quadtree
model23dtiles.exe -i D:\test.fbx -t quad -o D:\output -lat 30 -lng 116 -height 100
```
#### Parameters
`-tf` texture format,option values are png、jpg、webp、ktx2，default value is jpg.  
`-vf` vertex format,option values are draco、meshopt、none,default is none.  
`-t` tree format,option values are quad、oc,default is quad.  
`compress_level` draco comporession level,option values are low、medium、high,default is medium.
`-max` the maximum number of triangles contained in the b3dm node.default value is 40000.  
`-ratio` Simplified ratio of intermediate nodes.default is 0.5.  
`-lat` latitude,default is 30
`-lng` longitude,default is 116
`-height` height,default is 300
`-multi_threading` Is multithreading enabled,default is false
# Compilation Instructions
1. Compilation requires libraries such as fbxsdk and modified tinygltf, but the file is too large to upload, so it was placed on Baidu's online drive (link: [https://pan.baidu.com/s/1tAy3tAEuAut5GDLODfCKtA?pwd=fgah](https://pan.baidu.com/s/1tAy3tAEuAut5GDLODfCKtA?pwd=fgah),Extract code: fgah). After downloading and decompressing, it can be placed in the same level directory as src  
2. When compiling, it is necessary to modify the CMAKE in the CMakeLists file in the root directory the value of CMAKE_TOOLCHAIN_FILE the variable is the local vcpkg tool path.
# Defect
1. Currently, importing b3dm and gltf/glb files is not supported;
2. The ktx plugin cannot read some images from the ktx 2.0 version;
...
# TO DO
1. Read b3mm, gltf/glb;
...

# Who Use
* 中国电建华东勘测设计研究院

# About author  
这是作者的第一个开源项目，非常感谢[osg](https://github.com/openscenegraph/OpenSceneGraph)、[osgEarth](https://github.com/gwaldron/osgearth)、[osgVerse](https://github.com/xarray/osgverse)、[Fbx2glTF](https://github.com/facebookincubator/FBX2glTF)、[3dtiles](https://github.com/fanvanzh/3dtiles)等开源项目对我的启发和帮助。
