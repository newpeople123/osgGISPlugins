<center><h1><b>osgGISPlugins</b></h1></center>

<div align="center">
  <img src="https://img.shields.io/badge/Version-2.0.0-blue.svg" />
  <img src="https://img.shields.io/badge/C++-C++11-green.svg" />
  <img src="https://img.shields.io/badge/license-MIT-green.svg" />
  <img src="https://img.shields.io/badge/platform-windows-green.svg" />
  <img src="https://img.shields.io/badge/platform-linux-green.svg" />
</div>
<div align="left">
  <img src="https://img.shields.io/badge/feature-三维模型轻量化-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-更高效的3dtiles-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-纹理图集-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-gltf/glb/b3dm/i3dm-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-read fbx with pbr material-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-ktx2-yellow.svg" />
  <img src="https://img.shields.io/badge/feature-webp-yellow.svg" />
</div>
<div align="left">
  <img src="https://img.shields.io/badge/dependencies-osg-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-libwebp-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-ktx-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-draco-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-meshoptimizer-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-stb-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-tbb-orange.svg" />
  <img src="https://img.shields.io/badge/dependencies-tinygltf-orange.svg" />
</div>

# 简介
<div align="left">
  <img src="https://img.shields.io/badge/plugin-osgdb_fbx-brightgreen.svg" />
  <img src="https://img.shields.io/badge/plugin-osgdb_gltf-brightgreen.svg" />
  <img src="https://img.shields.io/badge/plugin-osgdb_webp-brightgreen.svg" />
  <img src="https://img.shields.io/badge/plugin-ktx-brightgreen.svg" />
</div>
<div align="left">
  <img src="https://img.shields.io/badge/tool-b3dm2gltf-brightgreen.svg" />
  <img src="https://img.shields.io/badge/tool-model23dtiles-brightgreen.svg" />
  <img src="https://img.shields.io/badge/tool-simplifier-brightgreen.svg" />
  <img src="https://img.shields.io/badge/tool-texturepacker-brightgreen.svg" />
</div>

osg的gis插件，能够读取、显示3dmax导出的具有PBR材质的fbx文件、导出gltf/glb/b3dm/i3dm文件、读取/导出ktx2格式图片、读取/导出webp格式图片。同时提供了如下子工具：

- `b3dm转gltf/glb`
- `3D模型转3dtiles`
- `3D模型简化`
- `多张纹理打包成一个纹理图集`

## osgdb_fbx 
<div align="left">
  <img src="https://img.shields.io/badge/read-fbx-brightgreen.svg" />
  <img src="https://img.shields.io/badge/write-fbx-brightgreen.svg" />
</div>

在osg的fbx的插件的基础上，参考了[FBX2glTF](https://github.com/facebookincubator/FBX2glTF)项目，使得能够读取3dmax导出的带有PBR材质的FBX文件，并能在osg中加载。

## osgdb_gltf
<div align="left">
  <img src="https://img.shields.io/badge/read-gltf/glb/b3dm/i3dm-red.svg" />
  <img src="https://img.shields.io/badge/write-gltf/glb/b3dm/i3dm-brightgreen.svg" />
</div>
<div align="left">
  <img src="https://img.shields.io/badge/extension-KHR_materials_unlit-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-KHR_materials_pbrSpecularGlossiness-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-KHR_draco_mesh_compression-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-KHR_mesh_quantization-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-EXT_meshopt_compression-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-KHR_texture_basisu-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-KHR_texture_transform-brightgreen.svg" />
  <img src="https://img.shields.io/badge/extension-EXT_texture_webp-brightgreen.svg" />
</div>
<div align="left">
  <img src="https://img.shields.io/badge/optimize-primitive-brightgreen.svg" />
  <img src="https://img.shields.io/badge/optimize-mesh-brightgreen.svg" />
  <img src="https://img.shields.io/badge/optimize-texture-brightgreen.svg" />
  <img src="https://img.shields.io/badge/optimize-material-brightgreen.svg" />
</div>

支持导出gltf/glb/b3dm/i3dm，暂不支持读取。

在osgEarth的gltf的插件基础上，增加了若干gltf扩展：

1、KHR_materials_unlit

2、KHR_materials_pbrSpecularGlossiness

3、KHR_draco_mesh_compression

4、KHR_mesh_quantization

5、EXT_meshopt_compression

6、KHR_texture_basisu

7、KHR_texture_transform

8、EXT_texture_webp

通过合并材质、合并几何图元等方式优化gltf性能，减少drawcall的调用次数。

## osgdb_webp
<div align="left">
  <img src="https://img.shields.io/badge/read-webp-brightgreen.svg" />
  <img src="https://img.shields.io/badge/write-webp-brightgreen.svg" />
</div>

和osgEarth的webp插件一样。

## osgdb_ktx
<div align="left">
  <img src="https://img.shields.io/badge/read-ktx/ktx2-brightgreen.svg" />
  <img src="https://img.shields.io/badge/write-ktx/ktx2-brightgreen.svg" />
</div>

在王锐大神的[osgVerse](https://github.com/xarray/osgverse)的ktx插件基础上进行了小改动，支持导出ktx2格式的纹理图片，支持Mipmaps。

# 工具简介

## b3dm2gltf

将b3dm文件转换为gltf/glb文件。

### 用法说明

#### 命令行格式

`b3dm2gltf.exe -i <path> -o <path>`

#### 示例命令

`b3dm2gltf.exe -i D:\test.b3dm -o D:\output.glb`

## model23dtiles

1、将3D模型转换为3dtiles 1.0；
2、支持四叉树和八叉树结构的3dtiles；
3、支持webp/ktx2纹理压缩；
4、支持draco和meshoptimizer压缩及顶点量化；
5、支持纹理合并(减少drawcall次数)；
6、导出的3dtiles会带有模型的用户属性；
7、导出的3dtiles是经过网格合并的；
8、支持纹理图集；
...
这里的3D模型指的是fbx、obj、3ds等osg能够读取的三维模型，但是不包括倾斜摄影模型，倾斜摄影模型目前建议使用[https://github.com/fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles)。

### 用法说明

#### 命令行格式

``model23dtiles -i <path> -tf <jpg/png/webp/ktx2> -vf <draco/meshopt/quantize/quantize_meshopt> -t <quad/oc> -ratio <Number> -o <DIR> -lat <Number> -lng <Number> -height <Number> -comporessLevel <low/medium/high> -translationX 0.0 -translationY 0.0 =translationZ 0.0 -upAxis Z -maxTextureWidth 256 -maxTextureHeight 256 -maxTextureAtlasWidth 2048 -maxTextureAtlasHeight 2048``

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
`-vf` 顶点压缩格式，可选的值有：draco、meshopt、quantize、quantize_meshopt，无默认值，即不对顶点进行压缩。
`comporessLevel` draco压缩级别/顶点量化级别，可选的值为：low、medium、high，默认值为：medium，仅当vf的值为quantize、quantize_meshopt和draco时生效。
`-t` 3dtiles的组织结构，可以为四叉树或八叉树，可选的值有：quad、oc，默认值为：quad。
`-ratio` 3dtiles中间节点的简化比例，默认值为：0.5。
`-lat` 纬度,默认30。
`-lng` 经度，默认116。
`-height` 高度，默认300。
`-translationX` 重设模型原点位置的x坐标，默认值为0。
`-translationY` 重设模型原点位置的y坐标，默认值为0。
`-translationZ` 重设模型原点位置的z坐标，默认值为0。
`-upAxis` 模型向上方向，可选的只有：X、Y、Z，需大写，默认值为：Y。
`-maxTextureWidth` 单个纹理的最大宽度，默认值为256，需为2的幂次。
`-maxTextureHeight` 单个纹理的最大高度，默认值为256，需为2的幂次。
`-maxTextureAtlasWidth` 纹理图集的最大宽度，默认值为2048，需为2的幂次，且值要大于maxTextureWidth的值，否则将不会构建纹理图集。
`-maxTextureAtlasHeight` 纹理图集的最大高度，默认值为2048，需为2的幂次，且值要大于maxTextureWidth的值，否则将不会构建纹理图集。

## simplifier

对3D模型进行网格简化操作，同时会删除简化后的空闲顶点。

### 用法说明

#### 命令行格式

`simplifier.exe -i <path> -o <path> -ratio 0.1 -aggressive`

#### 示例命令

`simplifier.exe -i C:\\input\\test.fbx -o C:\\output\\test_05.fbx -ratio 0.1`

#### 参数说明

`-i` 输入3D模型。

`-o` 简化后的3D模型。

`-ratio` 简化比例。

`aggressive` 更激进的简化方式，不保留拓扑。

## texturepacker

将多张纹理图片打包成一个纹理图集，并输出一个json文件指示原始纹理图片在纹理图集中的位置。

### 用法说明

#### 命令行格式

`texturepacker.exe -i <path> -o <path> -width <number> -height <number>`

#### 示例命令

`texturepacker.exe -i C:\\input -o C:\\output\\atlas.png -width 2048 -height 2048`

#### 参数说明

`-i` 输入待打包纹理图片或其所在文件夹。

`-o` 输出纹理图集。

`-width` 纹理图集最大宽度。

`-height` 纹理图集最大高度。

# 如何编译
<div align="left">
  <img src="https://img.shields.io/badge/build-visual studio-blue.svg" />
  <img src="https://img.shields.io/badge/build-cmake-blue.svg" />
  <img src="https://img.shields.io/badge/build-vcpkg-blue.svg" />
  <img src="https://img.shields.io/badge/build-docker-blue.svg" />
</div>

1、编译需要fbxsdk和修改后的tinygltf等库，但是文件太大无法上传，因此放在了百度网盘中(链接：https://pan.baidu.com/s/1tAy3tAEuAut5GDLODfCKtA?pwd=fgah
提取码：fgah )，下载解压后放在和src同级目录下即可；
2、编译时需要修改根目录下的CMakeLists文件中CMAKE_TOOLCHAIN_FILE变量的值为本地vcpkg工具路径；

其他方式：

1、windows环境下修改build.bat脚本的DCMAKE_TOOLCHAIN_FILE值为本地vcpkg工具路径，运行脚本即可编译；

2、linux环境下通过Dockerfile文件构建docker镜像即可在docker容器中运行；

# 缺陷

1、当前不支持i3dm、b3dm、gltf/glb文件的导入；
2、model23dtiles不支持构建kd树；
...

# 关于作者

这是作者的第一个开源项目，非常感谢[osg](https://github.com/openscenegraph/OpenSceneGraph)、[osgEarth](https://github.com/gwaldron/osgearth)、[osgVerse](https://github.com/xarray/osgverse)、[Fbx2glTF](https://github.com/facebookincubator/FBX2glTF)、[3dtiles](https://github.com/fanvanzh/3dtiles)等开源项目对我的启发和帮助。
