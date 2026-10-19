# AUDIT.md — 代码审计结果

基线提交：`b229500`（分支 `dev`）
审计范围：`src/3dtiles/**`、`src/utils/**`、`src/osgdb_gltf/b3dm/**`、`tools/model23dtiles/**`
标记含义：`CONFIRMED`=代码直接证明 / `TEST_REQUIRED`=需复现测试判定 / `NOT_REPRODUCED`=已尝试未复现

---

## 汇总表

| ID | 主题 | 文件:函数:行 | 状态 | 手册条款 | 严重度 |
|---|---|---|---|---|---|
| A-01 | 几何误差使用体积加权平均 + 固定 epsilon | `src/3dtiles/Tile.cpp:110-148` | CONFIRMED | 9.1/9.6/12.3 | 高 |
| A-02 | 固定三层 LOD + 固定简化比例 | `src/3dtiles/Tile.cpp:594-648` | CONFIRMED | 7.2/9.1 | 高 |
| A-03 | 按 LOD 编号设置 `refine=ADD`；父内容是子节点深拷贝 | `src/3dtiles/Tile.cpp:430,348-375` | CONFIRMED | 10.1/10.2 | 高 |
| A-04 | `Tileset::computeGeometricError()` 固定相机参数 + `×1.5` | `src/3dtiles/Tileset.cpp:99-159` | CONFIRMED | 9.6/9.7 | 高 |
| A-05 | `valid()` 只查严格不等式，可被 epsilon 满足 | `src/3dtiles/Tile.cpp:190-212`、`Tileset.cpp:20-24` | CONFIRMED | 12.3/0.1 | 高 |
| A-06 | `getAllDescendantNodes()` 每节点重建整棵后代 Group | `src/3dtiles/Tile.cpp:168-188` | CONFIRMED | 13.1 | 中 |
| A-07 | `descendantNodeIsEmpty()` 未判空即 `asGroup()` | `src/3dtiles/Tile.cpp:156` | CONFIRMED | 13.1 | 中 |
| A-08 | `processOverSizedNodes()` 固定 2×2×2 + 包围球中心分派 | `src/3dtiles/hlod/TreeBuilder.cpp:450-548` | CONFIRMED | 6.2/6.5 | 高 |
| A-09 | `regroupI3DMTile()` 树范围外实例 TODO | `src/3dtiles/hlod/TreeBuilder.cpp:381` | CONFIRMED | 6.5 | 中 |
| A-10 | `BoundingVolume::computeBox()` 产出负半轴 | `src/3dtiles/BoundingVolume.cpp:33-58` | CONFIRMED（已实测） | 12.1 | 高 |
| A-11 | `BatchIdVisitor` 覆盖通用顶点属性 0 | `src/osgdb_gltf/b3dm/BatchIdVisitor.cpp:14-18` | CONFIRMED | 5.5 | 高 |
| A-12 | `Simplifier` 简化后 `_BATCHID` 数组长度与顶点数失配 | `include/utils/Simplifier.h:86-123` | CONFIRMED | 5.5 | 高 |
| A-13 | TBB 搜索哨兵用错容器尺寸 | `src/3dtiles/hlod/TreeBuilder.cpp:218-240` | CONFIRMED | 13.2 | 高 |
| A-14 | 非 TBB 分支给不存在的结构体成员赋值（无法编译） | `TreeBuilder.cpp:695` / `TreeBuilder.h:71-75` | CONFIRMED | 13.2 | 高 |
| A-15 | 用户硬上限被自动放宽 | `src/3dtiles/hlod/TreeBuilder.cpp:705` | CONFIRMED | 3 | 高 |
| A-16 | 达到 maxLevel 时无条件吞掉剩余对象 | `src/3dtiles/hlod/TreeBuilder.cpp:554-577` | CONFIRMED | 6.5 | 高 |
| A-17 | 无任何 feature/身份概念、无 manifest | 全仓 | CONFIRMED | 5.1/5.4 | 高 |
| A-18 | 无几何误差测量（只转 `lodError`） | `src/3dtiles/Tile.cpp:640` | CONFIRMED | 9 | 高 |
| A-19 | 无 seam registry；三角形级切分不存在 | 全仓 | CONFIRMED | 8 | 高 |
| A-20 | `writeNode()` 每级 3 次 `DEEP_COPY_ALL` | `src/3dtiles/Tile.cpp:357-369` | CONFIRMED | 13.1 | 中 |
| A-21 | 进程退出码恒为 0xC0000409 | `tools/model23dtiles/main.cpp` | CONFIRMED（已实测） | 0.2 | 中 |
| A-22 | 单元测试只在 Debug 构建 | `CMakeLists.txt:164` | CONFIRMED | 14 | 中 |
| A-23 | CLI 文档写 `-r`，代码读 `-sr` | `tools/model23dtiles/main.cpp:314 / 414` | CONFIRMED | — | 低 |
| A-24 | 相对输出路径被 CWD 切换吞掉 | `tools/model23dtiles/main.cpp:462` | CONFIRMED（已实测） | — | 中 |
| A-25 | `BoundingVolume::box` 重复 push_back 未清空 | `src/3dtiles/BoundingVolume.cpp:43` | CONFIRMED | 12.1 | 中 |
| A-26 | `Simplifier` 遇到多 primitiveSet 直接 return；空代理路径清空顶点数组 | `include/utils/Simplifier.h`、`src/utils/Simplifier.cpp` | FIXED（单测复现） | 7.2/7.3 | 高 |
| A-27 | 聚合代理「每 cell 只留第一个 feature」整簇丢失；`meshopt_simplifySloppy` 写穿目的缓冲 | `src/3dtiles/production/ProxyMeshBuilder.cpp` | FIXED（单测复现） | 7.5/9.5 | 高 |
| A-28 | `selfValidate` 无源身份覆盖不变量；叶写出失败只记 warning；无端到端管线用例 | `ProductionPipeline.cpp:1222` | FIXED | 9.5/17 | 高 |
| A-29 | 聚合代理 `_BATCHID` 身份退化为位置索引，冒充真实源 feature | `ProductionPipeline.cpp:446-455` | FIXED | 8.1/5.4 | 高 |
| A-30 | 代理预算把纹理字节当硬约束，纹理密集模型的聚合 fallback 永不收敛 | `ProxyMeshBuilder.cpp::build` | FIXED（真实 FBX 复现） | 7.3/10.2 | 高 |
| A-31 | 远景代理建不出达标几何时判整批失败，大模型永远无法发布 | `ProductionPipeline.cpp:692` | FIXED | 7.3/14.17 | 高 |
| A-32 | 手册 §3 的"树深 ≤ 48"只是报告值，发布闸不检查 | `ProductionPipeline.cpp::selfValidate` | FIXED | 3/17 | 中 |
| A-33 | i3dm 实例瓦片不做三角形预算：块大小只按实例个数切，超限瓦片静默入库 | `ProductionPipeline.cpp::buildInstanceTree:903-1015` | FIXED（真实 FBX 复现） | 3/7.3/17 | 高 |
| A-34 | `manifest.aggregateProxies` 恒为空：代理 ID → 源 feature 集合无可查通路，登记接口是死代码 | `ProductionPipeline.cpp::writeManifest:1277-1300` | FIXED | 8.1/17 | 高 |
| A-35 | `report.manifestOccurrences` 与 `manifest.occurrenceCount` 两套定义，同一数据集两个数 | `TileFeatureMapper.cpp::sourceOccurrenceCount:45` | FIXED | 8.1/17 | 中 |
| A-36 | `boxJson` 把 NaN 半轴折成 0、未初始化包围盒折成全零盒：坏 bounds 被"洗白"成 schema 合法的 tileset，发布闸不看 bounds | `ProductionPipeline.cpp::boxJson:180-199` | CLOSED（发布闸 + `bounds` 单测） | 12.1/17 | 高 |
| A-37 | "父包围盒覆盖子包围盒"只在历史 bug 注释里提过，代码里没有任何检查点（当年靠外部回扫发现） | `ProductionPipeline.cpp::selfValidate` | CLOSED（发布闸 + 逐轴越界单测 + 18 数据集回扫） | 12.1/17 | 中 |
| A-38 | 产物里偶发单个荒谬顶点（矩阵 t02：X=6.555e10 / Z=4.474e27），源模型里没有；有限性与父覆盖子都拦不住（父盒同样被污染）。本轮逐元素差分定位到**缺陷形态**：一份共享源顶点数组里 1 个 POSITION 槽 + 5 个 NORMAL 槽装着"别的数据"，凡拷贝到该区间的瓦片都带上；值本身随分配器状态变化，所以看起来偶发。37 次产物里只出现 1 次，且那一次的 t1/t2/t3 与 A-40 的"树变体"**逐字节相同** → 污染那次就是这条非确定性分支上的一个产物 | `ProductionPipeline.cpp::selfValidate` 2b | OPEN（写入方未定位）+ CLOSED（发布闸拒绝出盘） | 12.1/17 | 高 |
| A-39 | `ProxyMeshBuilder::appendElements` 用**源**数组类型 `static_cast` **目标**数组，属性类型不一致时按错误步长写（堆越界） | `ProxyMeshBuilder.cpp::appendElements` | CLOSED（显式拒绝 dst/src 类型不一致；实测现存分组不会触发，属加固） | 5.3 | 中 |
| A-40 | 同一二进制、同一输入，产物**不逐位一致**：t02 共 37 次产物里 35 次逐字节相同，1 次（`WB_PROXY_DEBUG=1` 第 3 跑）整棵瓦片树变了（`nodesBoundLoose` 1→3、峰值 RSS 34.4→47.3 MiB），1 次就是 A-38 的污染产物。而且**同一份 b3dm 内容会换瓦片槽位**（基线 t5 == 变体 t3，sha256 相同）——变的不是数值，是分配次序 | `ProductionPipeline::buildB3dmTree` 并行窗口 / 误差评估分支 | OPEN（非确定性来源未定位；`WB_JOBS=1` 6 次全部与并行基线逐字节相同，所以不是并行度本身） | 17 | 高 |

---

## 逐项说明

### A-01 几何误差 = 体积加权平均 + 固定 epsilon　**CONFIRMED**
`src/3dtiles/Tile.cpp:110-148`

```cpp
totalWeightedError += childGeometricError * child->volume;   // 行 130
totalWeight += child->volume;
...
this->geometricError = totalWeightedError / totalWeight;     // 行 136  ← 体积加权平均
...
if (maxChildError >= this->geometricError)
    this->geometricError = maxChildError + MIN_GEOMETRIC_ERROR_DIFFERENCE;  // 行 147 ← epsilon
```
`MIN_GEOMETRIC_ERROR_DIFFERENCE = 0.1`（`Tile.h:33`），与模型尺寸/代理质量无关。
`childGeometricError` 来自 `getCesiumGeometricErrorByPixelSize(pixelSize, radius)`
——由「像素尺寸」和固定相机参数反推，不是几何误差（违反 9.1）。

**实测**：`t01` root ge=130.3726311087895，child ge=130.2726311087895，差值恰好 0.1。

**决定**：删除体积平均与 epsilon。改为按 9.6 的
`G(n) = max(measuredProxyErrorUpperM(n), max(G(child)))`，误差来自第 5 阶段双向表面距离评估器。

---

### A-02 固定三层 LOD + 固定简化比例　**CONFIRMED**
`src/3dtiles/Tile.cpp:594-648`

```cpp
case 2: applyLODStrategy(0.5f, 0.25f); break;
case 1: applyLODStrategy(1.0f, 0.5f);  break;
default: applyLODStrategy(1.0f, 1.0f); break;
```
`targetError` 由魔数 5.0 / 0.6 / 0.01 除以对角长度得到，再 clamp 到 0.3 / 0.075。
`buildLOD()`（Tile.cpp:403-488）恒定创建 `tileLOD2/tileLOD1/tileLOD0` 三层。

**决定**：改为预算驱动的自适应代理（第 3 阶段），层数由几何预算与误差门槛决定。

---

### A-03 按 LOD 编号设置 `refine=ADD`；父内容是子节点深拷贝　**CONFIRMED**
`src/3dtiles/Tile.cpp:430`
```cpp
tileLOD0->refine = Refinement::ADD;
```
`writeNode()`（行 348-375）：
```cpp
case 2: this->node = osg::clone(this->children[0]->node.get(), DEEP_COPY_ALL); ...
case 1: this->node = osg::clone(this->children[0]->node.get(), DEEP_COPY_ALL); ...
```
父内容 = **第一个子节点**的克隆，不是子树并集的代理；再叠加 `ADD` → 同一表面被画两遍。

**决定**：第 6 阶段 `RefinementPolicy` 按父子内容语义推导；
完整模型默认全部 `REPLACE`；父代理由 `ProxyMeshBuilder` 从子树完整源几何生成。

---

### A-04 `Tileset::computeGeometricError()` 固定相机参数 + `×1.5`　**CONFIRMED**
`src/3dtiles/Tileset.cpp:158`
```cpp
this->geometricError = osg::maximum(this->geometricError, this->root->geometricError * 1.5);
```
手册 9.6 明确点名禁止「任意 `*1.5` 使它们通过 valid()」。

**实测**：`t03` tileset.ge = 781.2612094123297 = root 520.840806 × 1.5。

---

### A-05 `valid()` 只查严格不等式　**CONFIRMED**
`Tile.cpp:190-212`
```cpp
if (this->geometricError <= this->children[i]->geometricError) return false;
...
else { return this->geometricError == 0.0; }   // 叶必须严格等于 0
```
`Tileset.cpp:23`: `return this->root->valid() && this->geometricError > this->root->geometricError;`

只要把差值凑成 0.1，任何真实误差都能通过；叶必须恰好 0 也是硬编码假设。
**决定**：改成检查真实不变量（有限性、非负、单调不增、bounds 覆盖、内容存在、batch 一致、预算）。

---

### A-06 `getAllDescendantNodes()` 每节点重建整棵后代 Group　**CONFIRMED**
`Tile.cpp:168-188` 递归构造 Group 并逐个 addChild；`computeBoundingVolumeBox()`
（行 78-82）对每个 tile 调一次 → 整树 O(n²) 且反复改 OSG 父子关系。
手册 13.1 明确禁止。**决定**：改为后序缓存聚合（`TileBoundsAggregator`）。

---

### A-07 `descendantNodeIsEmpty()` 未判空即 `asGroup()`　**CONFIRMED**
`Tile.cpp:156`: `this->children[i]->node->asGroup()->getNumChildren()`
若 `node` 是 `Geode` 或为空，直接空指针解引用。

---

### A-08 `processOverSizedNodes()` 固定 2×2×2 + 包围球中心分派　**CONFIRMED**
`TreeBuilder.cpp:450-548`
- `gridSize = 2` 硬编码，永远 8 等分，不按负载选轴、不按 SAH 选平面；
- 用**包围球中心**而非几何 AABB 决定归属，跨格几何会被整块错分；
- `(center.x()-bb._min.x()) / (bb._max.x()-bb._min.x())`：退化轴 → 除零 →
  `inf/NaN` 转 int 是未定义行为，再被 clamp 掩盖；
- 若超大节点只有 1 个 child（行 472 `getNumChildren() > 1`），**不进入拆分列表**，
  直接原样输出 → 静默产出超限瓦片；
- 无「每次递归必须降低最大负载」的保证。

**决定**：整节替换为自适应二叉 BVH（第 2 阶段）。

---

### A-09 `regroupI3DMTile()` 树范围外实例 TODO　**CONFIRMED**
`TreeBuilder.cpp:381` 注释 `//TODO:If i3dm tiles are outside the b3dm tree range`。
当 `b3dmTile->node` 无效时直接递归子节点；若某条分支没有匹配的 b3dm 子节点，
i3dm 瓦片不会挂到任何空间节点上。`build()`（行 56-60）会把剩余 i3dm 挂到 rootTile
作为 b3dm 树的**兄弟**，即游离于空间树之外。

**状态**：CONFIRMED（代码）+ TEST_REQUIRED（需要 t05 实例数据判定是否真的丢实例）。
基线中 t05 单独跑通（产出 1 个 i3dm），但矩阵化运行时 `.osgt` 插件缺失未覆盖。

---

### A-10 `BoundingVolume::computeBox()` 产出负半轴　**CONFIRMED（已实测）**
`BoundingVolume.cpp:39-57`
```cpp
const osg::Matrixd mat = osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f);
const osg::Vec3f size = boundingBox._max * mat - boundingBox._min * mat;
...
this->box.push_back(size.x()/2); ... this->box.push_back(size.y()/2); ...
```
绕 X 轴 +90° 把 (dx,dy,dz) 映射为 (dx,−dz,dy)。只要 dz>0，`size.y()<0`，
于是 box 的第 7 项（Y 半轴）恒为负。3D Tiles 要求 box 半轴为正。

**实测**：12/12 个资产的每一级都报 `box half-extent[7] is negative`。

**决定**：重写为先变换 8 个角点再取 min/max，半轴取绝对值并保证 ≥0。

---

### A-11 `BatchIdVisitor` 覆盖通用顶点属性 0　**CONFIRMED**
`BatchIdVisitor.cpp:14-18`
```cpp
const osg::Array* vertexAttrib = geometry->getVertexAttribArray(0);
if (vertexAttrib) { OSG_WARN << "...it will be overwritten!" ...; }
geometry->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);
```
源属性 0 被无声覆盖，只留一条 WARNING。**决定**：按 5.5 的 6 条要求重写。

---

### A-12 `Simplifier` 简化后 `_BATCHID` 数组长度与顶点数失配　**CONFIRMED**
`include/utils/Simplifier.h:86-123`：重建了 `positions / normals / texcoords` 并重映射索引，
但**完全没有处理 `getVertexAttribArray(0)`**。结果：
- `_BATCHID` 数组仍是**旧的**顶点数量；
- 索引已被 remap 到新 positions。

→ 顶点属性数组长度 ≠ 顶点数组长度，glTF 写出时 accessor 越界或错位，
batch id 与几何错位。**决定**：`Simplifier` 必须同步重映射所有绑定逐顶点的
vertex attrib 数组，并加单元测试（手册矩阵第 8 项）。

---

### A-13 TBB 搜索哨兵用错容器尺寸　**CONFIRMED**
`TreeBuilder.cpp:218-240`（`generateB3DMTile`）与 `TreeBuilder.cpp:109-125`（`apply(Geode&)`）
```cpp
std::atomic<size_t> foundIndex(_geodes.size());       // 用 _geodes 的尺寸
tbb::parallel_for(tbb::blocked_range<size_t>(0, uniqueStateSets.size()), ...)
   foundIndex.store(j, ...);
index = (foundIndex.load() == _geodes.size()) ? -1 : foundIndex.load();
```
搜索的是 `uniqueStateSets`，哨兵却取 `_geodes.size()`。两者尺寸独立：
- 若命中下标恰好 == `_geodes.size()` → 被误判为「未找到」→ 重复创建分组；
- 若 `uniqueStateSets.size() > _geodes.size()` → 大下标命中被丢。

并行结果因此与串行不一致（违反 13.2 的确定性要求）。

---

### A-14 非 TBB 分支给不存在的结构体成员赋值　**CONFIRMED**
`TreeBuilder.cpp:695`
```cpp
p.drawcallCount = dv.getCount();
```
但 `TreeBuilder::NodeProperty`（`TreeBuilder.h:71-75`）只有 `node / bb / triangleCount`。
`model23dtiles` 定义了 `USE_TBB_PARALLEL=1`，所以非 TBB 分支从未被编译过。
→ **手册 13.2「TBB enabled/disabled 均需构建」目前不可能满足**。
**决定**：补 `drawcallCount` 成员，并增加一个 non-TBB 构建目标做回归。

---

### A-15 用户硬上限被自动放宽　**CONFIRMED**
`TreeBuilder.cpp:705`
```cpp
_config.setMaxTriagnleCount(osg::maximum(maxTriangles, _config.getMaxTriangleCount()));
```
把「用户三角形硬上限」抬到至少等于最大单个对象的三角形数。违反手册第 3 节
「硬上限不得在转换中自动放宽」。**决定**：删除；超限必须继续切分或明确失败。

---

### A-16 达到 maxLevel 时无条件吞掉剩余对象　**CONFIRMED**
`TreeBuilder.cpp:554-577`：`if (tile->level >= _config.getMaxLevel())` 把所有
剩余相交对象一次性塞进同一个 tile，不再检查三角形预算。
`ABSOLUTE_MAX_LEVEL = 15`（`TreeBuilder.h:47`），远小于手册要求的 48。

---

### A-17 无任何 feature/身份概念、无 manifest　**CONFIRMED**
全仓搜索无 `globalFeatureId / feature-manifest / sourceNodeId`。
`_BATCHID` 由 `BatchIdVisitor` 在**写出时**按 Geode 遍历顺序临时编号
（行 28-37，`_currentBatchId++`），跨 tile、跨 LOD 都不稳定。
违反手册 5.1 / 5.4 / 17 节。

---

### A-18 无几何误差测量　**CONFIRMED**
`Tile.cpp:640` `this->lodError = simplifier.lodError * this->diagonalLength;`
`simplifier.lodError` 是 meshopt 的 `result_error`（相对单位，且只覆盖简化操作本身），
乘对角长度后当成米制误差。无双向 Hausdorff、无采样、无不确定区间。

---

### A-19 无 seam registry；三角形级切分不存在　**CONFIRMED**
`TreeBuilder` 只在**对象级**切分（`processB3DMWithMeshDrawcallCommandLimit` 按
`NodeProperty` 整体搬运）。单个超 mesh 无法切分（基线 t03 实测：134162 三角形进 1 个瓦片）。
无跨瓦片边界顶点记录、无锁边简化。

---

### A-20 `writeNode()` 每级 3 次 `DEEP_COPY_ALL`　**CONFIRMED**
`Tile.cpp:357/365/369`：LOD2/1/0 各克隆一次整棵子树 → 内存 ×3。
基线 t03 峰值 81.6 MiB，输出 8.8 MB（同一份几何 3 份）。

---

### A-21 进程退出码恒为 0xC0000409　**CONFIRMED（已实测）**
12/12 个资产均在 tileset.json 完整写出之后以 `0xC0000409`
（STATUS_STACK_BUFFER_OVERRUN）结束。影响：CI 无法用退出码判定成功/失败。
**待定位**（疑似 TBB `global_control` / OSG 静态析构顺序）。列入待修清单。

---

### A-22 单元测试只在 Debug 构建　**CONFIRMED**
`CMakeLists.txt:164`
```cmake
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()
```
生产 Release 构建完全不编译 `test_3dtiles` → 无法做发布前回归。
**决定**：加 `OSGGISPLUGINS_BUILD_TESTS` 选项，Release 也能编测试。

---

### A-23 CLI 文档写 `-r`，代码读 `-sr`　**CONFIRMED**
`main.cpp:314` 打印 `-r <number>`；`main.cpp:414` 实际是
`parseArgument(arguments, "-sr", 0.5)`。用户按帮助传 `-r` 会被忽略。

---

### A-24 相对输出路径被 CWD 切换吞掉　**CONFIRMED（已实测）**
`main.cpp:462` `osgDB::setCurrentWorkingDirectory(inputPath)`；
随后 `Tileset::write()` 用相对 `config.tileConfig.path` 拼路径 → 产物写到输入目录。
基线第一次运行因此 0 输出。**决定**：main 里把 `-i/-o` 都转成绝对路径。

---

### A-25 `BoundingVolume::box` 重复 push_back 未清空　**CONFIRMED**
`BoundingVolume.cpp:43` 起连续 12 次 `push_back`，但 `computeBox()` 从不 `clear()`。
同一 `BoundingVolume` 对象被复用一次就会变成 24 元素非法数组。

---

### A-26 `Simplifier` 多 primitiveSet 直接放弃简化 + 空代理清空顶点　**FIXED**
`src/utils/Simplifier.cpp`（旧实现整段在头文件里）

旧 `simplifyMesh()` 在 `geom.getNumPrimitiveSets() != 1` 时 `return`。合并后的
几何（`ProxyMeshBuilder` 按属性签名把同材质几何并成一个 drawable）天然携带多个
primitiveSet，于是**远景代理完全不简化**，硬预算永远达不到；同时旧
`simplifyPrimitiveSet()` 在 `destination->size()==0` 时把 vertex array、法线、
UV 全部 `setVertexArray(nullptr)`，产出零三角形代理瓦片。

**复现**：`test_production simplifier`（多 pset 几何必须真的降面且属性/索引不
错位）。
**修复**：逐个 pset 简化后按所有 pset 的顶点并集做一次统一重映射；支持
`DrawElementsUInt/UShort/UByte` 与 `DrawArrays`；目标小于一个三角形时跳过该
pset，绝不清空几何（`dst.size()<3` 直接退回原索引）。
**验证**：单测 71/71、矩阵 12/12。

---

### A-27 聚合代理整簇丢失 + `meshopt_simplifySloppy` 写穿缓冲　**FIXED**
`src/3dtiles/production/ProxyMeshBuilder.cpp::buildAggregate`

旧聚合 fallback 在每个 occupied cell 里只物化「第一个」源 feature。当 cell 尺寸
大于 feature 间距时，一整簇对象从远景 LOD 上消失；实测 12 个占用 cell 只剩 6 个
有几何，双向 Hausdorff 上界 497 m（约 4 个 cell 的整块空洞），而误差与预算检查
全部通过——**这类缺陷在平均误差上是隐形的**（手册 9.5 明确点出）。
另一侧：`meshopt_simplifySloppy` 在重复顶点/非流形输入上可能返回**多于输入**的索引
数（实测往 704 元素的目的地写了 1272 个索引 → 堆溢出，是当时随机段错误之一）。

**复现**：`test_production proxyAggregate`（断言每个占用 cell 都有代理三角形 +
误差必须在 cell 尺度内）。
**修复**：聚合改为「按 cell 抽稀 feature」——Morton 序（`cellSize/8` 量子）跨轴
交错取样，三角形按 cell 面数占比分配配额；目的缓冲按 `src.size()*2+16` 预留并夹断；
sloppy 被拒绝时同步截断 `dst`（旧代码会残留整段零索引）。
**实测**：12/12 cell 全覆盖，误差 lower=130 m / cellDiag=176 m，代理 92 三角形
（目标 120）。

---

### A-28 `selfValidate` 缺少源身份覆盖不变量　**FIXED**
`src/3dtiles/production/ProductionPipeline.cpp::selfValidate`

自校验只查 URI 存在、字节/三角形硬限、`geometricError` 单调，**没有任何一条保证
「每个 source feature 都还存在于某个叶瓦片」**（手册 9.5/17）。叠加两个洞：
叶瓦片写出失败只记 warning（`hasContent=false` 但流程继续），以及此前**没有任何
用例跑过完整管线**——所以 A-27 只有在真实 FBX 上才可能被发现。

**修复**（手册 14.17：宁可不出数据集）：
1. 叶瓦片必须带内容且三角形 > 0，否则按「丢失 N 个源 feature」判失败；叶写出失败
   从 warning 升级为 failure（代理写出失败仍可被子树覆盖，保持 warning）；
2. 覆盖核对使用分区单元表推出的 `sourceScope`（`globalFeatureId` 去重），源 feature
   全集未被叶集合覆盖即失败，计数写入 `report.featuresMissingFromLeaves`；
3. 聚合代理必须登记 `proxyFeatureId -> 源集合`、禁止出现在叶层、映射越界即失败；
   被抽稀掉的 feature 数写入 `report.proxyDroppedFeatureCount` 并记 warning；
4. `conversion-report.json` 改到自校验之后落盘，逐瓦片新增 `sourceFeatures` 字段。

**复现/验证**：新增首个端到端用例 `test_production pipelineCoverage`——合成 8×8
网格断言 64 个源 feature 全部落在叶瓦片；对照实验（临时抹掉一个覆盖记录）得到
`selfValidate: 1 source features are absent from every leaf tile` 且
`finalize()` 拒绝发布、输出目录里没有 `tileset.json`，证明该闸非空转。

---

### A-29 聚合代理的 batch 身份冒充真实源 feature　**FIXED**
`ProductionPipeline::writeContent`

`featureOrder` 由 Geode 名字回查 `_model.objects`；`proxy_cluster_<id>` 查不到就退化成
「按出现顺序的位置索引」。于是聚合代理的第 i 个 batch 顶着真实源 feature id `i`
进入 manifest——手册 8.1/5.4 明令禁止「把聚合几何当作其中某个源对象的真实几何」。

**修复**：`proxy_cluster_` 前缀直接解析出 `proxyFeatureId`（`0xF0000000+` 段），
与 `aggregateSources` 的键一致；只有真源对象才走名字回查。

---

### A-30 纹理字节被当成代理的硬预算，纹理密集模型聚合 fallback 永不收敛　**FIXED**
`src/3dtiles/production/ProxyMeshBuilder.cpp::build`

`estimateContentBytes()` 把「几何 + 内嵌 KTX2 纹理」合成一个数，然后拿它去卡
`maxTileContentBytes`（8 MiB 硬限）。对**叶瓦片**（必须带材质）正确；对**内节点
远景代理**错误——代理本来就可以建成无纹理版（`FarLodTextureStripper`，常规 LOD
做法），把纹理算成绑定约束等于要求「几何 + 全部纹理」也挤进 8 MiB。

实测 `longxiang.fbx`（123 张纹理）：node 20 估算 32 515 174 B，是纯几何的约 6 倍；
所有 sizeFactors × merge 组合都因此被拒，聚合循环走完 5×2 轮仍无一被接受，最后
报 `proxy still over budget after aggregate fallback` → 整个数据集不发布。
降采样（`estimateBytesSplit` 的纹理项按 /8 折算）也救不了：纹理项主导时，
削减三角形根本不改变它。

**修复**：预算判据拆成 `BytesSplit{geo, tex}`。
1. 绑定约束是 **geo 字节**（identity 早返回、身份保持代理接受、聚合接受三处同改）；
2. 聚合的 3 轮收敛按**实测 geo bytes/tri** 反推目标面数（静态 200 B/tri 仍偏乐观：
   t03 根代理 41 943 面 = geo 9.58 MB）；
3. `ProxyResult::needsTextureless` 成为契约：几何达标但 `geo+tex` 超限时置位，
   由 `processTileHeavy` 剥纹理并记 `far-LOD proxy built without textures to fit
   budget` warning，不再让调用方各自重复预估。

**验证**：`longxiang.fbx` 端到端 rc=0、0 error、129 瓦片、最大内容瓦片 2 226 920 B
（8 MiB 的 26.5 %）、`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0`，
93.4 s / 峰值 2 401 MiB；矩阵 12/12 rc=0、单测当时 71/71（A-31 用例落地后为 82/82，见 STATUS.md）（见 §1/§3 与 STATUS.md）。

---

### A-31 远景代理不达标就判整批失败，大模型永远出不了数据集　**FIXED**
`ProductionPipeline::finalizeTileFromWork`

A-30 之前为了让超限代理不再漏网（commit 37d832c），`processTileHeavy` 在
`pr.failure` 非空时直接置 `w.failure`，收尾函数把它原样推进 `_failures`
→ `finalize()` 丢弃 staging → 整个数据集不发布。对**叶**正确（源件真丢）；
对**内节点**错误：内节点代理只是远景 LOD 表示，它的子树仍然覆盖同一份
source scope，REPLACE 语义下「无 content、直接细化到子节点」是合法瓦片。
实测 `lugouqiao_v11`（335 MB）有 7 个内节点代理超过 120 000 面硬限，
按旧逻辑等于宣布这个模型永远无法生产。

**修复**：把失败按 `w.leaf` 分流。
- 叶 → 仍然 `_failures`（数据丢失，手册 14.17 优先于可用性）；
- 内节点 → `_warnings` + 新计数 `report.proxySkippedCount`，该节点保持
  `hasContent=false`，`geometricError` 由 `max(measuredProxyError, maxChild)`
  自然回落到子树值（`ProductionPipeline.cpp:1024`），根到叶单调性校验照过。

LOD 空洞因此变成**可观测指标**而不是静默降级：`proxySkippedCount` 与
`featuresMissingFromLeaves` / `proxyDroppedFeatureCount` 一起进报告，
前者=远景退化，后两者=真丢件，绝不混为一谈。

同一个洞在**写盘后**还有一份：`finalizeTileFromWork` 用实测字节判超限
（旧代码只看 `maxTileContentBytes`，连 120 000 面硬限都没查），超限就
`_failures.push_back` → 整批报废。现在按叶/内节点分流，且内节点超限时
**把刚写进 `.wb-staging` 的超限文件删掉**（`std::filesystem::remove`），
产物里绝不残留一个"没人引用但确实超限"的瓦片；叶超限仍然判失败，
因为删叶 = 丢源 feature。`selfValidate` 的聚合映射检查也同步跳过
`hasContent=false` 的降级代理，避免把"该层无内容"误记成"抽稀丢失"。
降级节点的身份/覆盖范围改为在函数入口就落账（原先在末尾，早退路径拿不到），
这样报告与自检看到的 `sourceScope` 不是"只有成功瓦片才有范围"。

**验证**：`test_production proxySkip`（合成 8×8 网格、64 对象 512 面，
`strictIdentity + lockSeamBorder`，硬限 150 面、代理目标 100 面）实测：
根节点代理覆盖全部 512 面、压不到 100 面目标，`strictIdentity` 下即判失败
→ 走内节点降级分支，
warning 原文 `proxy generation failed for node 0: strictIdentity: proxy
exceeds triangle budget, failing -> tile carries no far-LOD content,
children still cover 64 source features`，`proxySkippedCount=1`、
15 个瓦片中 14 个有内容、`maxTrianglesInTile=96`、
`featuresMissingFromLeaves=0`、`tileset.json` 正常发布且**不引用** `t0.b3dm`，
`tiles/` 目录里也确实没有 `t0.b3dm`。同一用例的反向对照把
`maxTreeDepth` 压到 1（512 面在该深度装不下）→ `finalize()=false`、
输出目录里没有 `tileset.json`，证明失败关闭没被降级逻辑顺手绕过。
写盘后的那份判据是防御网：本用例里构建期先失败，所以
`over-budget far-LOD file removed` 文案没被触发——它兜的是"估算达标但
实测超限"（估算器本身只是估算，见 VALIDATION_REPORT §1 注），
不能为了凑文案去伪造一个超限代理。单测合计 **82/82 通过**。

---

### A-32 树深硬限（48）不在发布闸内　**FIXED**
`ProductionPipeline::selfValidate`

手册 §3 把「深度 ≤ 48」列为不可自动放宽的硬限。已有两处相关代码都**不核对最终
产物**：`ProductionProfile.cpp:78` 只保证 profile 里声明的值在 [1,48]
（防止有人把上限调大），`SpatialPartitioner.cpp:397` 只让分区递归在该深度停止，
`report.maxDepth` 则纯粹是事后统计值。也就是说**发布闸从不检查写出来的
tileset 到底有多深**——一旦某条路径（实例子树、b3dm 子树、结构预分配）绕过了
分区器的深度判断，超深树照样能出盘。

**修复**：`selfValidate` 新增 3b，用 `_tiles[].children` 显式栈走一遍
（不用递归，避免超深树自身栈溢出），`deepest > maxTreeDepth` 即记
`selfValidate: tree depth N exceeds hard limit 48 (deepest tile M)` 并拒绝发布。
字节/三角形两条硬限在同一个函数里本来就有（第 1/2 项），树深补齐后
手册 §3 四条限制里工具侧可判的三条都在闸内；
「每瓦片 ≤ 24 个 glTF primitive」目前只有分区侧 `Load::prim`（源图元去重计数）
参与切分决策，写出后的 glTF primitive 数没有回读核对——该缺口记 STATUS.md B-9，
需要读回 b3dm 头才能闭合，留待外部 validator（B-3）一起做。

**验证（诚实口径）**：3b 是**负向防护网**，没有正向触发用例——要让它报错，
需要一棵实际深度 > `maxTreeDepth` 的树，而分区器本来就在该深度收口
（512 面场景最多到深度 3）。能超深的只有绕过分区器判断的路径（实例子树、
结构预分配），目前没有这种场景，所以无法在不伪造树的前提下给出 PASS 证据。
已实测的证据只有两条：闸的实现是显式栈遍历（自身不会因超深递归崩掉）、
12 资产矩阵 + `longxiang.fbx` 的 `report.maxDepth` 全部 ≤ 48（见
VALIDATION_REPORT §1/§6）。反向对照用的是分区器在 `maxTreeDepth=1` 时
"仍然超硬预算"的失败路径，能证明**深度约束失效会让数据集出不了盘**，
但它命中的是 `partition:` 失败而不是 3b 本身。

> 后续（2026-10-19）：B-3 已解除（官方 validator 14/14 `numErrors=0`，见
> VALIDATION_REPORT §7），但它**不核对每瓦片 glTF primitive 数**，所以 A-32 段里
> 留给"外部 validator"的那一半缺口由新脚本 `scripts/glb_prims.js` 补上：
> 读回 341 个内容文件，`mesh.primitives` 最大 11 ≤ 24、i3dm 展开三角形
> 最大 3 588 ≤ 120 000，0 违例（B-9 解除，见 VALIDATION_REPORT §8）。

---

### A-33 i3dm 实例瓦片不做三角形预算，超限瓦片静默入库　**FIXED**
`ProductionPipeline::buildInstanceTree`

手册 §3 把「单瓦片 ≤ 120 000 三角形」列为不可自动放宽的硬限。b3dm 通路
（`finalizeTileFromWork`）有 `overBytes || overTris` 双判据，实例瓦片通路只有
`wo.bytes <= maxTileContentBytes` **一条**，`_tiles[].triangles` 因此可以在超限
状态下被登记成有效内容；`selfValidate` 到发布前才第一次发现，于是整批报废。

`lugouqiao_v11` 失败时把原因写成"内节点远景代理放不下"（A-31 的口径）是**误判**：
带 A-31 降级 + A-30 geo/tex 分账的二进制复跑（`lugouqiao_v14`，471 行日志）
仍然失败，14 条 ERROR 全在 `selfValidate: tile N has X triangles` 这一类，
且 N ∈ {954, 955, 1014, 1038, 1039, 1042, 1043}——远景代理的 WARNING 只到
tile 928，i3dm 瓦片 id 排在主树之后（实测 t05_instances：4 个瓦片里 i3dm 是
`i2.i3dm`），说明这批全是实例瓦片，而且**构建期一条告警都没有**。

根因是量纲错配：实例块大小只按 `maxInstancesPerTile`（默认 512）切，
glTF 侧 accessor 是共享的所以**字节不超**，但运行时按实例数展开绘制，
真实三角形数 = 实例数 × 原型三角形数。实测最高 507 822 面，是硬限的 4.2 倍。

**修复**（`ProductionPipeline.cpp:918-950`）：把原型解析/物化提到实例区间循环
之外，用 `ProxyMeshBuilder::countTriangles(protoGeom)` 反推块大小
`chunk = min(maxInstancesPerTile, maxTrianglesPerTile / protoTris)`；
原型单体就超限时显式失败（`prototype N has X triangles ... even as a single
instance`，不静默丢实例）；写盘后的门槛改成与 b3dm 一致的
`overBytes || overTris`，超限时删除暂存文件并报出实测字节/三角形/实例数/原型面数；
顺带补上此前漏记的 `_report.maxTrianglesInTile`（实例瓦片过去不进三角形统计）。

**验证**：`lugouqiao_v15`（同输入、同 profile，纹理缓存复用）
——见 STATUS.md「真实 FBX 端到端」与 `F:\wb_3dtiles_out\lugouqiao_v15.log`。
单元测试 84/84 通过（A-34/A-35 的 manifest 断言并入后 118/118）；实例预算收敛后 `t05_instances` 的
`maxTrianglesInTile` 从"只统计 b3dm 的 12"变为含实例瓦片的真实值。

---

### A-34 `manifest.aggregateProxies` 恒为空，代理身份无可查通路　**FIXED**
`ProductionPipeline::writeManifest`

手册 17 节要求「聚合代理显式使用代理 ID + 源 ID 集合映射」。
`TileFeatureMapper::addAggregateTile` 就是为此写的，但**全仓库没有调用点**：
代理簇是简化阶段（`ProxyMeshBuilder`）算出来直接回传到
`ProductionTile.aggregateSources` 的，mapper 侧根本看不到，
于是 `manifestJson` 里那一路 `agg` 永远是空数组。
`longxiang_covfix` 实测：`conversion-report.json` 报
`aggregateProxyCount=10`，同数据集 `feature-manifest.json` 的
`aggregateProxies` 是 `{}` —— 用户拿不到任何代理身份映射。

**修复**：删掉死 API（`addAggregateTile` / `_aggregateSources` /
`manifestJson` 里的空 `agg`），由 `writeManifest` 按 `tileUri` 从
`_tiles[].aggregateSources` 填入真实映射
（`{ "tiles/tN.b3dm": { "<proxyFeatureId>": [sourceFeatureId, ...] } }`），
mapper 侧保留 `"aggregateProxies": {}` 占位以稳定 manifest schema。
单测新增契约断言：manifest 必须带 `aggregateProxies` 对象。

**复验工具**：`node scripts/check_manifest_assertions.js <数据集|容器目录>`。
修复前实测记录见 `evidence/assertions-before-a34.txt`（13 个数据集里 2 个 FAIL：
longxiang 的 11 个聚合瓦片全部缺 `aggregateProxies` 条目，t03 缺 3 个）。

**测试覆盖与变异测试**：`testPipelineLeafCoverageInvariant` 新增 6 条固定断言 +
每个聚合瓦片 2 条（夹具实测 `aggPublished>=1`，所以不是空断言），用例总数
84 → **118**。写盘闸 `hasContent` 那一条做过变异测试：去掉它 118 条全过——
说明现有夹具走不到"降级但仍带 aggregateSources"这一支（构建期降级在
`w.aggregate` 赋值之前就 `return` 了），能走到的只有"估算达标但实测超限"
那条防御分支。该条按防御网保留并在代码注释里注明未被夹具覆盖，
不据此声称已有测试证明。

---

### A-35 同一数据集两套"出现次数"定义，report 与 manifest 数字不一致　**FIXED**
`TileFeatureMapper::sourceOccurrenceCount`

`conversion-report.json` 的 `manifestOccurrences` 走
`TileFeatureMapper::occurrenceCount()`（所有 batch 条目），
`feature-manifest.json` 的 `occurrenceCount` 在 `manifestJson` 里另算一份，
只统计 `features.contains(key)` 的条目——落到代理身份上的条目被
`continue` 跳过。`longxiang_covfix` 实测 **report 623 / manifest 571**，
差的 52 条正是聚合代理条目（A-29 之后代理不再冒充源 feature，所以它们
本就不该算"源出现次数"）。两个数各自都不算错，但放在一起就是可验证的自相矛盾。

**修复**：统一成 `sourceOccurrenceCount(const SourceModel&)` 一个定义，
`manifestJson` 与 `_report.manifestOccurrences` 都调它；删除无模型参数的
`occurrenceCount()`。单测 `testFeatureManifest` 增补负向断言：把某个 feature
从 SourceModel 里摘掉后，它在 batch 表里的条目不再计入源出现次数（5 → 4）。

**复验工具**：同一个脚本的第 1 条断言就是 `report.manifestOccurrences ==
manifest.occurrenceCount`；修复前 longxiang 实测 623 / 571 不一致
（`evidence/assertions-before-a34.txt`）。

---

### A-36 `boxJson` 把 NaN / 未初始化包围盒折成零，坏 bounds 能"洗白"出盘　**CLOSED**
`ProductionPipeline::boxJson` / `selfValidate` 1b

手册 17 节把"NaN bounds 为零"列为验收门槛，但当时发布链路上**没有任何一处**
检查包围盒：`boxJson` 里 `std::max(0.0, nan)` 返回 `0.0`（`std::max` 的
`a<b` 语义遇到 NaN 恒为 false），未初始化的 `osg::BoundingBox` 又整支返回
12 个 0。也就是说源数据一旦出现非有限顶点，产物不会报错，而是得到一个
**贴着原点的零尺寸盒**——JSON 完全合法、validator 也挑不出毛病，
但 Cesium 拿它做视锥剔除会把整棵子树剔掉。

**修复**：`selfValidate` 增补 1b——逐个瓦片检查序列化后的
`boundingVolume.box` 12 个数全部有限、半轴非负、`geometricError` 是有限非负数，
任一条不满足即拒绝发布（不是记 warning）。

**测试**：`testPipelineBoundsGate`（`bounds`，12 条断言，
`evidence/test_production-130.txt`）。这里有一条必须如实记下的边界：
**JSON 表示不了非有限数**。`inf`/`NaN` 没有合法字面量，`1e999` 这类溢出在解析期
就抛 `json.exception.out_of_range.406`（nlohmann 不折成 `inf`），反向 `dump()`
把非有限 double 写成 `null`。所以管线内部的 NaN 到磁盘上只可能以 `null` 出现，
用例断的是完整链条（NaN 半轴 → `null` → 发布闸拒绝），而 `std::isfinite`
那几条在合成数据里无法被触发——它们防的是别的写入者（手写 tileset、上游工具）。
第一版用例正是踩了这个坑：`is >> json` 抛出未捕获异常 → `terminate()` →
进程静默消失，全量计数从 129 掉到 71，看起来像"用例互相干扰"。harness 现在
逐用例 try/catch + `test_production_progress.txt`（有 `>> name` 没 `<< name`
就是崩在中途），这类"少跑了但没人报错"再也不可能被读成绿灯。

---

### A-37 "父盒覆盖子盒"在代码里没有检查点　**CLOSED**
`ProductionPipeline::selfValidate` 1b

`boxJson` 的注释里写着历史 bug"t04/t05/t06/t08/t10/t11 都报过 child box not
covered"（float 中心舍入让 `center-half` 与 `min` 差出 1e-5），修法是全程
double。但**这条约束本身没有留在代码里**，只有注释——改回去也不会有任何检查
失败。本轮把它写成发布闸：对每个父子对按三条轴检查
`|Δcenter| + hChild <= hParent + eps`（eps 取父半轴的 1e-6 相对容差）。

回扫工具 `node scripts/tileset_bounds.js <目录>` 对既有 **18 个数据集**实测
父不覆盖子 0、NaN box 0、负半轴 0、非法 geometricError 0
（`evidence/tileset_bounds-before-a37.txt`）。所以这不是修一个已经发生的缺陷，
而是把该项从"只能靠外部工具回扫"变成"发布前自校验拦截"。

> 记一句踩坑：这条检查第一次实现时把半轴下标写成 `b[4]`/`b[8]`（以为盒布局是
> `cx,cy,cz` 后每轴 `center,half` 交替），合成夹具立刻报 23 条"父不覆盖子"。
> 独立写的 node 回扫器对同一批真实产物报 0，才把根因定位到检查本身——
> 盒布局是 `[cx,cy,cz, hx,0,0, 0,hy,0, 0,0,hz]`，半轴在 3/7/11。
> 两个互相独立的实现对照，是这次没把一个假缺陷写进产品记录的原因。

**测试**：同一 `bounds` 用例覆盖这条——三轴同时"紧贴"父盒内壁（等号情形，
`eps` 不能太紧否则误伤）必须放行，X/Y/Z 各自单独越界必须拒绝且**报对轴号**。
轴号断言是这条用例的主要价值：半轴下标写错时 Y/Z 的越界会被读成半轴 0 而
静默放行，上面那个踩坑只有靠"逐轴单独越界 + 校验报出的轴号"才拦得住。

---

### A-38 偶发单个荒谬顶点：有限性/父子覆盖都放行，根因未定位　**根因 OPEN / 发布闸 CLOSED**
`ProductionPipeline::selfValidate` 2b

矩阵数据集 `t02_thin_features`（`runs/matrix_a34/`，源是 ±50 m 的薄片网格）
一次运行产出 7 个瓦片，其中 **t0/t4/t6 三个**带同一个顶点，实测值三处完全一致：

```
b3dm POSITION accessor max = [6.555e+10, 5.000e+1, 4.474e+27]   （其余 4 个瓦片 max ≈ 45）
tileset.json 根节点 geometricError = 5.825846101672932e+24      （正常应 ≈ 23 m）
```

也就是说这不是"精度差一点"，是一个被索引引用、数量级 1e27 的顶点，几何、包围盒、
SSE 全部被它顶穿，而 JSON 依然 schema 合法、Cesium 只会表现成"远景永远在细化"。

**A-36/A-37 的新闸拦不住它**：值是**有限**的；父盒和子盒是从同一份污染数据算出来的，
"父覆盖子"照样成立。所以补 2b——每个有内容的瓦片，其 `contentBounds` 必须落在
**源模型范围 ± 0.5×源对角线**内。放宽量给到源尺度的一半，是因为远景代理的顶点
确实会被简化器移动；但任何真实输出都不可能被移动出源模型尺度这个量级
（本例 6.555e10 vs 源 ±50 m + slack≈87 m，超出 8 个数量级）。

**独立佐证（外部工具，不是我们自己的读数）**：官方
`3d-tiles-validator@0.6.1` 对同一批产物报 **3 个 ERROR**，路径与条数完全对应这三个
瓦片，且都落在第二个 mesh 的第一个 primitive 上：

```
tiles/t0.b3dm  /meshes/1/primitives/0/attributes/NORMAL
               Vector3 at accessor indices 234..236 / 246..248 / 258..260 is not of unit length: 0.
```

234/246/258 是标量下标，除 3 即向量下标 78/82/86，与自研探针
`scripts/b3dm_attr_probe.js` 报的位置逐一对齐。同一资产上一轮
（`validation/t02_thin_features.validator.json`，报告时间 06:26Z，同一 validator
版本）是 **0 error**——所以缺陷不是"一直在那儿、只是没人看"，是间歇出现。

**缺陷形态（本轮逐元素差分测出，比上一版的推测硬得多）**：拿污染产物与同一
二进制重跑 5 次的干净产物做 POSITION/NORMAL **逐顶点比较**，92 个顶点里只差
6 个槽：

```
POSITION  diff=1  @86 (=count-6)   干净=(0,0,0)   污染=(6.555e+10, -1.195e-8, 4.474e+27)
NORMAL    diff=5  @78,79,82,84,86  干净=平滑法线（如 (-0.5315,0.1772,0.8283)）
                                    污染=3 个零向量 + 2 个轴对齐旧值 (0,±1,0)
t4（52 顶点）与 t6（22 顶点）里相对位置完全相同：count-14/-13/-10/-8/-6
```

三条事实由此确定，也都推翻了上一版写在这里的推断：

1. **不是"整段缓冲区没填"**。其余 86 个槽逐位相同、污染槽是**内部**槽（它后面
   还有 5 个正常槽），所以形态是"个别槽装着另一份数据"——旧值、零值、或回收来的
   字节。同一个槽在干净跑里是 0、在污染跑里是 1e27，说明读到的就是**分配器当时
   还给我们的东西**，这也正是"偶发/不复现"的来源：缺陷本身是确定的，**可见性**
   才随机。
2. **三个瓦片是同一条尾巴的三个拷贝**：`t4` 的 mesh1 POSITION 数组（52 个）与
   `t0` 的 mesh1 POSITION 数组 `[40..91]` **逐字节相等**；`t6`（22 个）的后 14 个
   与 `t0[78..91]` 逐字节相等、前 8 个不同，而它的洞正好落在对应位置 8/12/16。
   所以污染源是**一条被多个瓦片共享的源顶点尾巴**，凡是拷贝到该顶点的瓦片都带上；
   那 12 个字节（`5031745138554db23450676d`）拿去扫仓库 + 输出盘共 58 133 个
   瓦片/模型/清单文件，只出现在这三个文件中，各一次（`t0@41928 / t4@21408 /
   t6@9164`）。
3. **上一版写的"X 分量在算 bounds 之前就在、Z 分量是序列化时才冒出来的"是错的**，
   是我漏算了 glTF 的 Y-up/Z-up 轴交换：tileset 盒的 Y 极值 `-4.474249806084812e+27`
   与 b3dm accessor 的 Z 极值 `+4.474e27` 是**同一个分量**的两种写法，盒的
   X `[0, 6.555e10]`（精确值 min=-50、max=6.555e10）与 accessor 的 X 也一致。
   正确结论平淡得多：从包围盒计算到写盘，全程都带着同一个污染顶点，中间没有
   "两个版本"。留这段是为了记住教训：**跨坐标约定比对数值前先确认轴序**。

已排除（都做过实验，不是推测）：加载器的有限性守卫、`materialize` 出的新数组 +
索引范围守卫、单线程分区（关掉并行仍然复现过一次）、`Simplifier::remapArray` 的
长度/计数双守卫（`src/utils/Simplifier.cpp:71,83`，长度不符直接放弃本次简化）。
`SourceModel::materialize` 里法线是**条件** push（`SourceModel.cpp:791`）因而可能比
POSITION 短，但落盘前有 `nrm->size() == pos->size()` 守卫（`SourceModel.cpp:842`）
会把整个法线数组丢掉，不会留下"短数组被补齐"的空洞；而
`ProxyMeshBuilder::mergeGeometries` 的**同名守卫缺失**（`ProxyMeshBuilder.cpp:361`
只看 `nrm.valid()`，不查长度）是一处真实隐患，已记在 A-38 名下但**不是**本例的
解释——本例 `aggregateProxyCount=0`，没走聚合路径。

**证据边界（如实说）**：2b 目前**没有**合成单测——`contentBounds`/`_model.bounds`
是私有构建期状态，测试没有注入点，为了"覆盖率高"而伪造一个能被触发的分支是没
意义的。也没有复现样本可供端到端实测：本轮又跑 36 次（30 次默认并行滑窗：5 串行 +
12 进程并发 `WB_JOBS=32` + 3 次 `WB_PROXY_DEBUG=1` + 10 串行；另 6 次 `WB_JOBS=1`），
**没有一次**重现污染。把污染字节 `5031745138554db23450676d` 拿去扫全部仓库 + 输出盘
（58 133 个瓦片/模型/清单文件），命中仍然只有那 3 个文件、每文件 1 次。
因此 2b 对本例的拦截是**按数值推演必然触发**（X=6.555e10 对源 ±50 m + slack≈87 m），
不是实测记录。根因（究竟是哪段代码往这些槽里写了旧数据/没写）未定位，但已从
"随机堆损坏"收窄到"某条构建路径按槽位复用了一份数组，且写入方与读取方看到的
不是同一份内容"；下一步该从这里查起，见 A-40 的非确定性测量。发布口径不变：
宁可拒绝发布，不可出一个能过 validator 但不可渲染的数据集。

回扫工具：`node scripts/b3dm_attr_probe.js <数据集目录或 .b3dm>`，逐 primitive
关联 POSITION/NORMAL/索引，报 `@count-k` 相对下标（本例的洞全部贴着数组尾巴，
用绝对下标在 92/52/22 三种长度上看不出是同构缺陷）。上面每一条数字的原始输出
（探针、validator ERROR 全列表、逐顶点差分、切片相等验证、污染字节全库扫描、
37 次产物的逐瓦片哈希对比、轴序更正）都在 `evidence/a38-forensics.txt` §1–§8。

---

### A-40 同一二进制、同一输入，产物不逐位一致　**OPEN**
`ProductionPipeline::buildB3dmTree` 并行窗口 / 误差评估分支

`t02_thin_features`（0.3 s 的小资产）共得到 37 份产物（30 次默认并行滑窗 + 6 次
`WB_JOBS=1` + matrix_a34 那一次），`tiles/*.b3dm` 逐字节：**35 次完全相同**，
**1 次（`WB_PROXY_DEBUG=1` 的第 3 跑）整棵树变了**——7 个瓦片的字节数全不同（`42808/25160/13712/12768/18968/
10552/10448` → `42832/22928/13688/10552/21944/12776/10472`），`nodesBoundLoose`
1→3，峰值 RSS 34.4→47.3 MiB；剩下 1 次就是 A-38 的污染产物。`WB_PROXY_DEBUG` 在代码里只做日志开关
（`Simplifier.cpp:16`、`ProxyMeshBuilder.cpp:35`），不改变任何分支，所以变的是
**时序/内存占用**，不是配置。

这件事的分量比"慢一点"大：VALIDATION_REPORT §1 里"两次独立运行 tile 目录 md5
相同"的结论只在**样本=2、且都是串行轻载**时成立，本轮样本=36 把它打破；而且
A-38 的污染产物**实测**就落在这条非确定性分支上——把三方逐瓦片哈希摆在一起
（`evidence/a38-forensics.txt` §8）：污染那次的 t1/t2/t3 与 `WB_PROXY_DEBUG` 的树变体
**逐字节相同**（sha1 分别 `bc6ae84633/434bb635cc/4e9b50cb2d`），而这两份都与基线不同；
基线的 t5 和变体的 t3 更是同一个文件（sha256 `3e0d68a79eacfa45…`，均 10 552 字节）。
也就是说"哪份几何落到哪个瓦片 id"本身随时序变化，内容哈希却没变——症状不是数值算错，
是**分配/收集次序**不确定。于是"产物不可复现"和"产物偶发坏"不再是两个假设同根，
而是同一棵树上的两个观测点：构建期存在依赖运行时时序的状态（并行滑窗里
`processTileHeavy` 与主线程 `finalizeTileFromWork` 共享源模型的数组/包围体，
或误差评估的分支被缓存状态影响），通常表现为"树换了/槽位换了"，偶尔表现为"某个顶点
槽位装着旧数据"。

已测：并行度不是**常态**下的变量——`WB_JOBS=1` 连跑 6 次，与默认并行滑窗的基线
**逐字节相同**、彼此也相同（`evidence/a38-forensics.txt` §7）。也就是说关掉并行不
改变"35/37 次都一样"这件事，那条偶发分支（1 次树变体）在单线程样本里还没被抓到，
所以不能把 A-40 简单归因成"并行窗口本身"，只能说：并行度对结果**通常**无影响，
但存在低概率的时序/内存相关分支。原始逐瓦片哈希表见 `evidence/a38-forensics.txt` §5/§7/§8。
待查（按可疑度排序）：① `processTileHeavy` 的并行窗口里对**共享** `SourceMesh`
数组/`osg::Geometry` 的读写次序；② `GeometryErrorEvaluator` 的
`budgetExhausted`→`boundLoose`→上层"是否继续细分"这条反馈环是否被别的瓦片的
状态污染；③ `mergeGeometries` 缺少的长度守卫（`ProxyMeshBuilder.cpp:361`）。
新增线索：既然同一块内容会在 t3/t5 之间换槽位，应先查**瓦片 id 分配与结果收集**
是否按完成次序而非稳定键排序（`submit` 按三角形数降序、`collect` 按瓦片 id，
但 id 本身是在细分过程中生成的）。
复现路径已具备：`for i in $(seq 1 30); do 转换; 比对 tile 哈希; done` 即可，
不需要真实模型——这也是把这条列成独立审计项而不是并入 A-38 的原因。

---

### A-39 `appendElements` 按源类型 cast 目标数组　**CLOSED（加固）**
`ProxyMeshBuilder.cpp::appendElements:217`

`mergeGeometries` 的合并分组靠 `AttrSig`（法线/UV/颜色/逐顶点属性的**类型**参与
签名）保证同组属性类型一致，而 `appendElements` 只按 `src->getType()` 分支、
把 `dst` 也按同一类型 `static_cast`。类型一旦不一致就按错误步长写 —— 那不是
"属性丢了"，是堆越界写。

现存调用链不会触发：位置走 `dynamic_cast<const osg::Vec3Array*>`（不匹配就跳过
整个几何），其余属性的 dst 由 `cloneEmpty(group.front() 的同类数组)` 生成，签名
又约束了类型一致。**所以 A-39 不是 A-38 的解释**，我把它单列是因为这类
"前提写在另一个文件里"的 cast 迟早会被下一个调用方踩到。现在显式
`dst->getType() != src->getType()` 就返回 false，调用方原有的
`if (!appendElements(...)) { nrm = nullptr; }` 语义把它降级成"该属性不进合并
几何"，是丢属性而不是写坏内存。

---

## 与手册「已发现风险清单」的对应关系

| 手册风险 | 结论 |
|---|---|
| `Tile::computeGeometricError()` 体积加权平均 + 最大子误差比较 | **CONFIRMED** → A-01 |
| `Tile::applyLODStrategy()` 固定比例/常数误差/固定三层 | **CONFIRMED** → A-02 |
| `Tile::buildLOD()` 中 `tileLOD0->refine = ADD` | **CONFIRMED** → A-03 |
| `Tileset::computeGeometricError()` 固定相机参数 | **CONFIRMED** → A-04 |
| `getAllDescendantNodes()` / `computeBoundingVolumeBox()` 重复递归 | **CONFIRMED** → A-06 |
| `processOverSizedNodes()` 固定 2×2×2 + 包围球中心 | **CONFIRMED** → A-08 |
| `regroupI3DMTile()` 树范围外实例 TODO | **CONFIRMED** → A-09 |
| `BatchIdVisitor` 覆盖属性 0 | **CONFIRMED** → A-11（并额外发现 A-12） |
| `Tileset::valid()` / `Tile::valid()` 掩盖错误 | **CONFIRMED** → A-05 |
| TBB 与非 TBB 路径都要可构建 | **CONFIRMED 不满足** → A-14 |
