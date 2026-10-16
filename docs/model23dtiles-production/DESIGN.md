# DESIGN.md — 生产级管线设计

对应手册第 5–12 节的设计要求。本文描述 `prod/model23dtiles-production` 分支上
新增的生产级转换管线（默认启用，`-legacy` 回退旧路径）的模块划分、数据流与关键算法决策。

## 1. 总体架构

```
输入(fbx/obj/osg...)
  │  osgDB 读入（A-23：双读修复；-up 变换烘焙进顶点）
  ▼
SourceModel（源几何 IR，§5）
  │  ①对象注册 + globalFeatureId  ②材质/属性规范化  ③世界变换烘焙
  ▼
SpatialPartitioner（自适应 BVH，§6）
  │  对象级 → primitive 级 → 三角形级 逐层切分
  ▼
ProxyMeshBuilder（HLOD 代理，§7）     GeometryErrorEvaluator（误差，§9）
  │                                     │
  ├─ 代理网格：子树并集简化             ├─ 双向 Hausdorff 表面距离
  ├─ feature 策略：聚合瓦片写           ├─ AABB BVH + patch 自适应细分
  │  aggregateSources 映射              └─ 输出 [lowerBound, upperBound] 区间
  ▼
TileFeatureMapper（§5.4/§8）
  │  globalFeatureId ↔ tile-local batchId 双向映射；feature-manifest.json
  ▼
ProductionPipeline（§10/§11/§12）
  ├─ refine：主树纯 REPLACE（根 ADD）
  ├─ b3dm / i3dm 实例瓦片
  ├─ boundingVolume.box：Z-up double 精度（A-10/A-25 修复）
  ├─ 写入 <outputDir>/.wb-staging 暂存目录（§14.17 原子化）
  └─ selfValidate 通过后 rename 到 outputDir
```

### 模块文件

| 模块 | 文件 | 职责 |
|---|---|---|
| 源几何 IR | `include/3dtiles/production/SourceModel.h/.cpp` | 对象/材质/三角形收集；世界变换烘焙 |
| 生产配置 | `include/3dtiles/production/ProductionProfile.h/.cpp` | 资源预算（硬/软上限）；`validate()` 超限抛异常；`ConversionReport` |
| 空间划分 | `include/3dtiles/production/SpatialPartitioner.h/.cpp` | 二叉 BVH；SAH 风格代价；`verifyCoverage()` |
| 代理生成 | `include/3dtiles/production/ProxyMeshBuilder.h/.cpp` | 子树并集物化 + meshopt 简化 |
| 误差评估 | `include/3dtiles/production/GeometryErrorEvaluator.h/.cpp` | 双向 Hausdorff；三角形 soup BVH |
| feature 映射 | `include/3dtiles/production/TileFeatureMapper.h/.cpp` | manifest 构建/校验/回查 |
| 主流程 | `include/3dtiles/production/ProductionPipeline.h/.cpp` | 编排 + tileset 写出 + 原子化 finalize |
| 追踪 | `include/3dtiles/production/Trace.h` | `WB_TRACE=1` 时逐行 stderr 日志（本环境退出码不可信，靠它定位） |

## 2. 关键设计决策

### 2.1 权威单元表（`_prims`）

三角形级切分会**新增**单元（把一个大 primitive 拆成多个三角形组），
这些新单元不在 `SourceModel::prims` 里。为此 `ProductionPipeline` 持有
`std::vector<PrimRef> _prims`（初值 = `model.prims` 的副本），分区器、代理构建、
物化、误差评估一律以这张表解析 item id。这消除了"分区器私有副本 id 与全局 id
错位"一类的悬垂索引崩溃（实测 t03 随机 ACCESS_VIOLATION 的根因）。

同理，分区器与 BVH 构建内部禁止持有 `vector` 元素的跨 `push_back` 引用
（`nodes[self]` 直取代替局部引用），这是 t04 崩溃的根因。

### 2.2 空间切分（§6.3 固定算法）

1. 候选轴 = 包围盒最长轴；
2. 候选切点 = 中位数 / 中点 / 均匀采样分位数；
3. 代价 = `max(左三角数/左预算, 右三角数/右预算) × (1 + 深度惩罚)`；
4. 三角形数/图元数均超预算且不可再切 → 三角形级切分（§6.6：默认**不裁剪**三角形
   到瓦片边界，只在对象/图元边界分组，保证源拓扑不被劈开——手册 8.3）；
5. `verifyCoverage()` 语义：父节点 items 必须等于其**直接子节点**声明集合的并
   （不是整棵子树叶子集合——那是实测 t12 堆损坏的第二个根因）；
   子节点建完用 `refreshItemsFromChildren()` 回填父 items/bounds/负载。

### 2.3 几何误差（§9.2/9.3）

- 定义：源面片集合 S 与代理面片集合 P 的**双向 Hausdorff 表面距离**，
  单位米；
- 计算：对每张源三角形，在代理 BVH 上求最近距离 d(S→P)；反之亦然；
  取 max。三角形按面积自适应采样，**未被采样的面片记保守全局上界**
  （源/代理包围盒并集对角线），保证上界恒成立、不会低估；
- 输出区间 `[errorLowerM, errorUpperM]`：下界 = 已采样的最大真实距离，
  上界含未采样补偿。写进 tileset 的 `geometricError` 用上界（宁可保守），
  `errorStatus: certified | boundLoose`；
- SSE 不参与几何误差测量（§9.7）；`geometricError` 由树自底向上
  `max(子误差, 自身测量误差)` 聚合（§9.6），根→叶单调不增由
  `selfValidate()` 硬校验。

### 2.4 refine 语义（§10）

- 主 HLOD 树统一 `REPLACE`；根节点 `ADD`（3D Tiles 语义要求根可加性）；
- 不再出现旧管线的"叶层 ADD"（A-03：父=子深拷贝 + ADD 造成重复渲染）；
- 代理是**子树并集的简化**，不是子节点深拷贝（A-06/A-20 修复）。

### 2.5 属性与身份（§5）

- 每个源对象注册获得稳定 `globalFeatureId`（0 起，转换期间不变）；
- 瓦片内 `_BATCHID` 是 tile-local 临时编号（0..batchLength-1），
  `feature-manifest.json` 维护 `globalFeatureId → [(tileUri, localBatchId)]` 映射，
  Cesium 端 pick 后经 manifest 回查全局身份；
- 源顶点属性 0 被占用时搬到空闲槽并写出 `_ATTRIBUTE_<n>`（A-11），
  同一几何被二次处理时凭 FloatArray 类型判据识别已写的 _BATCHID，不再搬动。

### 2.6 原子化输出（§14.17）

瓦片/manifest/report 全部落 `<outputDir>/.wb-staging`；
`finalize()` 读回 tileset.json 自校验：

1. 所有 content URI 在暂存目录真实存在；
2. 每个内容文件字节数 ≤ 硬上限（`-mb`，默认 8 MiB）；
3. 每个瓦片 triangles ≤ 硬上限；
4. `geometricError` 根→叶单调不增。

任一失败 → `remove_all(staging)`，outputDir 无任何数据集残留
（连空纹理缓存目录也清掉）。通过 → 逐项 rename（跨卷退回复制）。

## 3. 修复映射（AUDIT → 实现）

| AUDIT 项 | 修复位置 |
|---|---|
| A-03/A-06/A-20 重复/深拷贝代理 | ProxyMeshBuilder 子树并集；REPLACE 主树 |
| A-10/A-25 box 负半轴/重复 push | `yUpToZUpBounds`/`boxJson` 全 double |
| A-11 属性 0 覆盖 | BatchIdVisitor 搬槽 + `_ATTRIBUTE_<n>` + FloatArray 判据 |
| A-12 简化后 _BATCHID 失配 | Simplifier.h 数组重映射 |
| A-14 非 TBB drawcallCount | NodeProperty 补字段 |
| A-15 硬上限被放宽 | ProductionProfile::validate 抛异常；CLI 值直通 |
| A-16 maxLevel 吞对象 | 分区器终止条件显式失败/继续切 |
| A-17 无 feature 概念 | globalFeatureId + TileFeatureMapper + manifest |
| A-18 无误差测量 | GeometryErrorEvaluator（见 §2.3） |
| A-19 无 seam registry | 跨瓦片对象默认不裁剪（§8.3），对象归属单 tile |
| A-22 测试只在 Debug | `OSGGISPLUGINS_BUILD_TESTS` option |
| A-23 CLI `-r` 失效 | 双读 `-r`/`-sr` |
| A-24 相对路径丢失 | main.cpp 转绝对路径 |

## 4. 构建与复现

```bash
bash scripts/wb_build.sh            # 全量
bash scripts/wb_build.sh model23dtiles
bash scripts/wb_build.sh test_production
```

环境雷区（详见 BASELINE.md §6）：vcpkg manifest 安装关闭、大小写重复的
http_proxy 变量、`--parallel` 路径转换、测试目标链接 tbb12 绝对路径。
