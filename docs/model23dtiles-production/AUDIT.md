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
| A-38 | 产物里偶发单个荒谬顶点（矩阵 t02：X=6.555e10 / Z=4.474e27），源模型里没有；有限性与父覆盖子都拦不住（父盒同样被污染）。逐元素差分定位到**缺陷形态**：一份共享源顶点数组里 1 个 POSITION 槽 + 5 个 NORMAL 槽装着"别的数据"，凡拷贝到该区间的瓦片都带上；值本身随分配器状态变化，所以看起来偶发。**累计 127 次产物只出现 1 次**（matrix_a34），同一二进制（15:47:04，不含 2b）追加 90 次（3 配置 × 30）**0 次重现**，污染字节全库仍只命中那 3 个瓦片；那一次的 t1/t2/t3 与 A-40 的"树变体"**逐字节相同** → 污染那次就是这条非确定性分支上的一个产物 | `ProductionPipeline.cpp::selfValidate` 2b | OPEN（写入方未定位，90 次新样本未产生可差分污染产物）+ CLOSED（发布闸拒绝出盘） | 12.1/17 | 高 |
| A-39 | `ProxyMeshBuilder::appendElements` 用**源**数组类型 `static_cast` **目标**数组，属性类型不一致时按错误步长写（堆越界） | `ProxyMeshBuilder.cpp::appendElements` | CLOSED（显式拒绝 dst/src 类型不一致；实测现存分组不会触发，属加固） | 5.3 | 中 |
| A-40 | 同一二进制、同一输入，产物**不逐位一致**：t02 累计 127 次产物里 121 次落在同一基线，6 次是**整棵瓦片树变了**的变体（共 5 个互不相同的哈希；`nodesBoundLoose` 在 0/1/3、`nodesCertified` 在 1/2/3 之间跳，峰值 RSS 也随之变）。变体同时出现在默认并行(jobs8)、`WB_JOBS=4`、`WB_PROXY_DEBUG=1` 三种配置，其中 jobs4 那次的哈希与旧记的"WB_PROXY_DEBUG 第 3 跑"变体**完全相同** → 变体与调试开关、与并行度都无绑定（旧结论"不是并行度本身"证据仅 n=6 已撤回）。而且**同一份 b3dm 内容会换瓦片槽位**（基线 t5 == 变体 t3，sha256 相同）——症状不是数值算错，是分类/分配次序依赖运行时时序。**本轮新增第二个样本 t09（单轮 0.75 s）**：22:40 二进制单独跑 8 次得 3 个 `t0.b3dm` 变体，矩阵内那次是第 4 个，且 `errMax` 随变体在 386.77 ↔ 331.62 之间动；6/8 次逐字节等于 22:14 那轮的树 → 变体也与**二进制版本**无关 | `ProductionPipeline::buildB3dmTree` 并行窗口 / 误差评估分支 | OPEN（非确定性来源未定位；jobs4 也复现，说明"关掉并行即可稳定"未经证实；t09 可用 8×0.75 s 廉价复现，是后续定位的首选样本） | 17 | 高 |

| A-41 | i3dm 实例瓦片的**每实例位姿全部丢失**，且原型几何被同组 N 个实例矩阵**就地累乘**污染。链条：`writeContent` 让一份原型 drawable 被 N 个 `MatrixTransform` 共享（`ProductionPipeline.cpp:963-972`）→ `FlattenTransformsVisitor` 用 `osgUtil::TransformAttributeFunctor` **就地**改这份共享数组（`GltfOptimizer.cpp:549-554`）并把每个 MatrixTransform 复位成单位阵（`GltfOptimizer.cpp:563-565`）→ `createFeatureI3DMTableBinary` 只能分解出单位阵（`ReaderWriterGLTF.cpp:302-345`）→ POSITION 全 0、SCALE 全 1。**t05 定量证实**：299 个实例的合法平移合计 (273.09, 2956.73, −1788.77) 与出盘几何相对原型的偏移 (273.09, 1788.78, 2956.72) **逐项相等**（只差 Y-up 的 (x,−z,y) 轴序）。lugouqiao_v15：126/126 个 i3dm 瓦片 POSITION 全 0；80/126 是 IL≥2（共享数组被累乘）；6 个瓦片烘焙坐标 >1e4，最坏 `i1018.i3dm` \|v\|=1.22e8 m，而同一数据集里所有实例变换的矩阵元素绝对值最大只有 378；root 包围盒被抬到 1.33e8 m、root `geometricError` 抬到 7.65e7 m，全树 LOD 阈值随之失效 | `GltfOptimizer.cpp::FlattenTransformsVisitor` + `ProductionPipeline.cpp::writeContent` / `buildInstanceTree` | FIXED（两段分开修：flatten 掩码 22:40 生效；"相对原型"那半在 22:40 是**空操作**——`pw.invert(invProto)` 把方向写反，23:53 才真正生效。验收口径同时从"命中绝对平移"改到相对帧。v15/v16 与所有 22:40 产物一律作废） | 5.5 / 12.1 | 高（阻断级：实例内容不可渲染） |
| A-42 | 发布产物里泄漏**本地绝对路径**，且瓦片字节数随输出目录长短变化。两个口子都是"路径当名字"：`Osg2Gltf.cpp:763/766` 把纹理的完整输出路径写进 GLB `bufferView.name`/`image.name`（随 `.b3dm/.i3dm` **分发出去**：lugouqiao_v15 实测 **691/1057** 个瓦片带 `F:/wb_3dtiles_out/lugouqiao_v15\textures\…`）；FBX 导入器用源文件绝对路径命名根节点，顺着 `nodePath` 进 `feature-manifest.json`（同数据集 **2729** 处 `D:/BaiduNetdiskDownload/…fbx/…`）。副作用更难查：name 进 GLB JSON chunk → 同一输入、同一代码，t12 的 `t0.b3dm` 在不同输出目录下出现过 510432/510440/510344/510304/510296 五个尺寸，**以前把这类差异算成 A-40 的噪声** | `Osg2Gltf.cpp::getTexture` + `SourceModel.cpp::SourceModelVisitor::apply(Node&)` | FIXED（exe 00:12 + 插件 00:16：两处只留文件名；`path_leak_scan.js` 复扫 `matrix_a42fix` **0/213 瓦片、0/36 json**；两个长度差 40 字符的目录产出 sha256 相同；`test_production` 150/150。**`osgdb_gltf` 是 MODULE 插件，按目标构建不会重建它**） | —（手册未列：交付物含本地路径） | 中（分发物带操作者目录结构；并污染"逐字节可复现"这一验收口径） |
| A-43 | 产物里有零长度 NORMAL：`lugouqiao_v15` 实测 **195 个顶点 / 26 个瓦片**，其中 **55 个落在非退化三角形上**（法线本该算得出）。旧判据用绝对面积阈值，1e-8↔1e-9 之间"真隐患"档在 36↔55 之间跳，无法定级 | FBX 导入器的源法线 **或** `Osg2Gltf`/`GltfOptimizer` 的 NORMAL 写出通路（未定，判别要动构建） | OPEN（本轮补齐尺子：主判据改尺度相对 `REL×中位面积`、绝对阈值降为对照列，`make_zero_norm_fixture.js` 做阳性对照；已排除"简化器独有"（4 个 `EXACT_LEAF` 瓦片各 3 条全在非退化面上）与"A-38 同族"（值确定、跨 LOD 同签名）；源侧结论待 `WB_DUMP_SOURCE` 或 v17 复扫） | —（手册未列：非单位法线） | 待定（低～中：可渲染，局部 shading 异常 + validator 逐条告警；不建议按"拒绝发布"处理） |

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

**复验范围（已扩到全量产物，不只是 longxiang 一对）**：
`evidence/manifest_occurrences-a35-boundary.txt`。373 份带 manifest 的产物全扫，
**缺 key 的 0 份、两个数不一致的 5 份**——除 `longxiang_covfix`(571/623) 外还有
`wuhu_v1`(228/263)、`matrix_texless/t03`(4/11)、`matrix_covfix/t03`(6/9)、
`matrix_final/t03`(4/11)，产物时间全部早于修复提交 `262dc77`(16:39:55)；
该提交之后的 4 个矩阵批次 46 个数据集 + F 盘复跑产物 **0 处不一致**。
顺手钉死一个反复记错的点：`occurrenceCount` 只在 `feature-manifest.json` 里、
`manifestOccurrences` 只在 `conversion-report.json` 里，两个文件各用各的键名，
"report 没有 occurrenceCount"不是缺陷。

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

已排除（都做过实验或读码确认，不是推测）：加载器的有限性守卫、`materialize` 出的新数组 +
索引范围守卫、单线程分区（关掉并行仍然复现过一次）、`Simplifier::remapArray` 的
长度/计数双守卫（`src/utils/Simplifier.cpp:71,83`，长度不符直接放弃本次简化）。
`SourceModel::materialize` 里法线是**条件** push（`SourceModel.cpp:791`）因而可能比
POSITION 短，但落盘前有 `nrm->size() == pos->size()` 守卫（`SourceModel.cpp:842`）
会把整个法线数组丢掉，不会留下"短数组被补齐"的空洞；而
`ProxyMeshBuilder::mergeGeometries` 的**同名守卫缺失**（`ProxyMeshBuilder.cpp:361`
只看 `nrm.valid()`，不查长度）是一处真实隐患，已记在 A-38 名下但**不是**本例的
解释——本例 `aggregateProxyCount=0`，没走聚合路径。

本轮把"未初始化 size 构造数组"这一整类写法逐个排掉了：`BatchIdVisitor` 只用
`assign()`（`BatchIdVisitor.cpp:65,69`）；`Osg2Gltf.cpp:194` 那唯一一处
`new osg::Vec4Array(n)` 下一行就 `std::fill` 全覆盖；`mergeGeometries`/`appendElements`/
`cloneEmpty` 全用 `reserve + insert/push_back`（`ProxyMeshBuilder.cpp:229,257,337`）；
`Simplifier` 的索引缓冲是 `std::vector<unsigned>`，`resize` 值初始化为 0 不是脏值。
**都不留脏槽。**

也**推翻**了上一版留在文档里的一条猜测——"Osg2Gltf 的 buffer 缓存按裸指针键，数组
释放后地址被回收 → 新数组拿到一个旧 accessor"。读码否掉了：`_buffers/_bufferViews/
_accessors` 的键是 `osg::ref_ptr<const osg::BufferData>`（`include/osgdb_gltf/Osg2Gltf.h:12,20-22`），
map 持引用，**一次瓦片写出里每个见过的数组都被钉住不释放**，地址不可能在同一次写出内
被回收复用。所以这个"悬垂指针缓存"机制不成立。

顺带从这个"钉住 + 首触快照"设计里读到**两条潜在隐患**（都还**没有**证据把它和 A-38
的坏顶点签名挂上，如实标注为待验证）：
① `getOrCreateBuffer` 只在**第一次见到**该数组时 `copy` 一次字节（`Osg2Gltf.cpp:453-457`），
之后同一数组再被改也不重拷——若某数组在写出过程中被就地改（如 `Osg2Gltf.cpp:158`
`std::transform(... normalize())` **就地**改共享法线数组），accessor 元数据取的是改后
的 `getNumElements()`，字节却是首触快照，理论上能造出"元数据与内容两个版本"。
② `getOrCreateAccessor` 也按指针去重（`Osg2Gltf.cpp:498-500` 命中就早返回），而
`setPositionAccessor` 会按 `DrawArrays::getFirst/getCount` 写 `byteOffset/count`
（`Osg2Gltf.cpp:538-542`）：**同一 positions 数组被两个不同 DrawArrays 图元共享时，
第二个图元直接继承第一个的 byteOffset/count**——这是范围错，不是脏值，仍待判断是否与
本例有关。发布口径不变：宁可拒绝发布，不可出一个能过 validator 但不可渲染的数据集。

**证据边界（如实说）**：2b 目前**没有**合成单测——`contentBounds`/`_model.bounds`
是私有构建期状态，测试没有注入点，为了"覆盖率高"而伪造一个能被触发的分支是没
意义的。也没有复现样本可供端到端实测：又跑了 **90 次**（`node .workbuddy/a38_rate/drive.js 30`，3 配置 × 30：默认并行 `jobs8_plain`、
`WB_PROXY_DEBUG=1` 的 `jobs8_dbg`、`WB_JOBS=4` 的 `jobs4_plain`），把每一次的每个
`.b3dm` 都做了污染字节 `5031745138554db23450676d` 的 `indexOf`，**90 次 0 命中**；
更早的 36 次（30 默认并行 + 6 `WB_JOBS=1`）同样 0 重现。连同 matrix_a34 那次，
累计 127 份产物里污染**仍只有 1 次**，全库扫描（58 133 个文件）命中仍只有那 3 个
瓦片、每文件 1 次。所以"写入方是谁"这一步**没有**新样本可差分——90 次没再造出
一个坏顶点，能推进的只有下面 A-40 那条时序分支（本轮 4/90 的树变体就落在它上）。
因此 2b 对本例的拦截是**按数值推演必然触发**（X=6.555e10 对源 ±50 m + slack≈87 m），
不是实测记录。根因（究竟是哪段代码往这些槽里写了旧数据/没写）仍未定位。本轮把范围
从"随机堆损坏"往下压了两层：脏值形态（个别内部槽装着旧/零数据、三个瓦片是同一条
尾巴的拷贝）+ "未初始化 size 构造"整类写法与"裸指针缓存地址回收"机制都已排除，剩下
的候选都属"**数组字节快照与元数据不同步**"这一类——即上面 ① 首触快照 / 就地改、
② 共享数组 accessor 继承；下一步从这两条 + A-40 的时序分支查起。发布口径不变：
宁可拒绝发布，不可出一个能过 validator 但不可渲染的数据集。

回扫工具：`node scripts/b3dm_attr_probe.js <数据集目录或 .b3dm>`，逐 primitive
关联 POSITION/NORMAL/索引，报 `@count-k` 相对下标（本例的洞全部贴着数组尾巴，
用绝对下标在 92/52/22 三种长度上看不出是同构缺陷）。上面每一条数字的原始输出
（探针、validator ERROR 全列表、逐顶点差分、切片相等验证、污染字节全库扫描、
37 次产物的逐瓦片哈希对比、轴序更正、追加 90 次（3 配置 × 30）的可复现性与零重现
实测）都在 `evidence/a38-forensics.txt` §1–§9。

另一条**独立判据**的全量基线（本轮新增 `scripts/accessor_extents_scan.js`，
原始输出 `evidence/accessor_extents-baseline.txt`）：把每个瓦片的 accessor **声明
min/max 与按字节实算逐条对账**、查 POSITION 里有没有非有限分量、再单独看
`|coord|>1e6` 的幅度。三条都要留着，因为它们互相拦不住——本例污染的顶点**连声明的
max 一起污染**（实测 t02
"声明≠实算 = 0"、">1e6 = 3 个瓦片"，与上面 forensics 记录的三个文件逐一对上），
所以 min/max 全等这条**不构成** A-38 的防线；反过来把某个顶点改成 1234.5 只让
min/max 那条报警，把某个分量改成 `NaN` 则只有"非有限"那条报警（NaN 参与 `<`/`>`
恒为 false，既不动实算 min/max 也不动幅度）。修复前后共扫了 **124 个数据集 /
3 687 个瓦片 / 14 884 个 VEC3 accessor，声明≠实算 = 0、非有限 = 0**，幅度判据在
合法产物上的唯一命中是 fixture `t08_big_coords`（大坐标用例，本就该 1e7，用
`--abs 1e8` 即分开）。真实 FBX `lugouqiao_v15` 跑完之后的全量：
**931 个 b3dm（A-38 的作用面）最大只有 4 337 m、>1e6 命中 0**，
所以"这份产物没有 A-38 签名"从抽样升级成了逐 accessor 实测。

**2b 闸门自身的两条实测（见 A-41）**：① 第一版用 `_model.bounds` 当参考系是**错的**——
`SourceModel.cpp:578-584` 在识别重复实例后把 `bounds` 重算成"剔除实例几何"的范围，
所以每个实例数据集里**正常**的 i3dm 瓦片都会被判成越界（matrix_a38gate/t05 实测 rc=3 拒发，
而同一输入在同一代码路径的 matrix_a34 / covfix / final 三次里都正常出盘）。参考系已改为
全部 `_model.objects` aabb 的并集。② 改完后 t05 仍被拒，但那是**真阳性**：拦下的正是 A-41
的实例矩阵就地累乘（内容 y=2970 vs 合法实例最高 y=20）。结论要如实记：2b 到目前为止
**没有**在 A-38 本人的污染样本上实测过（127 次里那 1 次无法复现，没有新鲜坏样本可拦），
它被实测触发的两次，一次是它自己的误判、一次是它抓到了另一个缺陷。

---

### A-40 同一二进制、同一输入，产物不逐位一致　**OPEN**
`ProductionPipeline::buildB3dmTree` 并行窗口 / 误差评估分支

`t02_thin_features`（0.3 s 的小资产）累计 127 份产物：§5 的 30 次默认并行滑窗、§7 的
6 次 `WB_JOBS=1`、matrix_a34（A-38 那一次），加本轮 `node .workbuddy/a38_rate/drive.js 30`
的 3 配置 × 30 = 90 次（默认并行 / `WB_PROXY_DEBUG=1` / `WB_JOBS=4`）。`tiles/*.b3dm` 逐字节：

- **121 次落在同一基线** `6e606dca3487ecca…`；
- **6 次是"整棵树变了"的变体，共 5 个不同哈希**：`fe354714…` 本轮由 `jobs4_plain`
  跑出、旧 37 次里由 `jobs8_dbg` 跑出，**同一个哈希、两种配置**；`4c1e17f9…` 由
  `jobs8_plain`、`ed320972…`/`e2e55a08…` 由 `jobs8_dbg`；`d6dfbeef…` 就是 matrix_a34
  的 A-38 污染产物。

变体的 7 个瓦片字节数全不同（基线 `42808/25160/13712/12768/18968/10552/10448` →
`fe354714` 变体 `42832/22928/13688/10552/21944/12776/10472`）；`nodesBoundLoose` 在
**0/1/3** 之间跳、`nodesCertified` 在 **1/2/3** 之间跳（基线恒为 loose=1 cert=3），
峰值 RSS 34.4→47.3 MiB。`WB_PROXY_DEBUG` 在代码里只做日志开关（`Simplifier.cpp:16`、
`ProxyMeshBuilder.cpp:35`），不改变任何分支，且同一 `jobs8_dbg` 配置里 28/30 仍是基线，
所以变的是**时序/内存占用**，不是配置。

**本轮新增：第二个资产复现，且单轮只要 0.75 s**（取证成本远低于 t02 的 30 次滑窗）。
`t09_cross_tile_object` 用 22:40 二进制单独连跑 8 次：`t0.b3dm` 落在 **3 个变体**上——
6 次 `95b36adf74…`（`maxContentBytes=41120`）、1 次 `792a9422a0…`、1 次 `8e8f5da801…`
（后两次 `41144`），8 次的 `errMax` 都是 386.77、`nodesCertified/nodesBoundLoose` 都是 3/3；
而**嵌在矩阵里跑的那一次**（`runs/matrix_a41fix2/t09`）是**第 4 个**变体，
`maxBytes=41152`、`errMax=331.62`、7 个 b3dm 全部换哈希。两个要点：
① 变体与**二进制版本无关**——22:40 的产物有 6/8 次逐字节等于 22:14 那轮的 t09 树，
而 t09 的 `instanceTileCount=0` 根本不进 A-41 改动的分支，所以矩阵之间 t09 的差异
**不能**记在 A-41 账上（这是把"改动引起"证伪，不是猜测）；
② **`errMax` 本身随变体动**（386.77 ↔ 331.62），说明这条非确定性不只搬"槽位"，
还会改变**误差评估的结果**——比 t02 只观测到"树换了/哈希换了"更强，直接动摇 §1 里
逐资产误差数字的可复现性口径。

这件事的分量比"慢一点"大：VALIDATION_REPORT §1 里"两次独立运行 tile 目录 md5
相同"的结论只在**样本=2、且都是串行轻载**时成立，累计样本=127 把它打破；而且
A-38 的污染产物**实测**就落在这条非确定性分支上——把三方逐瓦片哈希摆在一起
（`evidence/a38-forensics.txt` §8）：污染那次的 t1/t2/t3 与 `fe354714…` 那棵变体树
**逐字节相同**（sha1 分别 `bc6ae84633/434bb635cc/4e9b50cb2d`），而这两份都与基线不同；
基线的 t5 和变体的 t3 更是同一个文件（sha256 `3e0d68a79eacfa45…`，均 10 552 字节）。
也就是说"哪份几何落到哪个瓦片 id"本身随时序变化，内容哈希却没变——症状不是数值算错，
是**分配/收集次序**不确定。于是"产物不可复现"和"产物偶发坏"不再是两个假设同根，
而是同一棵树上的两个观测点：构建期存在依赖运行时时序的状态（并行滑窗里
`processTileHeavy` 与主线程 `finalizeTileFromWork` 共享源模型的数组/包围体，
或误差评估的分支被缓存状态影响），通常表现为"树换了/槽位换了"，偶尔表现为"某个顶点
槽位装着旧数据"。

**撤回一条旧结论**：§7 曾据"`WB_JOBS=1` 连跑 6 次全部与并行基线逐字节相同"写"并行度
不是变量"。本轮 `WB_JOBS=4` 直接跑出了 `fe354714…`——**与旧 §5 里"只有 `WB_PROXY_DEBUG`
第 3 跑才出现"的那个哈希是同一棵树**，把变体归因到调试开关的解释就此作废；n=6 的串行
样本太薄，本轮也没在纯串行里抓到变体（既不能证实也不能证伪串行），所以只能降级为
"**关掉并行不足以证明可复现**"。原始逐瓦片/逐配置哈希表见
`evidence/a38-forensics.txt` §5/§7/§8/§9。

**新收窄**：变体之间跳的不只是字节布局，`nodesBoundLoose`（0/1/3）与 `nodesCertified`
（1/2/3）这两个**分类结论**本身也在跳。"这个节点算宽松包围还是认证包围"依赖遍历/
完成次序，而不是只依赖几何——这把嫌疑从"回收来的内存被读走"往"误差评估的反馈环带着
运行期状态"挪了一格。
待查（按可疑度排序）：① `GeometryErrorEvaluator` 的
`budgetExhausted`→`boundLoose`→上层"是否继续细分"这条反馈环是否被别的瓦片的
状态污染（**本轮数据把它顶到第一位**：分类结论变了才会整棵树变）；
② `processTileHeavy` 的并行窗口里对**共享** `SourceMesh`
数组/`osg::Geometry` 的读写次序；③ `mergeGeometries` 缺少的长度守卫
（`ProxyMeshBuilder.cpp:361`）。
新增线索：既然同一块内容会在 t3/t5 之间换槽位，应先查**瓦片 id 分配与结果收集**
是否按完成次序而非稳定键排序（`submit` 按三角形数降序、`collect` 按瓦片 id，
但 id 本身是在细分过程中生成的）。
写入方排查进展见 A-38（本轮把"按 size 构造漏填"整类与"裸指针缓存地址回收"都排除了，
剩两条"字节快照 ≠ 元数据"候选）。这里只补一条与 A-40 直接相关的新事实：
`Osg2Gltf` 的去重缓存用 `ref_ptr` 作键（`Osg2Gltf.h:12,20-22`），所以**一次瓦片写出会
钉住它见过的每个数组**——这既是 A-38 地址回收说不成立的原因，也**可能**是 A-40 里
`WB_PROXY_DEBUG` 那次峰值 RSS 34.4→47.3 MiB 的来源（写出期内存与"这一轮钉了多少数组、
钉多久"绑定）；此条仅为待验证假设，尚无插桩数据。下一步可沿 ①②两条候选 + 瓦片 id
分配次序，配一个"每次写出前后数组存活计数"的插桩去测。
复现路径已具备：`for i in $(seq 1 30); do 转换; 比对 tile 哈希; done` 即可，
不需要真实模型——这也是把这条列成独立审计项而不是并入 A-38 的原因。

**给 A-40 清掉一个假样本（A-42 修复后补记）**：VALIDATION_REPORT §1/§1b/§1c 里"t12 的
`maxBytes` 在不同矩阵轮次之间是 510 432 / 510 440 / 510 344"这类差异**不是非确定性**，而是 GLB `name` 里那条
绝对输出路径的**字符长度**（JSON chunk 按 8 字节补齐，所以差值恒为 8 的倍数）——它是
**确定性的、可用路径长度算出来的**，与运行时时序无关。A-42 修完后同一输入换目录重跑
sha256 已经相同。**本节赖以立论的 t02（127 份产物）与 t09（8 次连跑）都是无纹理资产，
不受这条混淆项影响，A-40 的证据 base 不变**；但往后凡用"逐字节一致"当验收，
必须**固定输出目录**，否则又会把确定性差异读成噪声。

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

### A-41 i3dm 实例位姿全部丢失 + 原型几何被实例矩阵就地累乘　**FIXED（flatten 掩码 22:40 / 相对帧 23:53，阻断级）**

**症状**（全部来自盘上产物实测，不是推演）：`lugouqiao_v15` 的 126 个 `.i3dm` 瓦片，
**126/126 的特征表 POSITION 全为 (0,0,0)、SCALE_NON_UNIFORM 全为 (1,1,1)**——也就是说
每个实例都画在同一个地方，1787 个实例的实际位姿一个都没进输出。同时原型几何被放到了
源数据里不存在的位置：`i1017.i3dm` 顶点量级 1.09e7 m、`i1018.i3dm` 1.22e8 m，而同一数据集
`feature-manifest.json` 里 1787 个实例变换的**全部 16 个元素**绝对值最大只有 378（p99 平移
350.5 m）——所以这些 1e7/1e8 不是源数据带来的，是写出阶段造出来的。

**机制链**（三段代码接成一条）：
1. `ProductionPipeline.cpp:963-972`：一个实例瓦片 = `Group → N × MatrixTransform(inst.matrix) → Geode`，
   而 N 个 Geode 里放的是**同一批原型 drawable 指针**（`protoGeom` 只 `materialize` 一次，
   再 `addDrawable(child->getDrawable(d))` 逐个挂）。
2. `GltfOptimizer.cpp:549-554` `FlattenTransformsVisitor::apply(osg::Drawable&)`：
   `osgUtil::TransformAttributeFunctor tf(_currentMatrix); drawable.accept(tf);`
   ——这个 functor **就地**改写 drawable 的顶点数组；因为数组是 N 个实例共享的那一份，
   遍历到第 k 个 MatrixTransform 时，前 k−1 个矩阵已经乘在这份数组上了。
   `GltfOptimizer.cpp:563-565` 随后把每个 MatrixTransform `setMatrix(identity())`。
3. `ReaderWriterGLTF.cpp:302-345` `createFeatureI3DMTableBinary`：只能从**已被复位成单位阵**
   的 MatrixTransform 里 `decompose`，于是 POSITION=0、SCALE=1。

净结果：GLB 里的几何 = `M_N · … · M_1 · v`（N 个实例矩阵的**乘积**作用一次），
特征表 = N 个单位阵（同一位置重复画 N 次）。IL=1 的瓦片只乘一次，所以看起来是对的——
这解释了为什么"123/126 个 i3dm 瓦片量级正常"和"6 个爆到 1e4 以上"能同时成立。

**定量验证**（把机制当成可证伪的预测来测，而不是事后解释）：t05 的 299 个实例矩阵经检查
**不含旋转/缩放**（非单位元素计数 0），纯平移连乘 = 平移相加。
- 预测：manifest 里 299 个平移之和 = (273.09, **2956.73**, **−1788.77**)
- 实测：出盘 `i2.i3dm` 盒中心 (309.96, 1700.22, 2971.56) 减原型 `t0.b3dm` 盒中心
  (36.87, −88.56, 14.84) = (273.09, 1788.78, 2956.72)
两组数逐项相等，只是 Y/Z 互换并带符号——正是本管线 OSG→glTF 的 `(x, −z, y)` 轴序换算
（见 `evidence/a38-forensics.txt` §6 的轴序纠正）。到小数第 2 位吻合，机制确认。

**影响面**：`nodesBoundLoose` 与 root 包围盒/`geometricError` 被这条链污染——v15 的 root 盒
跨度 1.33e8 m、root `geometricError` 7.65e7 m（而报告字段 `tilesetGeometricError` =
`errorMaxMeters` = 8857.99，是**另一个量**，文档与对话里都别再混用）。整棵树的屏幕误差阈值
由一个 1e8 量级的根盒决定，LOD/剔除的语义已经不成立。几何本身还退化到 1e5 m 尺度的搜索灯，
属于"过 validator 但不可渲染"这一类，和 A-38 的判定口径相同。

**与 2b 闸门的关系**：A-38 的 2b 闸门第一版直接拿 `_model.bounds` 当参考系，而
`SourceModel.cpp:578-584` 在识别重复实例后把 `bounds` **重算成"剔除实例几何"的范围**，
于是任何实例数据集的正常 i3dm 瓦片都会被判越界（matrix_a38gate/t05：i2.i3dm
x=[308.96,310.96] vs 源 [35.87,37.87]±1.73 → rc=3 拒发；同一份数据在 matrix_a34 /
matrix_covfix / matrix_final 里都正常出盘 4 个瓦片）。参考系已改为"全部 `_model.objects`
aabb 的并集"（`ProductionPipeline.cpp` 2b 块）。改完后 t05 **仍然**被拒，但这次是真阳性：
`ERROR: selfValidate: tile 2 content bounds leave the source model extent on axis 1
([2970.560303, 2972.560303] vs source [-0.897531, 20.985998] +-213.144777)`——2970 m 的
内容对着合法实例最高只有 20 m 的源范围。也就是说：**闸门现在能拦住 A-41，但代价是任何带
实例的数据集在 A-41 修好之前都发不出来**。

**修复（分两段生效：flatten 掩码 exe 22:40，相对帧 exe 23:53）**，两处：
1. `ProductionPipeline::writeContent` 对实例瓦片剔掉折变换的优化位——
   `FLATTEN_TRANSFORMS` / `MERGE_TRANSFORMS` / `FLATTEN_STATIC_TRANSFORMS`
   / `FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS`（两次 `optimize` 调用都用
   掩码后的 `optBits`）。这正是老路径 `I3DMTile::optimizeNode()`（`src/3dtiles/I3DMTile.cpp:5-12`）
   一直在做、生产路径漏做的事。顺带删掉了 `(void)isInstanceTile;`——这个形参以前**完全没被
   使用**，是"A-33 只修了计数、没修位姿"的根。
2. 实例矩阵改成**相对原型**：`mt->setMatrix(inverse(protoObj->worldMatrix) * insts[k]->matrix)`，
   原型世界矩阵不可逆时显式记 failure 并跳过该原型（宁可拒发）。

**第 2 点在 22:40 二进制里是空操作（23:53 才真正生效）**：那行写的是
`pw.invert(invProto)`，而本仓 OSG 的签名是
`bool Matrixd::invert(const Matrixd& rhs)`——语义为 **`this = inverse(rhs)`**，不是
"把 `this` 的逆写进 `rhs`"。于是 `invProto` 停在默认构造的单位阵上，`pw` 反而被覆盖成
`inverse(单位阵)`。结果 `mt->setMatrix(I * instWorld)` = 实例**绝对**世界平移，而 GLB 几何
已按原型世界矩阵烘好，渲染时仍整体偏移一个原型平移（双变换没消除）。追踪实测
（临时 TRACE，已删）：`protoWorld=(-0,-0,-0) rel=(10,3,-4)`，而单测里独立
`SourceModel::build` 出来的 `objects[0].worldT=(1,2,3)` —— 同一个 `build()` 给出两个值，
差在 `invert` 把 `pw` 覆写了；原型 `aabb` 仍是 (1,2,3)..(2,3,3)，证明输入与 `build()` 都没问题。
修正为 `invProto.invert(pw)`（exe 23:53），同一处调用点全仓只有这一例（`ObbVisitor.cpp:290`
那条是注释，`main.cpp:321` 用的是 `Matrixd::inverse()` 静态版，方向正确）。

**先纠正一条我自己写错的判断**：上面"要么跳过 flatten、否则 GLB 会双重变换"里的前提是错的。
读码确认 `ReaderWriterGLTF.cpp:37-50` 的 i3dm 分支**本来就不把 `MatrixTransform` 转进 GLB**
（它只取 `getChild(0)->asGroup()` 的孩子即 Geode 去转换），所以 GLB 里没有 per-node TRS
（复验输出 `nodes` 的键只有 `children`/`mesh`）。跳过 flatten **不会**引入双变换；真正会双变换的
是第 2 点那个帧问题——几何已按原型世界矩阵烘好（`SourceModel.cpp:356`），而
`SourceInstance::matrix` 是实例自己的世界矩阵（`SourceModel.cpp:557`），不换算就会把世界
变换乘两遍（IL=1 的瓦片也错，只是错得不显眼）。

**定量复验（t05，22:40 二进制，`F:/wb_3dtiles_out/a41fix_t05`）——只证 flatten 那一半**：
`i2.i3dm` INSTANCES_LENGTH=299、**299/299 个 POSITION 非零**，其 bbox
`(-148.99,-149.72,0.10) … (149.50,149.87,19.99)` 与 manifest 里 299 个实例**绝对**平移的真实
bbox `(-148.99,0.10,-149.87) … (149.50,19.99,149.72)` 在 `(x,−z,y)` 轴序下**逐项相等**；
GLB 几何跨度仍是 2.00 m（不再被连乘）、\|v\|max=89.56；瓦片包围盒从原来的
`(309.96,1700.22,2971.56)` 变成覆盖实例云的中心 `(37.1,−88.5,24.9)` + x 半轴 150.2。
**但"逐项等于绝对平移"本身就是错的期望**：正确口径是 `inverse(原型世界) · 实例世界`，
命中绝对平移恰恰说明相对帧那半没生效（见上一条）。POSITION 非零、几何不被连乘、
GLB 无 TRS 这三件事仍然成立，flatten 掩码是有效的。

**验收工具 `scripts/i3dm_pose_check.js` 的口径本身也错了一次，现已改到相对帧**：
逐 i3dm 瓦片读特征表 POSITION（i3dm 头 32 B，`ftj@12 ftb@16 btj@20 btb@24`），与
`feature-manifest.json` 里按 `(x,−z,y)` 置换后的期望做**逐条最近邻**匹配，同时检查 GLB 节点
有无 TRS 与原型几何量级。第一版把期望取成实例的**绝对**平移——正好是被否定的那个口径，
所以它给 22:40 的错产物打了 PASS。现在：
- 期望改为 `perm(inverse(prototypeTransform) · transform)`，原型矩阵由 manifest 每条实例
  新增的 `prototypeTransform` 字段提供（schema → **v1.1**，只补溯源数据，不把求逆公式
  再抄一份到脚本侧）；缺该字段或原型奇异一律 **FAIL**，旧产物不许静默通过。
- 绝对口径仍在，但只作为诊断打印（"命中绝对平移恰恰是双变换"）。

对照实测（三次独立取证）：
- **正例**：23:53 二进制的单测产物（4 个实例、原型平移非零）相对帧 **4/4 命中、残差
  0.00**，绝对口径 **0/4** → PASS / exit 0。
- **反例 1**：`a41fix_t05`（22:40 二进制）相对帧 **0/299**、绝对 **299/299**、
  `缺 prototypeTransform 299` → **FAIL / exit 1**。
- **反例 2**：`matrix_a34/t05`（flatten 未掩码）POSITION 全 0、原型 |v| 2972.56 m →
  **FAIL / exit 1**；`lugouqiao_v15`（126 个 i3dm 全扫）1787 条 0 命中、126/126 全 0、
  原型 |v| 最大 **1.22e8 m** → **FAIL / exit 1**。
探测器既不"永远 PASS"，也不再被错误口径骗过。

**回归已沉淀进单测**（`tests/production/main.cpp` 用例 `instancePoses`，11 条断言）：
5 个数值完全相同、仅矩阵不同的 Geode 走完整发布路径，直接解析落盘 i3dm 的特征表 POSITION，
要求逐条等于 `perm(inverse(原型世界) · 实例世界)`，并同时卡住"几何被连乘搬走"
（accessor min/max 的绝对量级 ≤3）与"GLB 节点带 TRS"。原型故意也带非零平移 (1,2,3)，
否则相对帧与绝对帧数值相同、这条用例抓不到本轮的 bug。
22:40 二进制下该用例 **0/4 命中**（打印 `got (10,4,3) want (9,7,1)`，即绝对平移的 Y-up 置换），
23:53 修正后 **4/4 精确命中**；`test_production` **150/150**（含 A-42 的 9 条）。

**12 资产矩阵（权威轮 `F:/wb_3dtiles_out/matrix_a42fix`，exe 00:12 + `osgdb_gltf.dll` 00:16）**：
**12/12 rc=0、`limitViolations=0`、`path_leak_scan` 0 命中**。t05 首次以"实例瓦片 + 2b 闸门 +
**相对帧**位姿验收"三者同时通过（`instanceTileCount=1`、`instanceCount=299`、`src/occ 300/2`、
`miss/drop 0/0`、`maxTri 3588`、`maxBytes 24712`；`i3dm_pose_check` 判
**299/299 = 100 %** 命中 `inverse(原型世界)·实例世界`、最近邻残差最大 **9.47e-6 m**、
按绝对平移比对 **0/299**——绝对帧命中数必须为 0，见上一条）。
与 gate-only 那轮（`runs/matrix_a41fix`，exe 22:14）逐列对比：除 t05（那轮被 2b 闸门拒发，
本轮才出盘）外，**11 个资产的汇总列里只有一格变了**——t12 的 `maxBytes` 510440 → 510224。
这格不属于 A-41：它正是 A-42（纹理 `name` 里那条绝对输出路径被换成文件名）的直接后果。
中间轮 `matrix_a41fix2`（22:40）当时把 t12 记成 510440、t09 记成 `maxBytes 41152 / errMax 331.62`：
**t09 那次是 A-40 的第 4 个变体**（`instanceTileCount=0`，根本不进 `buildInstanceTree` 也不进
`isInstanceTile` 分支；22:40 二进制单独重跑 8 次，6 次逐字节等于 `matrix_a41fix`，
见 A-40），本轮 t09 已回到基线 41120/386.77；**t12 的历史波动则不是 A-40**，是路径长度，
详见 A-42。

**尚未验证**：修好后 v15 的 root 盒与 `geometricError` 会回落到什么量级；80 个 IL≥2 瓦片的
实例展开（A-33 口径"真实三角形数 = 实例数 × 原型三角形数"）是否仍成立——计数不受这条链影响，
但**位置**受。`lugouqiao_v16`（22:53 起）跑到一半发现用的正是"第 2 点是空操作"的 22:40 二进制，
**已主动终止**（终止时没有 `tiles/`、没有 `tileset.json`，只有 `.wb-staging/` 里 8 个半成品
b3dm 与 `textures/` 里 8 个 ktx2；进程确认已退出），改用 23:53 二进制重跑为 `v17`。
**已发布/已归档的 i3dm 资产一律作废**：`lugouqiao_v15`、`longxiang*`、`matrix_a34` 等所有带
`.i3dm` 的产物，加上 A-41 修复过程中产的两批中间品 `F:/wb_3dtiles_out/a41fix_t05`（22:41，
位姿落的是绝对平移）与 `runs/matrix_a41fix2`（22:41，同上）——它们的位姿验收是**用错口径**
判成 PASS 的，不能当证据。中间轮 `F:/wb_3dtiles_out/matrix_a41fix3`（23:53）**位姿那半是成立的**
（相对帧 299/299 PASS），但它用的仍是 22:14 的 `osgdb_gltf.dll`，瓦片里带 A-42 的绝对路径，
已由 `matrix_a42fix` 取代。

### A-42 发布产物里泄漏本地绝对路径；瓦片字节数随输出目录变化　**FIXED（exe 00:12 / 插件 00:16）**

**症状**（`scripts/path_leak_scan.js` 盘上实测）：
- `F:/wb_3dtiles_out/lugouqiao_v15`：**691 / 1057** 个 `.b3dm/.i3dm` 瓦片内含
  `"F:/wb_3dtiles_out/lugouqiao_v15\\textures\\171f531698a65c1d-basecolor.ktx2"`，
  同数据集 `feature-manifest.json` 另有 **2729** 处
  `"D:/BaiduNetdiskDownload/20240529卢沟桥分洪枢纽.fbx/…"`。
- 矩阵里 `matrix_a41fix2` / `matrix_a41fix3` 各 213 个瓦片中 1 个（t12）泄漏——**只有 1 个，
  但它是分发物**。

**机制**（两个独立口子，同一类成因：把路径当名字）：
1. `src/osgdb_gltf/Osg2Gltf.cpp:763 / :766` 把 `bufferView.name` 和 `image.name` 直接写成
   纹理的 `filename`。生产路径在出盘前已把纹理写到 `<out>/textures/`，所以 `filename` 是
   **完整输出绝对路径**，随 GLB JSON chunk 进瓦片。
2. FBX 导入器用**源文件绝对路径**命名根节点，`SourceModel.cpp:201/211` 把它拼进 `nodePath`，
   `TileFeatureMapper.cpp:96` 原样写进 `feature-manifest.json`。

**第二条伤害比泄漏本身更隐蔽、也更难查**：名字进了 GLB 的 JSON chunk，**瓦片字节数就随输出
目录字符串长度变化**（chunk 按 8 字节补齐，所以差值恒为 8 的倍数）。同一个
`test_assets/t12_big_texture.obj`、同一版代码，历史 `t0.b3dm` 尺寸：
`matrix_a34` 510432、`matrix_a38gate`/`matrix_covfix`/`matrix_a41fix`/`matrix_a41fix2` 510440、
`matrix_a41fix3` 510344、单独重跑 510304、510296。这解释了 A-41 那轮"t12 的 `maxBytes`
莫名变了一格"——**既不是 A-41 的副作用，也不是 A-40 的时序非确定性**，是目录名长短。
更要紧的是：它让"两次独立运行产物逐字节一致"这类复验**换个输出目录就自动失败**，
以前把它当噪声，等于给 A-40 掺进了一个假样本。

**修复**：两处都只留文件名（`osgDB::getSimpleFileName`；本仓 OSG 这个函数是大写 N，
写成 `getSimpleFilename` 编译不过——C2039）。①`Osg2Gltf.cpp` 纹理 name；②`SourceModel.cpp`
拼 `nodePath` 时，段名以 `/`、`\` 或 `[A-Za-z]:[/\]` 开头的取其文件名。**资产名仍留在
provenance 里**（`20240529卢沟桥分洪枢纽.fbx/园博园桥`），只是不再交付目录结构。
纹理去重逻辑（`Osg2Gltf.cpp:709` 比较的是 `existPathName == filename`，用的是内存里的
osg::Texture 文件名，不是 GLB 里的 `name`）不受影响，且全仓没有任何地方回读
`image.name`/`bufferView.name`，所以改动只在输出侧。

**验收**：
- 新工具 `scripts/path_leak_scan.js <数据集目录>`：只认 JSON 字符串字面量形态的盘符。
  **口径纠正**：第一版用裸 `[A-Za-z]:[/\\]` 扫，把 t03 的 **7 个瓦片误报**成泄漏
  （float 二进制里凑巧出现 `X:/` 字节序列）——那种命中不能当证据。收紧后：修复前
  `lugouqiao_v15` 仍报 691/1057、`matrix_a41fix2` 仍报 1/213（**不是空跑**），修复后
  `matrix_a42fix` 报 **0/213 瓦片、0/36 旁证 json**（**不是恒阳**）。
- 修复后 t12：`bufferView.name` = `6ab89426e712936c-basecolor.ktx2`，瓦片 clean。
- **路径无关性（这条才是"字节随目录变"的正解验证）**：同一输入写到
  `F:/wb_3dtiles_out/x` 与 `F:/wb_3dtiles_out/much_longer_output_directory_name_for_path_length_test/t12`
  两个目录，`t0.b3dm` sha256 前缀 **`115eef9af8c80ef63e7c2cf8` 相同**（修复前必然不同）。
- **真实 FBX 的同资产前后对照**（`longxiang.fbx`，65 MB，129 瓦片 / 127 分发文件）：
  修复前 `F:/wb_3dtiles_out/longxiang_covfix` = **81/127 瓦片泄漏**、manifest **145 处**
  `"D:/BaiduNetdiskDownload/longxiang.fbx/…"`；修复后 `F:/wb_3dtiles_out/longxiang_a42`
  = **0/127、0/3**，`rc=0` 正常出盘，nodePath 变成 `"longxiang.fbx/DTZTFT_06"`
  ——**provenance 留着、目录去掉**。同一份 FBX 顺带把 A-41 也在真实资产上过了一遍：
  6 个 i3dm 瓦片 / 10 条实例 **10/10 命中相对帧**（残差最大 7.11e-6 m）、绝对帧比对 0/10、
  TRS=0、原型 `|v|` 最大 126.55 m（v15 污染态是 1.22e8 m）。
- 回归单测 `test_production` 用例 `noLocalPaths`（+9 条断言，其中 3 条是**探测器自检**：
  正例必须命中，否则"0 泄漏"可能只是谓词写坏了恒返回 false）。
  **诚实边界**：该用例的 fixture 不带纹理，所以它只钉住 `nodePath` 那一路；纹理 `name`
  那一路由矩阵里的 t12 + `path_leak_scan.js` 钉，**没有单测保护**，将来若有人把
  `Osg2Gltf.cpp` 那两行改回去，只有跑矩阵才能发现。
- `test_production` **150/150**。

**构建陷阱（本轮踩到，写进 STATUS 第 6 条）**：`osgdb_gltf` 是 `add_library(... MODULE)`，
不在 `model23dtiles` 的依赖图里。第一次只 `--target model23dtiles test_production` 构建，
`Release/osgdb_gltf.dll` 还停在 22:14，重跑 t12 得到"泄漏没变"，几乎把已修好的代码判成没修。
插件目录共四个：`osgdb_gltf` / `osgdb_fbx` / `osgdb_ktx` / `osgdb_webp`。

---

### A-43 产物里有零长度 NORMAL，一部分落在非退化三角形上　**OPEN（判据与证据本轮补齐，定级待源侧结论）**

**现象**：官方 3D Tiles validator 会对每条这样的法线报 `is not of unit length: 0`，
但不告诉调用方那个三角形是不是退化的，所以"报了多少条"本身没有严重性信息。
`lugouqiao_v15` 实测 **195 个零长度 NORMAL 顶点，分布在 26 个瓦片**（1057 个分发文件，≈2.5 %）。
旧判据（面积 > 绝对阈值 1e-9 才算非退化）**没有判别力**：阈值在 1e-8 ↔ 1e-9 之间摆动时
"真隐患"一档在 36 与 55 之间跳，据此无法定级——这是本轮开这条审计项的直接原因。

**先把尺子修对（本轮只改测量，没动生产代码）**：
- `scripts/zero_normal_triage.js` 的**主判据**改成尺度无关的相对阈值
  `面积 > REL × 该 primitive 自身的中位面积`（默认 `--rel 1e-6`）；绝对阈值 `--eps` 降级为
  **对照列**，两档同时打印，翻转时一眼能看见。
- **阳性对照**（新增 `scripts/make_zero_norm_fixture.js`，生成一个两图元最小 b3dm：
  一个面积 100 的三角形三条法线全零 + 一个面积为 0 的退化三角形两条法线全零）：
  实测 `零法线 5 / 主判据真隐患 3 / 退化 2`，且 `--rel 1e12` → 真隐患 **0**、
  `--eps 1e12` → 对照档 **0**（两判据互相独立、随阈值单调）。
  **没有这个对照之前，"0 条"完全可能只是判据写坏了恒返回 false。**
- 反向确认：矩阵 12 个资产 + `longxiang_a42` 全部 **0 条**——合成资产不产生这个形态。

**实测分档**（`lugouqiao_v15` 全量 1057 文件，扫一遍 2.1 s）：

| 口径 | 结果 |
|---|---|
| 按三角形面积（主判据） | 退化 **140** / 非退化 **55**（这 55 条涉及 **19 个**文件；注意口径：26 = 含任何零法线的文件，19 = 含"真隐患"零法线的文件。旧记录写的"19 个文件"未回溯确认按哪一档）；绝对阈值 `--eps 1e-9` 档给出**同样的 140/55**（这份资产的尺度刚好让 1e-9 够用） |
| 按瓦片类别（关联 `conversion-report.json`） | 代理瓦片 **134** / `EXACT_LEAF` 叶瓦片 **61** / 聚合代理 **24** |
| 顶点签名 | `mesh0 v24/v25/v26` 在 **6 个**瓦片里以同样下标重复出现（同一份原型几何被多个 LOD 复用） |

**证据**：`docs/model23dtiles-production/evidence/zero_normals-lugouqiao_v15.txt`
（顶部就是复现命令，内含阳性对照、阈值单调性、v15 全量分档与逐文件类别）。
类别关联用新脚本 `scripts/zero_normal_tile_class.js <数据集目录>`，
它把含零法线的瓦片与 `conversion-report.json` 的 `isProxy`/`isLeaf`/`aggregate`/`errorStatus` 对上。

**已排除**：
- **不是简化器独有的**：`t162`/`t166`/`t699`/`t716` 是 `EXACT_LEAF`（未经简化的源几何拷贝），
  各自 3 条零法线**全部落在非退化三角形上**，简化路径根本没碰过它们。
- **不像 A-38 那一族**：A-38 的形态是"槽里装着别的数据、值随分配器状态变"；这里是干净的 0，
  并且**跨瓦片、跨 LOD 完全确定**（同一 `v24/v25/v26` 签名在 6 个文件里逐字对得上）。

**待定级的原因（不是偷懒）**：剩下两条候选——① FBX 导入器（smoothing group 本来就给 0 法线，
管线只是忠实透传）；② OSG→glTF 的 NORMAL 写出通路（`Osg2Gltf`/`GltfOptimizer`，
属 A-38/A-39 排查过的同一带）。判别需要能读到**源侧**法线的口子，而两条都要动构建，
`lugouqiao_v17` 正在跑（构建会撞 post-build 拷贝，MSB3073），本轮不做。实验设计：
① 加 `WB_DUMP_SOURCE` 把加载后的源 POSITION/NORMAL/索引落盘，逐条对上这 55 个顶点；
② 或在 v17 上复扫——若仍是 195 条、同样的 `v24/v25/v26` 签名，再排除一次随机性。

**处置倾向（未动手，理由先记）**：源侧带的 0 法线**不该触发拒绝发布**——数据可渲染，
只是那几面光照异常，且 validator 只会逐条告警；但也不该原样交付。候选修法是在写出时把
零长度法线按**邻接非退化三角形的面积加权平均**重算。要先拿到 ① 的结论，才决定改写出侧
还是导入侧，否则可能把源数据的锅修在错误的层上。
