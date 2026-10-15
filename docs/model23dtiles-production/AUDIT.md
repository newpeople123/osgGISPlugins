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
