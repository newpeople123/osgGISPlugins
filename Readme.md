<center><h1><b>osgGISPlugins</b></h1></center>

<div align="center">
  <img src="https://img.shields.io/badge/Version-2.0.0-blue.svg" />
  <img src="https://img.shields.io/badge/C++-C++11-green.svg" />
  <img src="https://img.shields.io/badge/license-MIT-green.svg" />
  <img src="https://img.shields.io/badge/platform-windows/linux-green.svg" />
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

osg的GIS插件，能够读取、显示3dsmax导出的具有PBR材质的fbx文件、导出gltf/glb/b3dm/i3dm文件、读取/导出ktx2格式图片、读取/导出webp格式图片。同时提供了如下子工具：

- b3dm转gltf/glb
- 3D模型转3dtiles
- 3D模型简化
- 多张纹理打包成一个纹理图集

## osgdb_fbx

<div align="left">
  <img src="https://img.shields.io/badge/read-fbx-brightgreen.svg" />
  <img src="https://img.shields.io/badge/write-fbx-brightgreen.svg" />
</div>

在osg的fbx的插件的基础上，参考了[FBX2glTF](https://github.com/facebookincubator/FBX2glTF)项目，使得能够读取3dmax导出的带有PBR材质的FBX文件(原本的插件读取带有PBR材质的FBX文件时，材质会丢失，读到引擎里显示为白模)，并能在osg引擎中正常渲染。

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

- KHR_materials_unlit
- KHR_materials_pbrSpecularGlossiness
- KHR_draco_mesh_compression
- KHR_mesh_quantization
- EXT_meshopt_compression
- KHR_texture_basisu
- KHR_texture_transform
- EXT_texture_webp

通过展开/合并变换矩阵、合并材质、合并几何图元等方式优化gltf性能，减少drawcall的调用次数。

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

```sh
b3dm2gltf.exe -i <path> -o <path>
```

#### 示例命令

```sh
b3dm2gltf.exe -i D:\test.b3dm -o D:\output.glb
```

## model23dtiles

- 将3D模型转换为符合 3dtiles 1.0 标准的瓦片数据；
- 支持三种3dtiles空间组织结构：KD树（kd）、四叉树（quad） 和 八叉树（oc）；
- 支持纹理压缩格式：webp 和 ktx2，也支持传统的 jpg、png；
- 支持顶点数据压缩：draco、meshoptimizer、量化（quantize）及其组合；
- 支持纹理合并，减少绘制调用次数（drawcall），提升渲染效率；
- 导出的3dtiles会携带模型的用户自定义属性；
- 对网格数据进行合并，生成优化的模型瓦片；
- 支持纹理图集（Texture Atlas）以进一步优化纹理加载。
  **特别说明**
  这里的3D模型指的是fbx、obj、3ds等单个模型文件，但不包括倾斜摄影模型，倾斜摄影模型目前建议使用[fanvanzh/3dtiles](https://github.com/fanvanzh/3dtiles)。

### 用法说明

#### 命令行格式

```sh
model23dtiles -i <path> -o <DIR> -tf <jpg/png/webp/ktx2> -vf <draco/meshopt/quantize/quantize_meshopt> -t <quad/oc/kd> -sr <Number> -cl <low/medium/high> -tx <Number> -ty <Number> -tz <Number> -up <X/Y/Z> -tw <Number> -th <Number> -aw <Number> -ah <Number> -tri <Number> -dc <Number> -sx <Number> -sy <Number> -sz <Number> -lng <Number> -alt <Number> -nm <v/f> -nrm -unlit -ntf
```

或

```sh
model23dtiles -i <path> -o <DIR> -tf <jpg/png/webp/ktx2> -vf <draco/meshopt/quantize/quantize_meshopt> -t <quad/oc/kd> -sr <Number> -cl <low/medium/high> -tx <Number> -ty <Number> -tz <Number> -up <X/Y/Z> -tw <Number> -th <Number> -aw <Number> -ah <Number> -tri <Number> -dc <Number> -sx <Number> -sy <Number> -sz <Number> -epsg <Number> -nm <v/f> -nrm -unlit -ntf
```

#### 示例命令

```sh
model23dtiles.exe -i D:\test.fbx -o D:\output -lat 30 -lng 116 -alt 100
# 输出使用ktx2进行纹理压缩和使用draco进行顶点压缩的3dtiles
model23dtiles.exe -i D:\test.fbx -tf ktx2 -vf draco -o D:\output -lat 30 -lng 116 -alt 100.5
# 设置3dtiles的中间节点的简化比例为0.6
model23dtiles.exe -i D:\test.fbx -sr 0.6 -o D:\output -lat 30 -lng 116 -alt 100
# 设置3dtiles的树结构为四叉树，顶点坐标为4549投影坐标系
model23dtiles.exe -i D:\test.fbx -t quad -o D:\output -epsg 4549
# 设置3dtiles的树结构为四叉树，顶点坐标为4549投影坐标系，并将原始模型单位从厘米转换为米
model23dtiles.exe -i D:\test.fbx -t quad -o D:\output -epsg 4549 -sx 0.01 -sy 0.01 -sz 0.01
```

#### 参数说明
- **输入输出**

  - `-i` 输入文件
  - `-o` 输出文件夹
- **坐标参数**

  - `-lat` 纬度，默认30.0
  - `-lng` 经度，默认116.0
  - `-alt` 高度，默认300
  - `-epsg` 若模型顶点坐标为投影坐标系，指定epsg编码，与lat、lng和alt参数互斥，可以配合tx、ty、tz参数使用
- **变换参数**

  - `-tx` 重设模型原点位置的x坐标，默认0.0
  - `-ty` 重设模型原点位置的y坐标，默认0.0
  - `-tz` 重设模型原点位置的z坐标，默认0.0
  - `-sx` x方向缩放（单位转换），默认1.0
  - `-sy` y方向缩放（单位转换），默认1.0
  - `-sz` z方向缩放（单位转换），默认1.0
  - `-up` 模型向上方向轴，选项：X、Y、Z（大写），默认Y（FBX模型自动转换为Y轴向上）
- **组织结构参数**

  - `-t` 3dtiles组织结构，可选：kd（KD树）、quad（四叉树）、oc（八叉树），默认quad
- **压缩与简化参数**

  - `-r` 3dtiles中间节点简化比例，默认0.5
  - `-tf` 纹理压缩格式，可选：png、jpg、webp、ktx2，默认ktx2
  - `-vf` 顶点压缩格式，可选：draco、meshopt、quantize、quantize_meshopt，无默认值
  - `-cl` draco压缩级别/顶点量化级别，选项：low、medium、high，默认medium，仅对quantize、quantize_meshopt和draco有效；压缩级别越高，模型精度损失越大
- **性能限制参数**

  - `-tri` 3dtiles瓦片最大三角面数，默认20w
  - `-dc` 3dtiles瓦片最大drawcall数量，默认20
  - **纹理尺寸参数**
    - `-tw` 单个纹理最大宽度，默认256，需为2的幂
    - `-th` 单个纹理最大高度，默认256，需为2的幂
    - `-aw` 纹理图集最大宽度，默认2048，需为2的幂，且大于单个纹理最大宽度，否则不构建图集
    - `-ah` 纹理图集最大高度，默认2048，需为2的幂，且大于单个纹理最大高度，否则不构建图集
  - **变换参数**
    - `-nft` 不对顶点应用变换矩阵；默认会对顶点应用变换矩阵以提升渲染性能(减少drawcall)，但可能带来顶点位置精度损失(如果不启用该参数时，模型发生了变形，请启用该参数)
- **其他参数**

  - `-nrm` 重新计算法线
  - `-nm` 配合 `-nrm`参数使用，指定法线模式：`v`表示顶点法线，`f`表示面法线（默认），可选：v、f（顶点法线适用于平滑曲面，面法线适用于棱角分明的物体）
  - `-unlit` 启用 `KHR_materials_unlit` 扩展，适用于烘焙模型
  - `-gn` 生成法线贴图（使用Sobel算子）和切线，瓦片会有更好的渲染效果（提升有限）但瓦片的体积也会变大并且处理时间也更长

## simplifier

对3D模型进行网格简化操作，同时会删除简化后的空闲顶点。

### 用法说明

#### 命令行格式

```sh
simplifier.exe -i <path> -o <path> -ratio <Number> -aggressive
```

#### 示例命令

```sh
simplifier.exe -i C:\input\test.fbx -o C:\output\test_05.fbx -ratio 0.1
```

#### 参数说明

`-i` 输入3D模型。

`-o` 简化后的3D模型。

`-ratio` 简化比例。

`-aggressive` 更激进的简化方式，不保留拓扑。

## texturepacker

将多张纹理图片打包成一个纹理图集，并输出一个json文件指示原始纹理图片在纹理图集中的位置。

### 用法说明

#### 命令行格式

```sh
texturepacker.exe -i <path> -o <path> -width <Number> -height <Number>
```

#### 示例命令

```sh
texturepacker.exe -i C:\input -o C:\output\atlas.png -width 2048 -height 2048
```

#### 参数说明

`-i` 输入待打包纹理图片或其所在文件夹。

`-o` 输出纹理图集。

`-width` 纹理图集最大宽度。

`-height` 纹理图集最大高度。

# 下载Release版本

[https://pan.baidu.com/s/16YB3yUm8jEC6Ep4q4O_PoQ?pwd=2o84](https://pan.baidu.com/s/16YB3yUm8jEC6Ep4q4O_PoQ?pwd=2o84)

# 如何编译

<div align="left">
  <img src="https://img.shields.io/badge/build-visual studio-blue.svg" />
  <img src="https://img.shields.io/badge/build-cmake-blue.svg" />
  <img src="https://img.shields.io/badge/build-vcpkg-blue.svg" />
  <img src="https://img.shields.io/badge/build-docker-blue.svg" />
</div>

1、编译需要fbxsdk和修改后的tinygltf等库，但是文件太大无法上传，因此放在了百度网盘中(链接：[https://pan.baidu.com/s/16YB3yUm8jEC6Ep4q4O_PoQ?pwd=2o84](https://pan.baidu.com/s/16YB3yUm8jEC6Ep4q4O_PoQ?pwd=2o84)
提取码：2o84 )，下载解压后放在和src同级目录下即可
2、编译时需要修改根目录下的CMakeLists.txt文件中CMAKE_TOOLCHAIN_FILE变量的值为本地vcpkg工具路径

其他方式：

1、windows环境下修改build.bat脚本的DCMAKE_TOOLCHAIN_FILE值为本地vcpkg工具路径，运行脚本即可编译；

2、linux环境下通过Dockerfile文件构建docker镜像即可。

备注：windows环境下运行vcpkg安装依赖包时，可能会遇到编译jasper库失败的问题，解决方案：[https://blog.csdn.net/weixin_41364246/article/details/140124085](https://blog.csdn.net/weixin_41364246/article/details/140124085)

详细教程：[开源跨平台三维模型轻量化软件osgGISPlugins-2、如何编译](https://blog.csdn.net/weixin_41364246/article/details/142723370)

# 缺陷

1、当前不支持i3dm、b3dm、gltf/glb文件的导入
...

# 后续计划

1、model23dtiles支持倾斜摄影数据转换
...

# 关于作者

这是作者的第一个开源项目，非常感谢[osg](https://github.com/openscenegraph/OpenSceneGraph)、[osgEarth](https://github.com/gwaldron/osgearth)、[osgVerse](https://github.com/xarray/osgverse)、[Fbx2glTF](https://github.com/facebookincubator/FBX2glTF)、[3dtiles](https://github.com/fanvanzh/3dtiles)等开源项目对我的启发和帮助。
联系方式：vx:jlcdhznextwhere     ps:添加时备注说明来意，不然有时候我不知道咋回事可能会拒绝添加，谢谢！
