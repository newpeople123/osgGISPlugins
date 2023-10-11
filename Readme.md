**简体中文 | [English](#Introduction)**

# 简介
osg的gis插件，能够读取、显示3dmax导出的具有Pbr材质的fbx文件、导出gltf/glb文件、导出b3dm文件、读取/导出ktx2格式图片、读取/导出webp格式图片。  
同时提供了如下子工具：
- `b3dm转glb`
- `3D模型转3dtiles`
- `...`
## osgdb_fbx
在osg的fbx的插件的基础上，参考了[FBX2glTF](https://github.com/facebookincubator/FBX2glTF)项目，使得能够读取3dmax导出的带有Pbr材质的FBX文件，并能在osg中加载。

## osgdb_gltf
支持导出gltf/glb，暂不支持读取。在osgEarth的gltf的插件基础上，增加了纹理压缩和顶点压缩功能，支持webp、ktx2格式纹理；使用draco对顶点、法线、纹理坐标进行压缩；同时支持对顶点绑定batchId。

## osgdb_b3dm
支持导出b3dm，暂不支持读取。b3dm插件具有gltf插件的所有功能，同时能够导出UserData中的属性到b3dm中。

## osgdb_webp
和osgEarth的webp插件一样。

## osgdb_ktx
在王锐大神的[osgVerse](https://github.com/xarray/osgverse)的ktx插件基础上进行了小改动，支持导出ktx2格式的纹理图片。

# 工具简介

## b3dm2glb
将b3dm文件转换为glb文件。  
### 用法说明
#### 命令行格式
`b3dm2glb.exe -i <path> -o <path>`
#### 示例命令
`b3dm2glb.exe -i D:\test.b3dm -o D:\output.glb`
## model23dtiles
将3D模型转换为3dtiles，支持四叉树和八叉树结构的3dtiles，支持webp/ktx2纹理压缩和draco压缩。   
这里的3D模型指的是fbx、obj、3ds等osg能够读取的三维模型，但是不包括倾斜摄影模型，倾斜摄影模型目前建议使用[https://github.com/fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles)。  
### 用法说明
#### 命令行格式
``model23dtiles -i <path> -tf <jpg/png/webp/ktx2> -vf <draco/none> -t <quad/oc> -max <Number> -ratio <Number> -o <DIR> -lat <Number> -lng <Number> -height <Number>``
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
`-tf` 纹理压缩格式，可选值有：png、jpg、webp、ktx2，默认值为：png。  
`-vf` 顶点压缩格式，可选的值有：draco、none，默认值为：none，即不对顶点进行压缩。未来会支持meshopt压缩。  
`-t` 3dtiles的组织结构，可以为四叉树或八叉树，可选的值有：quad、oc，默认值为：quad。  
`-max` b3dm文件所包含的三角面的最大数量，默认值为:40000。  
`-ratio` 3dtiles中间节点的简化比例，默认值为：0.5。  
`-lat` 纬度  
`-lng` 经度  
`-height` 高度  
# 缺陷
1、当前不支持b3dm、gltf/glb文件的导入；  
2、webp插件无法导出灰度图；  
3、ktx插件无法读取部分ktx 2.0版本的图片；  
4、b3dm、gltf/glb文件暂不支持meshopt格式的顶点压缩；  
5、model23tiles工具暂时不支持对小块纹理的合并优化处理。  
...
# 未来计划支持
1、读取b3dm、gltf/glb；  
2、增加对meshopt顶点压缩的支持；  
3、增加model23tiles工具对小块纹理的合并优化处理。  
...  
**[简体中文](#简介) | English**

# Introduction
OSG's GIS plugin can read and display fbx files with Pbr material exported by 3dmax, export gltf/glb files, export b3dm files, read/export ktx2 format images, and read/export webp format images.  
At the same time, the following sub tools are provided:
- `convert b3dm to  glb`
- `convert 3D model to 3dtiles`
- `...`
## osgdb_fbx
Based on the FBX2glTF plugin of OSG's FBX, reference was made to [FBX2glTF](https://github.com/facebookincubator/FBX2glTF) project to enable reading of FBX files with Pbr material exported from 3dmax and loading in OSG.
## osgdb_gltf
Supports exporting gltf/glb, but currently does not support reading. On the basis of osgEarth's gltf plugin, added texture compression and vertex compression functions, supporting webp and ktx2 format textures; Use Draco to compress vertices, normals, and texture coordinates; Simultaneously supports binding batchId to vertices.
## osgdb_b3dm
Supports exporting b3dm, but currently does not support reading. The b3DM plugin has all the functions of the gltf plugin, and can also export attributes from UserData to b3DM.
## osgdb_webp
Same as the webp plugin of osgEarth.
## osgdb_ktx
In xarray's [osgVerse](https://github.com/xarray/osgverse) the project has made minor changes to the ktx plugin, which supports exporting texture images in ktx2 format.

# Tools Introduction

## b3dm2glb
Convert b3dm files to glb.  
### Usage
#### Command Line
`b3dm2glb.exe -i <path> -o <path>`
#### Examples
`b3dm2glb.exe -i D:\test.b3dm -o D:\output.glb`
## model23dtiles
Convert the 3D model into 3D tiles, support quadtree and octree structured 3D tiles, support webp/ktx2 texture compression and draco compression.  
The 3D model here refers to 3D models that can be read by OSG such as FBX, OBJ, and 3ds, but does not include oblique photography models. Currently, it is recommended to use oblique photography models [https://github.com/fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles).
### Usage
#### Command Line
`model23dtiles -i <path> -tf <jpg/png/webp/ktx2> -vf <draco/none> -t <quad/oc> -max <Number> -ratio <Number> -o <DIR> -lat <Number> -lng <Number> -height <Number>`
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
`-tf` texture format,option values are png、jpg、webp、ktx2，default value is png.  
`-vf` vertex format,option values are draco、none,default is none.  
`-t` tree format,option values are quad、oc,default is quad.  
`-max` the maximum number of triangles contained in the b3dm node.default value is 40000.  
`-ratio` Simplified ratio of intermediate nodes.default is 0.5.  
`-lat` latitude  
`-lng` longitude  
`-height` height  
# Defect
1. Currently, importing b3dm and gltf/glb files is not supported;
2. The webp plugin cannot export grayscale images;
3. The ktx plugin cannot read some images from the ktx 2.0 version;
4. The b3dm and gltf/glb files currently do not support vertex compression in the mesh format;
5. The model23tiles tool currently does not support merging and optimizing small textures.  
...
# TO DO
1. Read b3mm, gltf/glb;
2. Increase support for mesh vertex compression;
3. Add the model23tiles tool to merge and optimize small textures.  
...

# About author  
这是作者的第一个开源项目，非常感谢[osg](https://github.com/openscenegraph/OpenSceneGraph)、[osgEarth](https://github.com/gwaldron/osgearth)、[osgVerse](https://github.com/xarray/osgverse)、[Fbx2glTF](https://github.com/facebookincubator/FBX2glTF)、[3dtiles](https://github.com/fanvanzh/3dtiles)等开源项目对我的启发和帮助。
