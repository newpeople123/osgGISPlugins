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
| A-42 | 发布产物里泄漏**本地绝对路径**，且瓦片字节数随输出目录长短变化。两个口子都是"路径当名字"：`Osg2Gltf.cpp:763/766` 把纹理的完整输出路径写进 GLB `bufferView.name`/`image.name`（随 `.b3dm/.i3dm` **分发出去**：lugouqiao_v15 实测 **691/1057** 个瓦片带 `F:/wb_3dtiles_out/lugouqiao_v15\textures\…`）；FBX 导入器用源文件绝对路径命名根节点，顺着 `nodePath` 进 `feature-manifest.json`（同数据集 **2729** 处 `D:/BaiduNetdiskDownload/…fbx/…`）。副作用更难查：name 进 GLB JSON chunk → 同一输入、同一代码，t12 的 `t0.b3dm` 在不同输出目录下出现过 510432/510440/510344/510304/510296 五个尺寸，**以前把这类差异算成 A-40 的噪声** | `Osg2Gltf.cpp::getTexture` + `SourceModel.cpp::SourceModelVisitor::apply(Node&)` | FIXED（exe 00:12 + 插件 00:16：两处只留文件名；`path_leak_scan.js` 复扫 `matrix_a42fix` **0/213 瓦片、0/36 json**；两个长度差 40 字符的目录产出 sha256 相同；`test_production` 150/150。**`osgdb_gltf` 是 MODULE 插件，按目标构建不会重建它**。v17 复验：结构判据 `path_leak_structural.js` 报 **1057 瓦片 CLEAN / rc=0**、旁证绝对路径 **2729→0**；`path_leak_scan.js` 那 4 处是 BIN chunk 噪声，不是泄漏，该工具降级为粗筛） | —（手册未列：交付物含本地路径） | 中（分发物带操作者目录结构；并污染"逐字节可复现"这一验收口径） |
| A-43 | 产物里有零长度 NORMAL：`lugouqiao_v15` 实测 **195 个顶点 / 26 个瓦片**，其中 **55 个落在非退化三角形上**（法线本该算得出）。旧判据用绝对面积阈值，1e-8↔1e-9 之间"真隐患"档在 36↔55 之间跳，无法定级 | **源数据本身**（非产品代码）；产物侧为忠实透传 `SourceModel.cpp:805-807`、写出闸 `:856` | 源侧定性 **CONFIRMED**，写出侧修复**已在 `49b6d3c` 实施，并于 2026-10-20 下午**产物级复验通过**（`lugouqiao_v19`：validator 叶层 ERROR **0** / `rc=0`；同轮对 v17 复跑仍是 195 条 / `rc=1` 作阳性对照；`zero_normal_triage.js` v17=195 → v19=0）**：裸读源 FBX 实测 2729 个法线数组（解析失败 0）、按内容去重 626 个，其中 **7 个含零法线**（另有 29 个逐字节相同的 instance 拷贝，故原始计数是 36 个数组）；零角点 **63** 个 = **21 个整面全零三角形**（按 PVI 真实负下标边界算，不是"3 角点/面"假设），散落单角点 **0**。尺子本轮已修对（主判据 `REL×中位面积`、绝对阈值降为对照列、`make_zero_norm_fixture.js` 阳性对照、非单位列做解析自检）。已排除：简化器（`EXACT_LEAF` 瓦片带同样 3 条）、写出侧造零（`SourceModel` 只能原样带上或整条丢掉）、平滑（`-nrm` 未开，`SmoothingVisitor` 不参与）、A-38 同族（确定、可逐字节复现）。证据 `evidence/a43-source-side-fbx-probe.txt`，含源↔产物两条独立读路径在同一个 4 818 面 mesh 上对上"面数 + 3 条零" | —（手册未列：非单位法线） | 低（**成因**：输入数据缺陷 + 忠实透传，可渲染、局部 shading 异常）。**发布等级本轮上调**：2026-10-20 官方 validator 对 `lugouqiao_v17` 判 **rc=1 / 26 个内容文件 / 叶层 195 条 `not of unit length: 0` ERROR**（同工具对 `matrix_a42fix` 12/12 与 `longxiang_a42` 判 0 error），旧写法"validator 只逐条告警、不按拒绝发布处理"已撤回——写出侧修复已于 `49b6d3c` 落地（`SourceModel::repairZeroNormals`，v17 分档实测 55 可由入射面修复 / 140 退本图元平均面法线 / 0 无解），v17 与已终止的 v18 都不是这份代码出的盘，**`lugouqiao_v19` 才是**（2026-10-20 下午，ERROR 0 / rc=0）； **v19 的视觉验收仍未做**，电池与 validator 全绿不等于可发布；禁止用丢属性或降级判据凑通过 |
| A-51 | A-48 保住代理纹理之后，内节点瓦片 glTF primitive 冲破手册 §3 的 ≤24（实测 6 张：`t0`=27/`t1`=26/`t21`=28/`t29`=25/`t79`=32/`t100`=28，修前基线最大 11），代理侧 `prims == materials` 严格 1:1、材质合并**零收益**；机制是图集路径被 `GltfOptimizer.cpp:1089-1099` 的 UV>1 判据整体挡掉（本资产 sampler 是 `REPEAT`，UV 越界合法），而 `maxPrimitivesPerTile`/`maxMaterialsPerTile` 只被 `SpatialPartitioner.cpp:16-17` 消费，**代理路径与 `selfValidate` 都没有这道闸** | `src/utils/GltfOptimizer.cpp:1089-1099`、`src/3dtiles/production/SpatialPartitioner.cpp:16-17` | **方案 A 已实施 + 产物级实测（2026-10-21）**：代理 prims 32/12.7 → **24/9.8**、`prims>24` **6 → 0**、纹理保住（代理无纹理仍 2）；`selfValidate` 的 primitive/材质闸已补。三条未决归用户：①`-dc` 默认 20 会把折叠后的 longxiang 判死（代理 max=24 > 20 → rc=3）；②折叠态跨瓦片外泄已用作用域守卫修掉（守卫**为瓦片隔离而保留**；先前记在它头上的"写出 2.4× CPU"已用同时段无守卫对照**撤回**，真实代价未测，见 §A-51 落地第 5 条）；③近景 + 发布资产视觉验收未做 | 3 / 7.3 / 17 | 高（阻塞 A-48 的产物级验收：`longxiang_final512` 不是可发布产物） |
| A-52 | 同一二进制、同一参数连跑两遍，产物不逐位相同。首观测 `longxiang_fold512` vs `longxiang_fold512_b`（都 `-dc 24 -ptw 512`）差 5 组图集页 + 3 个**根本没走折叠**的叶瓦片字节（`t40`/`t78`/`t119`）；带守卫的 `guard512` vs `guard512_b` 同样。**原归因（`processTextureImages` 按 `unordered_map<osg::Geometry*,…>` 的指针哈希桶序装配图集）已被实测推翻**，见下"实测机理" | `src/utils/TexturePacker.cpp:100`（画布分配） | **已修，串行 + 默认并发两档均复证通过**：`lx_z_a`/`lx_z_b`（串行）与 `lx_p_a`/`lx_p_b`（并发）各 **246/246 逐字节相同、trace 差 0 行**；阳性对照用同一把尺子跑修前那对（`lx_tr2_a`/`lx_tr2_b`）仍判 **FAIL**，⇒ PASS 不是尺子失灵。机理见下方"实测机理"。**回归测试已补**：`atlasCanvas`（`tests/production/main.cpp`，14 条断言）—— 负对照实测：去掉 `TexturePacker.cpp:114` 的清零重编 → `dirty=600/3600`，3/3 次 rc=1；装回 → 14/0，3/3 次 rc=0。第一版（只污染 1 MiB 堆）在无清零时仍 10/0 通过，是空过，已重做 | 17 | 中（不影响单份产物的自洽性，影响跨运行可复现性与回归对照方法） |
| A-53 | `test_production.exe` **间歇性堆损坏**：进程在 `[partition] giant single mesh must split` 处硬死，bash 侧只看到 rc=127。Windows 事件日志 Id 1000 记故障模块 `ntdll.dll`、异常码 **`0xc0000374` = STATUS_HEAP_CORRUPTION**，随后 Id 1001 出现 `FaultTolerantHeap`（系统在反复崩溃后启用的兜底）。单跑该用例（filter `giant`）也能死 ⇒ 与前面的套件无关 | `tests/production/main.cpp:353`（用例入口），真正越界点**未知** | **OPEN（未归因）**。旧二进制（18:01，不含 A-52 清零）：18:01 全量 **263/0**、19:03 全量 **rc=127**（当时两遍 trace 生产运行在跑）、19:30 单跑 **rc=127**、几分钟后单跑 **rc=0**。新二进制（19:37，含清零）：全量 **263/0 ×2**（rc 直接取）、单跑 `giant` **8/8 通过**。再加一轮（20:0x，含清零 + `atlasCanvas` 的二进制，无并发生产负载、串行单进程）：单跑 `giant` **20/20 通过**、全量 **277/0 ×2**。两侧样本都小，**不能声称已消失**；两次崩溃观测都有并发因素（重负载生产运行 / 同 CWD 里第二个 test 进程抢写 `test_production_progress.txt`）。资源已排除：物理可用 71 GB、提交 108/150 GB、C/E/F 盘余 288/124/3693 GB。复现时下一步：Application Verifier 页堆或 WinDbg 抓现场 | 12 / 17 | 中（不碰产物，碰的是回归结论的可信度） |
| A-54 | 发布资产 `lugouqiao`（335 MB）在**当前二进制**（A-51 折叠 + A-48 代理纹理 + A-52 清零，默认 `-dc 24` / `-ptw 512`）下被 primitive 预算闸拒绝：`selfValidate: content over primitive budget: tiles/t8.b3dm (26 primitives > 24)`，`rc≠0`、产物**未提交**（`discardStaging()` 删掉 `tiles/`，`F:/wb_3dtiles_out/lugouqiao_v20/textures/` 只剩 939 张暂存图集页）。stderr 只有这一条 ERROR ⇒ 越界瓦片集合 = {`t8`} | `src/3dtiles/production/ProductionPipeline.cpp:1952-1958`（闸门本身按手册正确开火）；越界成因在代理写出侧：A-48 保住贴图后宿主数上升，A-51 折叠未能把 `t8` 压进 24 | **已裁定（2026-10-22，走 ③「明确失败」）：上界一条都不动，该资产当前不可发布**。对照读数：同资产 `lugouqiao_v19`（10-20 二进制，A-48/A-51 之前）**1057 个瓦片 max prims/tile = 11、0 违例** ⇒ 这 26 是"保纹理"这一侧换来的。三条出路：①显式抬 `-dc`（= 放宽手册 §3 上界，需批准）；②对 `t8` 这类瓦片继续按预算降代理纹理分辨率/合并宿主压到 24（牺牲远景清晰度）；③按 §7.3 走明确失败，该资产当前不可发布。诊断跑 `lugouqiao_v20prims`（`-dc 100`，20:42→21:06，**仅测量、不作验收产物**）换闸不掉：**`content over material budget: tiles/t8.b3dm (26 materials > 24)`，rc=3** ⇒ ①`prims == mats == 26`（1:1，折叠合不掉）、②两次都只有一行 ERROR ⇒ 越界集合 = {t8}、③**抬 `-dc` 单独不够**：`maxMaterialsPerTile` 恒 24 且 CLI 无开关（`ProductionProfile.h:34`、`main.cpp:661-679`）。口径限制：`main.cpp:666-668` 连 `targetPrimitivesPerTile` 一起改，而 `SpatialPartitioner.cpp:16-17` 消费两者 ⇒ 诊断跑的切分与出货配置不同，两个"t8"不是同一块内容。对照 `v19` 的 `t53` = 11 prims/11 mats/10 texs、`t8` = 1 prim/0 material（解析器 `E:/tmp/glb_mats.js`，阳性对照 = `glb_prims.js` 报的 t53=11 两脚本一致）。**裁定原文（2026-10-22）：走 ③「明确失败」** —— 上界一条都不动（`-dc` 默认 24、材质闸恒 24 都不改），也不再用纹理预算侧杠杆去凑通过；`lugouqiao` 这档资产**按当前判据即不可发布**，验收链就此停止（不出盘、不再跑 validator、不做该资产视觉验收），先把 `longxiang` 档的验收做完（已完成，见 §8 的 2026-10-22 复测段）。以后若要把这档资产救回来，需要的是**重新裁定判据**，不是再跑一次 | 3 / 7.3 / 17 | 高（阻塞该资产发布；对 `longxiang` 档不阻塞）|
| A-57 | **Q2（A-56）那道读模后索引守卫拦不住 OBJ 输入**：`osgdb_obj` 不交付文件里的 `f` 索引，而是按引用把 `v` 坐标逐个拷进几何自己的坐标数组，"文件里写了 N+1"因此被**吸收成本地 0..N-1 的合法下标**——守卫结构性失明。指纹是产物顶点数 **>** 源 `v` 数，多出来的那颗是源 float 流的错位窗口；值有限，所以非有限丢弃与 `selfValidate` 一路放行 | **新增** `include/3dtiles/production/ObjIntegrityPrecheck.h` + `src/3dtiles/production/ObjIntegrityPrecheck.cpp`（读模前的原文扫描）；接入点 `tools/model23dtiles/main.cpp:501-509`（`-objcheck reject\|warn\|off`，档位非法 rc=4）、`:574-606`（rc=7 硬拒，落在建目录之前 ⇒ 天然零产出）、`ProductionPipeline.h:150-152`（`Options::objPrecheck`）、`ProductionPipeline.cpp:1483-1522`（干净也要落计数） | **FIXED（未提交，2026-10-28）**：`objPrecheck` 格式专项单元测试 14 例 / **55 条断言全过**，套件 **383 / 0**；**尺子有效性用变异对照钉住**（把 `beyond-total` 判据短路 → 同一套断言 **12 FAIL**、rc=1，改回后与改动前逐字节相同）；CLI 三档实测：reject（默认/显式）rc=7 且目标目录只剩 `run.log`、warn rc=0 且报告有 5 个 `objPrecheck*` 计数 + 2 条 warning、off rc=0 且**无任何 obj_precheck 痕迹**；同二进制同时段 `-objcheck off` / 默认 reject 成对矩阵 **12/12 rc 一致、`outputBytes` 逐对相同**；官方 validator 对 reject 档 12 套产物 + `c2_naruto` 共 13 份报告：**叶层 `severity=ERROR` 0 条 / WARNING 8 条**（首跑的紧凑 grep 是死尺子，已用阳性对照重测，见小节内"尺子失效与重测"）。详见下方 A-57 小节 | 5.3 / 4.1 / 14.17 | 高（输入缺陷被上游洗白，且污染包围盒 / 质心 / 误差评估） |
| A-58 | `ConversionReport` 的 **7 格身份/资源字段**（`inputFile`/`outputDir`/`inputBytes`/`outputBytes`/`processWallSeconds`/`processCpuSeconds`/`peakRssBytes`）以前**从不赋值**，产物里恒为 `""`/`0` —— 而 `0` 看起来像"量出来是 0"。投影（`-epsg`）构建失败还走 `exit(0)`，CI 与回归脚本一律读成成功 | `ProductionPipeline.cpp:1401-1420`（身份格 + 不可用如实标注）、`:2490-2500`（`outputBytes` 口径固定：自校验通过后、写报告前对已提交产物递归求和，含报告自身之外的内容文件）、`tools/model23dtiles/main.cpp` 投影失败分支 → **rc=6** | **FIXED（未提交，2026-10-28 实测）**：矩阵 12/12 的 `inputBytes` 等于 `stat` 值（如 t03 `9 429 501`）、`outputBytes`/`processWallSeconds`/`processCpuSeconds`/`peakRssBytes` 全部非零非空；`-epsg 999999` → **rc=6** 且输出目录**只有本人建的 `run.log`**（工具未落一个文件）。**注**：A-58 首跑时 `inputBytes` 在真实资产上仍是 0，真因是 A-59（不是 A-58 的赋值漏了），见下一行 | 7 / 14.17 | 中（报告不可信 + 失败被读成成功） |
| A-59 | **UTF-8 路径 vs `std::filesystem` 的 ANSI 解码**：`main.cpp:551-555` 在 Release 下把 `-i`/`-o` 显式转成 UTF-8，而 `std::filesystem` / `ofstream` / `ifstream` 的**窄字符入口按 C locale 代码页解码同一个字符串**，两条路解出**不同**的路径。模型本身读得动（osgDB 走宽字符），所以症状是"只跟文件系统有关的字段/操作失败"：`inputBytes` 恒 0 + `cannot stat` 警告（真实资产全中，本轮审计的资产**无一不在中文目录下**） | **新增** `Utils::utf8ToPath()`（`Utils.h:302` / `Utils.cpp::utf8ToPath`，Windows 走 `osgDB::convertUTF8toUTF16`，POSIX 走 `u8path`）；改站点 `ProductionPipeline.cpp` **14 处**（`grep -c utf8ToPath` 实测：`:622`/`:660`/`:2015`/`:2049` `ifstream`、`:1029`/`:1325` `remove`、`:1414` `file_size`、`:1813`/`:1846`/`:1887` `ofstream`、`:2453`/`:2526` `path` 构造、`:2500` `recursive_directory_iterator`、`:2536` `directory_iterator`）+ `main.cpp:811-814`/`:835-838`（纹理缓存目录清理，每块 3 个站点）+ `ObjIntegrityPrecheck.cpp:468` | **FIXED（未提交，2026-10-28 实测）**：真实 FBX 同参数复跑 `c1_naruto_a58` → `c2_naruto_a59`，`inputBytes` **0 → 29 205 344**（与外部 `stat` 逐项相等）、`cannot stat` 警告 **1 → 0 条**、`outputBytes 190 915` 与 `maxContentBytes 188 832` **不变**、4 个产物文件 **sha256 逐字节相同**（改路径解析不动几何）；中文**输入+输出**双端 CJK 目录端到端 rc=0（`inputBytes`/`outputBytes`/报告写出全通）。单测第 13 例把这条钉死（CJK 临时目录 + `bytes == file_size`）。**未证实**：失败路径上的 `remove()` / 空纹理目录清理两处未被 CJK 端到端触达（三次强行失败尝试 `-tri 1`/`-tri 2`/`-dc 1` 全部 rc=0）；**Debug 构建不做 UTF-8 转换**（`#ifndef NDEBUG`），本修复在 Debug 下不生效 | 7 / 12 | 高（真实资产全在中文目录；报告字段静默为 0） |

> **汇总表滞后**：本表有 **A-01 ~ A-43 + A-51 ~ A-54 + A-57 ~ A-59**。缺行的是
> **A-44 ~ A-50**（有独立小节，`grep '^### A-'` 为准）与 **A-55 / A-56 / Q1 / Q2**
> —— 后四条的判据在提交 `8abd48c` 与 `F:\wb_stage0_ffbb3fe\20261026T092706\A56_ROOT_CAUSE.md`，
> 本轮没有逐条复核，不代它们写结论。补表时别只补几条就宣称表是完整的。

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
> VALIDATION_REPORT §7），**但这条"14/14 零 error"只对那批产物成立**：2026-10-20 同一工具
> 对 `lugouqiao_v17` 判 **rc=1 / 26 个内容文件 / 叶层 195 条 ERROR**（全为 A-43 零长度 NORMAL），
> 对 `matrix_a42fix` 12/12 与 `longxiang_a42` 仍是 0 error，见 VALIDATION_REPORT §7 的复跑段。
> 但它**不核对每瓦片 glTF primitive 数**，所以 A-32 段里
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

**导入器侧那条候选机制现在单独存档，并且已按源文件实测缩小了范围**：
`evidence/fbx_reader_candidate-mechanism.txt` 记的是 `fbxRMesh.cpp::getVertexIndex:74-102`
（不支持的 mapping mode 只 `OSG_WARN` 一句就带 `index = 0` 继续）与 `getElement:124-131`
（`GetDirectArray().GetAt(...)` 不校验上界，而 FBX SDK 头 `fbxarray.h:181-202` 明写越界不抛错）
——这是"值随分配器状态变"的**性质来源**，但**仍不是 A-38 的原因**。已实测缩小的部分：
本文件含零法线的那两个数组属性头是 `ByPolygonVertex` + `Direct`，而该组合在 `getVertexIndex`
里直接返回 `nMeshVertex`，**两个可疑分支都不会走到**（顺带把 A-43 与这条机制分开）；
文件里另有一批 `IndexToDirect` 形态的 layer element（UV/材质类）确实会走
`GetIndexArray().GetAt()` 那条不校验的路。没做的判别（含三步廉价方案）写在那份文件 §3。
**在这些被实测之前，不要给 `default` 分支加"显式失败"**——那会把真正的成因掩盖掉。

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

**65 MB 真实 FBX 上的第三个样本（2026-10-21，`longxiang`）**：同一功能等价二进制、
同参数 `-ptw 512`、各自全新输出目录的两跑
（`longxiang_final512` vs `longxiang_l0b`，后者只多了 A-47 的落盘仪表）：

- **8/129 个瓦片 `contentBytes` 不同**：叶 3 个（`t40`/`t78`/`t119`，正是以前认定
  "与同参数对照对完全一样"的那三张）+ 代理 5 个（`t25`/`t43`/`t62`/`t105`/`t106`，
  单张最大 −6 928 B）。**每瓦片 `triangles` 全同、五个误差字段 129/129 全同、
  结构计数与 `errorMax/p95/mean` 逐值相同**，总字节只差 −9 920 B（0.01 %）。
  ⇒ 噪声地板不是"3 张叶"，65 MB 档实测**最多 8 张**，且代理瓦片也会动
  （代理的字节主要来自图集/KTX2 组合，`tbb::parallel_for` + 按文件名/哈希去重的
  完成次序都是嫌疑；本轮未定位）。
- **耗时也跟着动**：同输入同参数，wall 119.16 → 188.28 s、进程 CPU 秒 372.6 → 542.9 s，
  而 `micro_evaluate_samples` 两跑**完全相同**（48 963 414）。也就是说变体不只是
  "谁落到哪个瓦片"，**同样的样本量可以花掉 1.46 倍的 CPU 秒**——
  这条对本条的老问题（"变的是分类/分配次序"）是新的支撑，
  对"计时口径不可跨会话比"是新的量化。
- **对验收闸的直接影响**：`report_tile_diff.js` 在这种纯观测改动下判 **FAIL
  （contentBytes=8）**，是**真噪声不是回归**。所以 PERFORMANCE §2.2 那句
  "每根杠杆 A/B 要同一杠杆重复 ≥3 次"在 65 MB 档同样成立，
  而且**代理瓦片也要纳入对照**（以前只盯叶）。

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

**真实 FBX 复验（v17，2026-10-20）**：`path_leak_scan.js` 在 v17 上报 **rc=1 / 4 处**
（`t669`/`t671`/`t688`/`t840`），但那个工具**没有结构**，二进制 payload 里的字节噪声和真泄漏
同号——这 4 处全部落在 GLB 的 BIN chunk，是噪声。新工具 `scripts/path_leak_structural.js`
给出可判定的口径：按 b3dm/i3dm/pnts/cmpt 头切出 feature table、batch table、GLB 各 chunk，
逐个 `JSON.parse` 后走字符串，只有 JSON 区里的绝对路径才算泄漏；判据覆盖盘符、前导 `/` 或
`\`（含 UNC）、`file://` 三种形态（旧口径只认盘符，会漏后两种）。
实测：**v17 瓦片 1057 / 旁证 3 → CLEAN，rc=0**；四个裸命中按区分布 `glbBIN_1=4`、
进 JSON 区 0 处、padding 残留 0、**尺子失效 0**。旁证侧同一字段直接对照：
`features/0/nodePath` 从 `"D:/BaiduNetdiskDownload/20240529卢沟桥分洪枢纽.fbx/园博园桥"`
变成 `"20240529卢沟桥分洪枢纽.fbx/园博园桥"`，schema `v1 → v1.1`，旁证含绝对路径
**2729 → 0**（`instances=1787`/`occurrenceCount=4250` 两版相同）。

**这把尺子自己先错过三次，都记下来**：① GLB chunk 头起点写成 `base+20`（正确 `base+12`），
导致 `glbJSON` 区根本不存在、所有命中归进一个叫 `glbChunk_73736563`（"cess"）的假区，
当时照样打印 CLEAN；② 漏了 i3dm 偏移 28 的 4 字节 `gltfFormat`，使 i3dm 所有区错位 4 字节、
GLB 段整个没被查——**v15 那类 i3dm 真泄漏会被它整条放过去**；③ cmpt 偏移 12 本仓实测写的是
**内层块数**（`cmpt_probe`：2）而不是规范的字节总长，且内层从偏移 **16** 开始，按字节总长解释
得到 `lim=2`、递归一次都没进。三个都是"恒阴但看着干净"，全靠对照才暴露：
已知阳性 `lugouqiao_v15` → **LEAK 3112 处**（能报出 `glbJSON_0/images/0/name` 这种 JSON 指针）；
注入对照 `scripts/make_path_leak_fixture.js` 两种底本（最小合成 `zn_fixture.b3dm` 与真实
`lugouqiao_v17/tiles/t669.b3dm`）各一对，注盘符的那份 rc=1、只注相对 uri 的那份 rc=0——
触发的是盘符，不是"多了一个键"。退出码 **3 = 尺子失效**，取到 3 时那个 0 不算数。
**本轮又拿到第二个真实 FBX 阳性**：`wuhu_v1`（另一份输入 `芜湖水厂总装.fbx`）报
**rc=1 / 瓦片 JSON 区 226 处 + 旁证 87 处**，且命中里明确出现 **`.i3dm`** 文件
（`tiles/i56.i3dm glbJSON_0/bufferViews/0/name`、`images/0/name`）——上面 ② 那个
"i3dm 偏移少 4 字节 → i3dm 整段没被查"的坑如果在，这一类就会被读成 CLEAN，
所以这份产物同时是 **i3dm 分支的阳性对照**（此前只有 `lugouqiao_v15` 的 3112 处撑着）。
证据 `evidence/real-fbx-status-2026-10-20.txt` §2。
cmpt 偏移 12 与规范的偏差目前只有 1 份样本（其余数据集 `.cmpt` 计数实测为 0），
消费侧没查，暂不单开审计项。证据：`evidence/a42-path-leak-v17-adjudication.txt`。

**验收口径改用哪把尺子**：`path_leak_structural.js` 的退出码进验收（0/1/3 语义明确），
`path_leak_scan.js` 降级为粗筛探针——它报命中只说明"该人工看了"，不构成交付结论。

---

### A-43 产物里有零长度 NORMAL，一部分落在非退化三角形上　**源侧定性 CONFIRMED；写出侧修复 `49b6d3c` + 产物级复验通过（`lugouqiao_v19`，2026-10-20 下午）**

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
- **不是平滑/重算造成的**：`recomputeNormals` 走 `osgUtil::SmoothingVisitor`
  （`tools/model23dtiles/main.cpp:164-171`），只在 `-nrm` 下调用（`main.cpp:600-611`），
  v15/v17 命令行都没有 `-nrm`。
- **不是写出侧造的**：`SourceModel.cpp:805-807` 是**逐顶点条件 push**、`:856` 在尺寸不匹配时
  把**整条**法线数组丢掉——这两条路径要么原样带上、要么整条没有，造不出干净的 0。

**源侧结论（本轮把那个岔口判掉了，不需要动构建）**：原本记的是"两条候选都要动构建才能判，
计划加 `WB_DUMP_SOURCE`"——其实源 FBX 是二进制文件，法线数组按属性名就能直接读。
新增 `scripts/fbx_source_normals_probe.js`（裸读 + 按 `encoding==1` 解压 zlib +
按同 mesh 的 `PolygonVertexIndex` 真实负下标边界归面）实测
`20240529卢沟桥分洪枢纽.fbx`（335 435 216 B）：**2729 个法线数组、解析失败 0**，
按内容去重 626 个，其中 **7 个含零法线**（原始命中 36 个数组，多出的 29 个是逐字节相同的
instance 拷贝——不去重会把"7 个 mesh"说成"36 个 mesh"，那是夸大）；零角点 **63** 个，
**全部**是**整面全零**：涉及 21 个多边形、21 个整面、散落单角点 **0**、
每个零面恰好贡献 1 个"多边形末角点"，21×3 = 63 = 零角点数 = 零角点唯一顶点数
（⇒ 21 个面都是三角形，且面间不共享顶点）。同文件另有 16 675 068 个**非零**向量，
"不在 1±1 % 单位长度"的条数 = **0**。

三重自检（结构 / 统计 / 配对）与源↔产物对应都在
`docs/model23dtiles-production/evidence/a43-source-side-fbx-probe.txt`，最关键的一条是
两条**完全不相干**的读路径在同一个 mesh 上撞出同一组数：源侧某个法线数组 14 454 角点
（= **4 818** 个三角形，1 个整面全零）↔ 产物侧 `i952`/`t162`/`t166` 三个瓦片的
`#mesh0` 各自"该图元三角形 = 4 818、零法线顶点 = 3"。产物其余瓦片条数对不上是**预期**
（瓦片按空间切分几何、同一原型被多个 LOD 复用），不是反例；能证伪这条对应的观察是
"某含零法线的瓦片来自一个源里没有零法线的 mesh"，那需要 mesh 级源↔产物 id 映射，未做——
所以本条定的是**成因层面**（源里确实有、且是整面形态），不是**逐顶点归因**。

**成因定级：低（源侧 authored 的数据缺陷 + 我们忠实透传，不是产品缺陷）**；
**但"不触发拒绝发布"这半句在 2026-10-20 被实测推翻，已撤回**。当时的推断是
"validator 只会逐条告警"，实际跑下来官方 `3d-tiles-validator@0.6.1` 对 `lugouqiao_v17`
判的是 **severity=ERROR**：**rc=1、26 个内容文件、叶层 195 条**
`Vector3 at accessor indices N..N+2 is not of unit length: 0`，且这批 ERROR 之外
**没有其他类 ERROR**（叶层 ERROR 195 条 100 % 同签名）。同一工具对 `matrix_a42fix`
12 个资产和 `longxiang_a42` 判 **0 error**，所以不是校验器对整条管线的系统性误报。
后果：**携带这些顶点的产物过不了官方发布闸**，v17 在写出侧修复落地并重跑之前不可发布；
手册的"宁可拒绝发布"在这里成立的不是"数据不可渲染"，而是**官方口径把非单位法线判为 ERROR**。
处置方向不变：三处都是**整面三条角点法线同时为 0**，所以邻接平均没有可用的非零邻居，
应当把这类零法线**重算为其所在非退化三角形的面法线**（顶点位置现成，不需要邻接图）。
**不允许**用"整条丢掉 NORMAL 属性""剔除含零法线的图元""把 validator 的 ERROR 记成 warning"
来凑通过——那属于手册禁止的放宽验证。
动手需要构建位——`lugouqiao_v17` 当时正在跑，并行构建会撞 post-build `z-applocal` 拷贝（MSB3073）。

**写出侧修复已于 `49b6d3c` 落地（代码级证据；产物级复验未做）**：
`SourceModel::repairZeroNormals(pos, nrm, tris)` 在 `materialize()` 装好 `setNormalArray`
之前调用（`src/3dtiles/production/SourceModel.cpp` 物化出口），b3dm 叶子、i3dm 原型、
代理聚合三条内容通路都经过这里（`ProductionPipeline.cpp:612`/`:964`、
`ProxyMeshBuilder.cpp:434`/`:626`/`:675` 全部以 `materialize` 为源，实测 grep 得到）。
规则按本轮新测的分档定（`scripts/zero_normal_repair_plan.js` 对 v17 全量 1057 个内容文件）：

| 档 | 判据 | v17 实测 | 处置 |
|---|---|---|---|
| A | 顶点自身有入射非退化面（面积 > 1e-6 × 本批中位面积） | **55** | 入射面法线按面积加权平均 |
| B | 自身入射面全退化，但同图元存在非退化面 | **140** | 退到本图元面积加权平均面法线 |
| C | 整个图元无非退化面 | **0** | 保持零 + `OSG_WARN`，让 validator 继续拒 |

读数存 `F:/wb_3dtiles_out/v17_checks/zero_normal_repair_plan.txt`。**C 档为 0 是关键**：
它说明修复不需要编造方向，前面"三处都是整面全零、邻接平均没有可用非零邻居"的预判
只说对了形态、没说中后果——那 140 个顶点的图元里另有有效面可兜底。
其余约束：**可用法线一律不动**（判据是长度 > 1e-6 且三分量有限）——非单位可用法线在
进入 `SourceModel` 时已由 `SourceModel.cpp:392-399` 的 `normalize()` 归一、写出器
`Osg2Gltf.cpp:158-162` 还会再归一一次，本轮没有改动这两处，也不把"顺带归一"混进这个修复；退化尺子与 `zero_normal_triage.js` 主判据同源，绝对阈值不再参与判定。
`test_production` 新增 `nrmRepair` **17 条**（含"全退化不编造"与"非单位法线不动"两条对照、
以及一条走 `materialize` 的接线自检），单测 rc=0；全量 **passed=167 failed=0**。
构建为 `cmake --build build_wb --config Release --target test_production`（rc=0），
只建测试目标、未触碰正在出盘的 `Release/model23dtiles.exe`。

**已于 2026-10-20 下午产物级闭合**（`lugouqiao_v19`，用含 `49b6d3c` + #6/#7 + A-44/A-45/A-46
的那份未提交二进制出盘，`rc=0`）。官方 validator 同轮双跑对照：
**v17 叶层 195 条 `is not of unit length: 0` ERROR / 归组 26 个内容文件 / `rc=1`**，
**v19 叶层 ERROR = 0 / `rc=0`**；自研尺 `zero_normal_triage.js` 同轮读 v17 = **195**
（退化 140 + 非退化 55，与 A/B/C 三档计划逐值对上）、v19 = **0**。
更有说服力的是**非 ERROR 的叶层逐类计数两份完全相同**（1969 / 1569 / 1569 / 691 / 7），
说明这轮只换了法线、没动纹理与图元。明细见 VALIDATION_REPORT §7 末尾。
**仍未闭合的是视觉验收**：v19 一份都没看，"电池 + validator 全绿"不等于可发布。
> 早期记录里"10:16 起跑的 `lugouqiao_v18` 用的 exe 时间戳 10:00（早于 `49b6d3c`），
> 不能当验收"这句仍然成立，v18 已按指令终止、盘上只有 `textures/`，是废件。

### A-44 `MeshBVH` 叶子按数组位置取三角形，最近距离系统性偏大（最高实测 3.7×）　**FIXED（未提交，2026-10-20 下午）**

**机制**：`buildRecursive` 用 `_tris[order[i]]`（HEAD `GeometryErrorEvaluator.cpp:104`）
计算节点 AABB 并按质心中位数重排 `order`，但 `distanceSquaredRecursive` 的叶子循环
读的是 `_tris[i]`（HEAD `:166`）——**置换从来没有落到三角形数组上**。
叶子区间 `[begin,end)` 因此按位置读到不属于这个包围盒的面片。

**方向**：只会**偏大**（读到的是同一 soup 里别的面片，距离是"某些面的最小值"而不是
全局最小值），不会偏小。所以**没有**触发手册 9.3"不得把保守上界写小"的违规，
产物在语义上一直是安全的；但它让每一次误差评估的下界、上界、收敛判定都建立在错的距离上：

- `errorMaxMeters` / `tilesetGeometricError` / `errorP95Meters` **全部虚高**，
  跨版本不可比（VALIDATION_REPORT §5b 给了修复前后的逐资产对照，t08 1.607e7→5 993.6 m）；
- 代理会被判得比实际粗糙（因为下界虚高 → 误差"看起来"大 → 更多轮简化/更保守的几何），
  这直接污染性能数字的可解释性；
- 收敛判定（`maxSampleDist + radius <= globalLower + tolerance`）里的 `globalLower` 偏大，
  于是 CERTIFIED 曾经"太容易"拿到。

**发现路径**（不是主动 review 出来的，是 A/B 的阳性对照逼出来的）：给误差上界做
"同一输入新旧上界逐节点对比"时，正例（真值已知的小几何）里**报出的下界超过了分析真值**——
这是不可能事件，只能来自查询本身。

**修**：建树结束后把置换落到数组上（`GeometryErrorEvaluator.cpp:98-100`：
`sorted[i] = _tris[order[i]]; _tris.swap(sorted);`），查询侧继续用位置下标，两侧口径一致。

**验证**：新增 `testMeshBVHMatchesBruteForce`（8×8 网格 64 三角形 × 6 探针，
oracle 用"每个三角形单独建一个 BVH"取最小——单三角形必为叶子，索引路径退化，因此精确）。
**修复前 6 个探针 4 个失败**，修复后 6/6；12 资产矩阵 `mx_bvh` 复跑
215 瓦片 **100 个自身上界变小 / 0 个变大**、`upper<lower` 违例 0、
b3dm/i3dm **内容文件改动 0**、`tileset_bounds.js` 12/12 干净、官方 validator 12/12 `errors=0`。

### A-45 覆盖半径在采样密度被降级后仍取 `L/3`，上界**不成立**（HEAD 即存在）　**FIXED（未提交，2026-10-20 下午）**

**机制**：上界 = `maxSampleDist + patchRadius`，`patchRadius` 在 HEAD 里无条件返回
`最长边 / 3`（HEAD `:232-240`，注释写"采样点集合 = 3 顶点 + 3 边中点 + 重心"）。
`L/3` 只在**那 7 个点全采到**时成立。而 HEAD 的密度自适应是
`while (perTri > 1 && from.size() * perTri > budget / 2) { --perTri; }`（HEAD `:253`）——
三角形数一大，`perTri` 能一路掉到 **1**，连三个顶点都不保证查全。
此时正确的保守系数是 `L/√3`（顶点集合覆盖：锐角外接半径 `L/(2 sin θ_max)`，
θ_max∈[60°, 90°) ⇒ ≤ `L/√3`；钝角外接半径 `L/2`），`L/3` 会**把上界写小**，
属于手册明令禁止的方向。这不是本轮并行改动引入的，HEAD 就是错的。

**修（两条一起，缺一不可）**：
① `perTri = 3 + extra`，**三个顶点永远查**（`GeometryErrorEvaluator.cpp:308-316`），
密度降级只砍 3 个边中点 + 重心；
② 系数按实际密度取（`:317-318`）：`radiusFactor = (extra == 4) ? 1/3 : 1/√3`。
另外把"剪枝只省算力、不改口径"写死：被剪的 patch 记进 `prunedUpper`（`:403`），
深度上限 patch 置 `depthCapped` 并把 key 计入 `upper`（`:427`），
`budgetExhausted = depthCapped || (堆未空 && 预算耗尽)`（`:490`）。

**验证**：新增 `testErrorBoundSoundness` 第一段专门把这条打出来——环带（rIn 0.98 /
rOut 1.02）+ 40 个内接等边三角形（外接圆半径 1 ⇒ 顶点全落在环带里、顶点距离 0，
真实误差由三角形内部贡献 ⇒ **sup ≥ 0.98**；最长边 `L = √3`）。
配 `maxSamplesPerNode = 3`：`cap = 1` 把密度压到只剩 3 个顶点、细分预算 0，
上界完全由覆盖半径撑起。**修复前精确失败在 `upper=0.577350`**——那正是 `L/3 = √3/3`，
而顶点集合口径要求 `L/√3 = 1.000 ≥ 0.98`，**方向是把上界写小**，与手册禁令相符地严重。
修复后通过并稳定判 `BOUND_LOOSE`（`ri.boundLoose` 也是断言之一：没有细分预算却不承认
`BOUND_LOOSE` 同样算失败）。
矩阵侧确认这个修复没有带来"上界变小以外"的异常（0 个变大、0 个 `upper<lower`）。

### A-46 `maxSamplesPerNode = 0` 时误差评估静默返回 0（防御性发现，非现场缺陷）　**加固（未提交）**

排查 A-45 的系数时顺路确认的边界：预算传 0 时旧实现会把"一个样本都没采"报成
"误差为 0"，而 0 恰好是**最小的上界**——这是一条能把上界写小的通路。
现在 `budget = (maxSamplesPerNode > 0) ? maxSamplesPerNode : 1`（`:310`），
并且 `perTri` 下限锁 3、未采样面按 `globalLower` 进账，
预算为 0/极小时的结果由 `testErrorBoundSoundness` 第 1、3 段钉住
（必须 `BOUND_LOOSE` 且 `upper >=` 真值）。
**12 资产与三份真实 FBX 的历史配置里 `maxSamplesPerNode` 均为默认 400 000，
没有出现 0**，所以本条记为加固、不记为既有产物的缺陷。

**遗留（不是缺陷，是判据）**：距离与半径都诚实之后，`nodesCertified` 在 12 个小资产矩阵上全部
变 0——`CERTIFIED` 要求上界−下界 ≤ 1 mm（或上界的 2 %），而这个差就是覆盖半径 ≈ patch
尺寸，40 万样本预算内数学上够不到。**335 MB 档不是归零**：`lugouqiao_v19` 仍有 63 个 CERTIFIED
（450→63），所以这条是**资产相关**的判据问题，不是普适结论。
**加大 `maxSamplesPerNode` 或接受诚实的 `BOUND_LOOSE`
是仅有的两条合法出路**；调 `toleranceMeters`/`certifyRatio` 属放宽验收，未获授权前不动。

### A-47 提速仪表有三个洞，其中一个会把"计数"当"秒"读　**FIXED（未提交，2026-10-21；判据①只在 65 MB 档闭合，335 MB 档待 `lugouqiao_v20` 复测。观测性缺陷，2026-10-20 下午由 v19 首份 `micro_*` 读数暴露）**

**背景**：上一轮的结论"`micro*` 仪表没接进 `ConversionReport`，跑完也读不到占比"是错的，
`ProductionPipeline.cpp:1219-1226` 一直在写。接上了之后暴露的是另外三条：

1. **32 % 的 CPU 秒没有归属**。`lugouqiao_v19`：`micro_*` 六项秒数累加
   `7510.16 + 564.71 + 89.80 + 44.59 + 25.55 + 10.02 = 8 244.83 s`，
   而同轮 detached 监视器记的进程 CPU 秒 ≈ **12 168.6 s**（末样本 15:32:31，
   wall 4 549 s ⇒ 平均 2.7 核）⇒ 差 **3 923.8 s（32 %）**。
   已知漏计的地方：叶瓦片 `_model.materialize()`（`ProductionPipeline.cpp:613`）
   **完全没有计时器**，`ProxyMeshBuilder::build()` 只回传 simplify/aggregate 两项，
   `collectTriangles` 之外的分支归属不明。
   **在补上之前，任何"某段是瓶颈"的说法都只能是猜测**——本轮就吃过一次：
   旧结论"写出/最终化可能是 t04/longxiang 只提升 8 % 的原因"，
   现在有数了，`micro_finalize_encode=564.71 s` 对 4 592.42 s 阶段 wall，**串行上限 12 %**，
   不是主因；反过来说，若 `materialize` 那 3 924 s 才是主因，前面所有杠杆都排错过。
2. **瓦片级耗时不可读**。`micro_evaluateSec` 只在 `finalizeTileFromWork` 里累加进
   `_microEvaluateSec`，没有落到 `report.tiles`。于是**读不出关键路径是哪个瓦片**，
   而 #6（乱序收割）与 #7（节点内并行）这两根杠杆的上限恰好就由关键路径决定：
   现在只能说"评估平均吃到 1.63 核"，不能说"最后一个瓦片跑了多久、它为什么是最后一个"。
3. **单位混淆**。`micro_evaluate_samples`（v19 实测 **438 681 617**）与
   `micro_aggregate_candidates`（**122**）是**计数**，却通过同一个
   `_report.addStage(name, seconds)` 写进 `stages` 这个**秒**数组。
   危害不是难看：`stages` 里任何"按秒排序找瓶颈"的读法都会把这两行读成
   "4.39 亿秒的巨型阶段"，而 `grep micro` 的人无法从字段名区分。
   这是**仪表自己说谎**，比缺仪表更贵。

**判据**：`micro_*` 六项秒数累加与进程 CPU 秒之差 ≤ 5 %；`report.tiles` 每瓦片有
`evaluateSeconds`；`stages` 里不再出现非秒的量（计数走 `ConversionReport` 的独立字段）。

**修法（不改生产行为，纯观测）**：① `processTileHeavy` 开头/结尾各一个 `Timer`，
`materialize` 单独一个；② `TileHeavyWork` 已有 `microEvaluateSec`，随瓦片一起进
`_tiles[id]` 并写进 report；③ `addStage` 之外加 `addCounter(name, value)`，
两条计数搬过去。**先做这个再做任何提速**（`PERFORMANCE_REPORT` §2.2 的 L0）。

**为什么定 OPEN 而不是"已修"**：本轮只完成了"发现并量化"，代码未动、没有复跑读数。
本条不涉及产物质量，不影响 v19 的验收结论。

#### 闭合读数（2026-10-21，**未提交**）：第三条漏的不是 `materialize`，是 libktx 的压缩线程

三个洞都补了，判据②③闭合；判据①在 **65 MB 档闭合（-0.8 %）**，335 MB 档要等 `lugouqiao_v20`
用新二进制出盘才能复测，所以这条的状态写成 **FIXED（65 MB 档实测，335 MB 档待复测）**。

**加了什么仪表**（纯观测，不改生产行为）：
① 叶瓦片 `_model.materialize()` 单独一个 `Timer`/`CpuTimer`（`micro_materialize*`）；
② `report.tiles[].timing` 逐瓦片落 `evaluateSec / evaluateCpuSec / evaluateWorkerCpuSec /
evaluateSamples / heavySec / materializeSec / proxyBuildSec / collectSec / bvhBuildSec /
finalizeSec / writeSec`；
③ `ConversionReport::counters`（`addCounter`），`micro_evaluate_samples` /
`micro_aggregate_candidates` 从 `stages` 搬过去；
④ **新增的第第四条**：`GltfOptimizer::textureExportStats()` 量每次纹理落盘的
壁钟、**调用线程** CPU 秒与**进程** CPU 秒之差（`src/utils/GltfOptimizer.cpp` 的
`TextureExportProbe`，落在 `exportImage` 真正写盘的那个分支里），
报表项 `micro_texture_export` / `_thread_cpu` / `_process_cpu` + 计数
`micro_texture_export_files`。
**动机**：写 `.ktx2` 会进 `ktxTexture2_CompressBasisEx`，而它自己起
`threadCount = hardware_concurrency` 个线程（`src/osgdb_ktx/LoadTextureKTX.cpp:630`）。
生产侧**所有** `micro_*` 都是调用线程的 `GetThreadTimes`，这批第三方线程一秒都记不进去——
这正是"32 % CPU 秒没有归属"的候选，而 `materialize` 不是。

`longxiang_l0b`（65 MB 档，`-ptw 512`，`rc=0`，129 瓦片，全新输出目录；单轮样本）：

| 量 | 读数 |
|---|---|
| 进程 CPU 秒 / 壁钟 | 542.91 s / 190.42 s ⇒ 平均 **2.85 核** |
| 不重叠各层 CPU 秒相加（`micro_total_cpu` 240.84 + `micro_finalize_encode_cpu` 111.52 + `input_read_cpu` 1.88 + 串行三段 0.19） | **354.42 s** |
| ⇒ 补仪表前无归属 | **188.5 s = 34.7 %**（与 v19 的 32 % 同一件事，不是巧合） |
| `micro_texture_export`（壁钟 / 调用线程秒 / 进程秒） | **78.90 / 64.77 / 257.75 s**，121 个文件 |
| ⇒ 第三方线程（进程 − 调用线程） | **192.98 s** |
| 已归属合计 547.41 s 对进程 542.91 s | **残差 −4.50 s = −0.8 %** ✅ 判据①（≤ 5 %） |

三件必须一起读的事实：
1. **`materialize` 只值 0.09 CPU 秒**（`micro_materialize` 壁钟 0.129 s）。上一轮把它列为
   "3 924 s 无归属的头号嫌疑"是**排错对象**：叶瓦片物化根本不贵。
2. **写出阶段的 CPU 是调用线程读数的 4 倍**：纹理落盘 78.90 s 壁钟里，主线程只花 64.77 s，
   进程却花掉 257.75 s。`micro_write_cpu` 那种"主线程串行秒"口径**结构性地看不见**
   basis 压缩的量——L5（编码/写出并行）以前只按主线程秒评估，低估了。
3. **残差为负**说明进程口径把写出窗口里并行瓦片的 worker 秒也计了一次（上界）；
   `-0.8 %` 就是这个双重计数的量级，不是漏计。

**判据②③**：129/129 瓦片都有 `timing.evaluateSec`（v19 时一个都没有）；
`stages` 里不再有任何计数，计数三项都在 `counters`。

**为什么还不写"全绿"**：判据①只在 65 MB 档实测闭合，`lugouqiao_v19` 的那 3 924 s
是旧仪表量的，没有用新仪表复跑过；335 MB 档纹理量比 65 MB 档大一个数量级，
第三方线程占比很可能不同。v20 复跑后本条才算真正闭合。
**没有单测覆盖"仪表算得准"**，只有接线断言（`pipelineCoverage` 用例里 5 条：
三项 `micro_texture_export*` 阶段存在、`micro_materialize`/`micro_total_cpu` 存在、
逐瓦片 timing 非零、`micro_evaluate_samples` 在 `counters`、`stages` 里不含
`samples`/`candidates` 字样）。数值闭合靠产物级复跑，`test_production` **228/228**。

---

### A-48 代理纹理字节按 primitive 重复计数，42/60 内节点被判"超预算"退化成无贴图灰模　**代码 FIXED / 验收 OPEN（未提交，2026-10-21；产物级新破口见 A-51）**

**现象**（`longxiang.fbx`，65 张贴图被大量 primitive 共享）：顶层 `t0.b3dm` 只有 350 KB、
**一张纹理都没有**，整棵树 60 个有内容的内节点代理里 **42 个无纹理**。用户判据：
「顶层无贴图是不可接受的」——远景 LOD 的意义是远处先看个有颜色的轮廓，灰模不是降级
LOD，是缺件。

**根因**：`processTileHeavy` 的超限判据用 `Σ PrimRef.byteEstimate` 逐 primitive 累加纹理
字节。同一张 2048×2048 RGBA 被 N 个 primitive 引用就算 N 次（`longxiang` 实测共享度最高
的贴图被几十个图元引用），估算值直接放大一到两个数量级 → `geo + tex` 永远超 8 MiB →
`FarLodTextureStripper` 把整片剥成灰模。A-30 那次拆分 `BytesSplit{geo, tex}` 修的是
"纹理该不该算绑定约束"，没修"纹理怎么算"，所以这次爆在同一个判据的下一次调用里。

**修法**（三步，全在预算判据与写出阶段，不动产物语义）：
1. `ProxyMeshBuilder::estimateBytes()` 返回 `ProxyBytes{geo, texFull, texFloor}`，
   纹理项**按 `osg::Image*` 去重**；`texFloor` 是"每张折到 `proxyTextureFloorEdgePx`(64 px)
   后"的面积折算字节，给调用方"先降分辨率、最后才去纹理"的依据（手册 7.3 的顺序）。
   `ProxyResult::needsTextureless` 换成 `fitsWithFloorTextures`（三处：identity 早返回 /
   身份保持简化 / 聚合接受）。
2. 新增 `ResourceBudget::proxyTextureMaxEdgePx`（默认 **512**，CLI `-ptw`）：内节点代理首轮
   就按这个上限压纹理，叶瓦片不受影响（仍用全局 `-tw/-th`）。下限校验进
   `ProductionProfile::validate()`：`proxyTextureMaxEdgePx < proxyTextureFloorEdgePx` 直接
   报错而不是静默钳制——**不得私自降低上界**同样适用于下界方向的保护。
3. 仍超硬预算时走降档阶梯 `textureDownscaleLadder()`（256→128→64，图集尺寸同步折小，
   否则 atlas 页数不降反升），到下限还装不下才剥纹理并记 warning。

**复验（产物级，`longxiang_ptw2`，2026-10-21）**：内节点有纹理 **18/60 → 58/60**，
无纹理 **42 → 2**；剩下的 2 个（`t73`/`t74`）是**源模型本就无图**——它们整棵子树连叶
`t75`-`t77` 都是零图，不是被剥掉的。顶层 `t0.b3dm` 350 KB 无图 → 883 KB 带 15 张图。
内节点 >8 MiB 硬限：**0 张**，最大单文件 2 580 040 B。内节点产物字节 41 874 KB → 43 917 KB
（**+4.9 %**），代价只发生在本该有图的地方。
> 注：这一轮的叶字节 **33 817 KB** 比修前基线（42 036 KB）小了两成——那不是本条的收益，
> 是 A-49 那个串瓦片的缺陷。两条分开定级、分开复验。

> **本条复验的口径缺口**：上面只核了**字节**预算（内节点 >8 MiB = 0 张），没核
> **primitive/材质结构预算**。同一批产物实测代理图元数从 ≤10 涨到 ≤32、6 张破手册 §3 的 ≤24
> ⇒ 见 **A-51**。"纹理保住了"不等于"没违反 §7.3 的预算顺序"。

**回归测试**：`test_production proxyTexture`（去重只算一次 + 阳性对照"两张不同纹理必须算两次"
+ 下限以下不缩水 + 几何项不受纹理估算污染 + 空节点为 0）、
`proxyTextureLadder`（**叶必须保留全部档位**：判据一度写成与钳制后的全局 `-tw/-th` 比较，
而叶瓦片拿到的正是 2048，条件恒真会让整套字节兜底对叶静默失效；集成用例走三角形超限分支
看不见这个退化，所以把它抽成可测函数钉住）。

---

### A-49 代理的纹理上限顺着共享的 `osg::Image` 改掉**叶**瓦片的贴图　**FIXED（未提交，2026-10-21）**

**现象**：A-48 修完，同二进制同资产两跑做 A/B（唯一变量 `-ptw 512` vs `-ptw 2048`），
叶瓦片并没有躲过这个上限：**67 张里只有 8 张逐字节相同**，叶合计 **33 817 KB vs 42 023 KB
（-19.5 %）**。看单瓦片 `t71.b3dm`：图集页尺寸两边完全一样（2048×2048 / 1024×2048 / 128×128），
但 KTX2 字节 294 305 vs 534 958、160 427 vs 234 754 —— **尺寸没变、内容变小**，
是"先缩小再塞回大图集页"的糊图特征，不是编码参数差异。

**根因**（两条都要成立，都在写出阶段）：
1. `GltfOptimizer::TextureAtlasBuilderVisitor::apply()` 每个 drawable 克隆 StateSet 用的
   copyop 是 `DEEP_COPY_STATESETS|DEEP_COPY_STATEATTRIBUTES|DEEP_COPY_TEXTURES`，
   **没有 `DEEP_COPY_IMAGES`**。实测（`test_production texResize` 里的前提核对）：克隆出的
   `osg::Texture2D` 是新对象、用户值也带过来了，但 `getImage()` **仍是源模型里那张共享图**。
2. `resizeImageToPowerOfTwo()` 用 `img->scaleImage(...)` **就地改像素**，还会把
   `origin-width/height` 写在这张共享图上，把后续瓦片的判据基准也一并改掉。

合起来：`-ptw` 让第一个代理瓦片把共享原图缩到 512，之后每个引用同一张贴图的瓦片
（含全部叶）拿到的都是缩过的图。HEAD 之所以看不出来：只有一个全局上限时，就地缩放对所有
瓦片是同一个目标尺寸，**幂等**；一旦上限按瓦片不同（本条正是 `-ptw` 引入的），就地缩放就
会串瓦片。属于 A-41 同族（就地改写共享对象）。

**危害定级**：产物质量缺陷。叶瓦片按手册 7.4 是"最终解码后必须保留原始表面"的那一层，
用远景代理的预算吃掉叶的贴图分辨率，而 `conversion-report.json` 里看不见
（`proxyTextureCappedCount` 只数代理），电池和 validator 也测不出来（几何没变）。

**修法**：`resizeImageToPowerOfTwo()` 改成**返回"可安全使用"的图**——判据一字未动
（PO2 + 上限），不需要改动时原样返回入参（不做无谓整幅拷贝，也不留两份可分别改写的像素），
需要改动时 `new osg::Image(*img, SHALLOW_COPY)` 私有化后再 `scaleImage`，`ORIGIN_*` 只写在
副本上。5 个调用点全部接返回值；三处贴图路径（`optimizeOsgTexture` / `optimizeOsgTextureSize`
/ `addImageFromTexture`）把副本**挂回本瓦片克隆出来的那个纹理**——图集回填按
`texture->getImage()` 取 `packer.getId(image)`，不挂回去打包坐标就对不上；两处图集页路径
直接接收返回值（页是本地新建对象，本来就私有）。
`ProductionPipeline.cpp` 里"克隆即私有"的注释按实测改写，不再声称克隆能隔离像素。

**复验（产物级，2026-10-21，同机同资产同二进制，唯一变量 `-ptw`）**：
`F:\wb_3dtiles_out\{longxiang_ptw2, longxiang_ptw2048, longxiang_iso512, longxiang_iso2048,
longxiang_final512, longxiang_final2048}`，尺子 `leafab.js`（逐文件 md5 + 字节）。

| 对比 | `-ptw` A vs B | 二进制 | 叶瓦片逐字节相同 | 叶合计字节 |
|---|---|---|---|---|
| `ptw2` vs `ptw2048` | 512 vs 2048 | 修前（就地缩放） | **8 / 67** | 33 817 vs 42 023 KB（**-19.5 %**） |
| `iso512` vs `iso2048` | 512 vs 2048 | 修后 | **64 / 67** | 42 022 vs 42 021 KB |
| `final512` vs `final2048` | 512 vs 2048 | 修后（复跑） | **64 / 67** | 42 021 vs 42 022 KB |
| `final512` vs `iso512` | **512 vs 512**（噪声地板对照组） | 修后 | **64 / 67** | 42 021 vs 42 022 KB |

- 修后三行的"64/67"与**同参数两跑的噪声地板完全一样**，差的都是同样 3 张
  （`t40`/`t78`/`t119`）：`t40` 在四轮里给出 4 个不同 md5，属每轮都会变的抖动
  （A-40 未闭合的表现，本条不重复定级）。差异位置每张只有 1 张图、字节
  ±368 / ±208 / +1552，与 -20 % 那种系统性量级不是一回事。
  ⇒ `-ptw` 对叶瓦片的影响已经降到抖动以下。
- 代理侧收益未被削弱：内节点有纹理仍是 **58/60**（缺的 2 张 `t73`/`t74` 源模型本就无图），
  `漏压=0` —— 内节点里 >512 的 27 张全是带非 identity `KHR_texture_transform` 的**图集页**，
  切片本身没有超限（"≤512"说的是纹理内容分辨率，不是页的边长，别把这两件事混写）。
- 硬预算：内节点 >8 MiB **0 张**，最大单文件 2480 KB；内节点产物字节
  43 706 KB（512）vs 119 737 KB（2048）→ 上限本身省 63 %。
- 峰值 RSS 3631～3645 MB 全程一个量级：私有化副本没吃到内存。
- `test_production` **223/223**（新增 `texResize` 11 项断言，含"低于上限但非 2 次幂的图
  同样要私有化"这条阳性对照）。
- 计时不作证据：同参数两跑 wall 303.8 s（冷）→ 122.2 s（热），本机缓存冷热主导。

**判据（后续任何人再动纹理路径时要重新成立）**：同一二进制、只有 `-ptw` 不同的两跑，
叶瓦片逐字节相同的比例不低于"同一 `-ptw` 两跑"的抖动水平。

---

### A-50 报告里的 profile 是硬编码缺省值，且"降过分辨率"把空操作也算成收益　**FIXED（未提交，2026-10-21）**

纯观测性，两条都会让产物**自证不了自己**：

1. `ConversionReport::profile` 从来没被赋值过。`conversion-report.json` 的 `profile` 字段
   于是恒等于代码里的缺省值：`-ptw 2048` 那一轮照样显示 `proxyTextureMaxEdgePx: 512`，
   `-tol` / 预算 / `bins` 的覆盖同样读不出来。上一轮记忆和文档里"profile 默认值记录在
   conversion-report.json"这句话**只对了一半**——记的是缺省值，不是生效值。
   修法：`ProductionPipeline::run()` 里 `_report.profile = _profile;`。
2. `proxyTextureCappedCount` 的语义是"传过上限"，不是"降过分辨率"：`-ptw >= -tw/-th`
   时上限是空操作（`min()` 钳制后不变），却照样把 60 个代理全记成 capped，汇总 warning
   也跟着说"capped at <= 2048 px"。修法：只有当代理上限**严格小于**全局
   `min(maxTextureWidth, maxTextureHeight)` 时才带上，否则传 0；计数与 warning 都只反映
   真起作用的降分辨率。

**复验（产物级，2026-10-21，final 二进制两跑）**：`longxiang_final2048` 的
`conversion-report.json` 现在写 `profile.budget.proxyTextureMaxEdgePx = 2048`
（修前与 `-ptw 512` 那轮一样都显示 512），`proxyTextureCappedCount = 0`，日志里
"far-LOD proxies: … capped" 这条汇总 warning 也不再出现；`longxiang_final512` 仍是
512 / capped=60 / 有 warning。两条判据方向相反，才算数。

---

### A-51 保住代理纹理之后，内节点瓦片的 glTF primitive 数冲破手册 §3 的 ≤24，而生产侧对 primitive/材质**没有任何闸**　**OPEN（2026-10-21，产物级实测；阻塞 A-48 的验收）**

**现象**（实测，非推断）：`node scripts/glb_prims.js F:/wb_3dtiles_out/longxiang_final512`
→ **rc=1**，6 个内节点代理超限：`t0`=27、`t1`=26、`t21`=28、`t29`=25、`t79`=32、`t100`=28。
同一把尺子对修前基线 `longxiang_covfix` 判 **rc=0，全库最大 11**。

逐瓦片口径（`F:\wb_3dtiles_out\per_tile_prims.js`，GLB 偏移算式照抄 `glb_prims.js:87-106`，
`纹理解析不明=0` 表示 image 索引全部解析成功，KTX2 的索引在
`extensions.KHR_texture_basisu.source` 里而不是顶层 `.source`）：

| 数据集 | 叶 prims max/mean | 代理 prims max/mean | 代理 图去重后 max/mean | 代理 prims>24 | 代理无纹理 |
|---|---|---|---|---|---|
| `longxiang_covfix`（修前） | 11 / 3.6 | **10 / 2.6** | 6 / 1.1 | 0 | 42 |
| `longxiang_final512`（A-48/A-49 后） | 11 / 3.6 | **32 / 12.7** | 18 / 7.6 | **6** | 2 |
| `longxiang_final2048` | 11 / 3.6 | 31 / 12.7 | 19 / 7.6 | 6 | 2 |
| `longxiang_l0b` | 11 / 3.6 | 32 / 12.7 | 18 / 7.6 | 6 | 2 |

三点要说清的：
1. **叶瓦片四份产物逐值相同**（max 11 / mean 3.6）⇒ 退化完全发生在代理侧，与 A-49
   修的那条"代理串改叶贴图"是两件独立的事，不是同一条回归。
2. 声明的预算也一起破了：`conversion-report.json` 里 `profile.budget.maxPrimitivesPerTile = 20`
   （desktop-web-high-quality），超限的 6 张同时 >20 与 >24；`per_tile_prims.js` 统计
   代理里 **11 张 >20**（不止那 6 张破 24）。
3. 代理侧 **`prims == materials` 严格 1:1**（`t79`=32p/32m、`t29`=25p/25m、`t0`=27p/27m）
   ——一个材质都没合并。

**根因链**（四步，前三条是本条的机制，第四条是它没被拦住的原因）：

1. **代理的 primitive 数 = 聚合进来的源 feature 的 distinct 材质数。** 剥掉纹理时
   （A-48 之前 `FarLodTextureStripper` 的做法）所有 drawable 的 StateSet 落到同一个纯色材质，
   `GltfOptimizer` 的 `SHARE_DUPLICATE_STATE` / `MERGE_GEOMETRY` 把它们并成 1~10 个图元
   （covfix 实测代理 mean 2.6）。**保住纹理后这一步不再发生** —— 所以本条是 A-48 修法的
   必然后果，不是新写的代码坏了。
2. **图集本来该接住这个合并，但对 `longxiang.fbx` 整条 atlas 路径被 UV 越界挡住**：
   `src/utils/GltfOptimizer.cpp:1089-1099` 只要 drawable 有任一 TEXCOORD 分量绝对值 > 1，
   就 `bBuildTexturePacker = false` 并转 `exportOsgTextureIfNeeded()` 单图导出。
   实测 `t29` 图元 UV 范围 `[-71.28, 72.28]×[-5.51, 6.51]`、`[-152.23, 153.23]×[0,1]`，
   sampler `wrapS/wrapT = 10497 (REPEAT)` ⇒ 这是**合法的 UV 重复**，不是坏数据；
   被挡在图集外是那条判据的保守选择，不是本资产的缺陷。
3. **共用同一张图的材质也合不动**：`t29` 里 11 个图元共用 image 6，但各自的
   `KHR_texture_transform` 的 offset/scale 不同（`[0.25,1]×[0.25,-0.5]`、
   `[0.875,0.375]×[0.03125,-0.0625]` 等）—— 源模型用一张大图分块贴多处的正常做法。
   本轮按"同图 + 同 baseColorFactor + 同 metallic/roughness + 同样变换 + 同双面/透明"
   做签名去重，实测**合并后上限 == 现有 prims**（代理 mean 12.7 → 12.7，max 32 → 32），
   也就是说**什么都不改、只指望"重复材质归并"是零收益的**。
4. **生产侧没有 primitive/材质闸**：`maxPrimitivesPerTile` / `maxMaterialsPerTile`
   （`include/3dtiles/production/ProductionProfile.h:31,34`）只被
   `src/3dtiles/production/SpatialPartitioner.cpp:16-17` 的源侧切分比例消费（叶瓦片因此没破），
   `selfValidate` 里没有对应检查，代理路径完全不看它 —— **A-32/A-33 同族**
   （声明的预算没有对应的闸）。这就是为什么 `glb_prims.js` 是唯一发现它的人。

**危害定级**：阻塞级（对 A-48 的验收而言）。运行时影响是远层 LOD 的 draw call 从 1~10 涨到
25~32（三角形数、实例数、`contentBytes` 都没变），属**结构性退化**而不是字节膨胀；
`longxiang_final512` 因此**不是可发布产物**。

**四条候选方向**（均未实施，需用户裁定；列 D 只为完整）：

| # | 方向 | 实测上限 | 代价 / 风险 |
|---|---|---|---|
| A | 代理写出时把 `KHR_texture_transform` **折叠进 TEXCOORD_0**，之后共用同一张图的材质就能合并 | 代理 prims 上限 ≈ 图去重数：**max 18、mean 7.6**，全库 ≤20 也 ≤24 | REPEAT 下"先变换后 wrap"与"先折叠后 wrap"等价，但**等价性本轮只有一行推理，没实测**；且 writer 会统一加 `[1,-1]` 的 Y 翻转，折叠时不能重复应用 ⇒ 必须配"折叠前后采样等价"的单测。改的是贴图坐标，属**质量敏感路径**，要产物对照 + validator + 视觉验收 |
| B | 给代理路径加**真的** primitive/材质预算闸，超限就把该聚合代理再细分成多个瓦片 | 硬保证 ≤ 预算 | `tileCount`/`maxDepth` 会变，与本轮"运行时细化行为不变"的质量口径冲突 ⇒ 归用户裁定 |
| C | 只对超限的 6 张代理退化成无纹理 | 立刻 rc=0 | `t0`/`t1` 是顶层，直接撞"顶层无贴图是不可接受的"这条硬判据 ⇒ **未经用户改口不可行** |
| D | 把 §3 的 24 上调 / 代理豁免 | — | 手册禁止，`不得私自降低上界`，我没有这个权限，列出仅为穷举 |

**（2026-10-21 更新，读上表前先看这句）**：上表是裁定前的备选，用户已选 **A** 并实施完毕，
实测结果在文末 §A-51 落地。表内 A 行那两处（"等价性只有一行推理，没实测"、"上限 max 18"）
已被实测取代：等价性有构造推导 + 450 组产物级多重集测量，实测代理上限是 **24**（不是 18）。
下面那句"下一步：#16 保持未完成"同样只对裁定前成立。

**手册顺序上的位置**：§7.3 是"几何预算 → **材质/图元合并** → 纹理溢出处理"。A-48 那轮直接
动了第三档（纹理），本条爆在第一档没做（合并）。所以方向 A/B 是**回到顺序里补做前一档**，
而不是把第三档改回去。

**下一步**：#16 保持未完成；提交裁定（用户）在方向选定之前不成立。
**排序上的连带影响**：`lugouqiao_v19` 的 `glb_prims` 判 0 违例（max prims/tile = 11）——那份产物是
**不含 A-48 的 10-20 13:09 二进制**出的，代理仍被剥成灰模。所以 `lugouqiao_v20` 一用新二进制出盘，
这 335 MB 档**很可能同样破**（它的贴图共享度比 longxiang 更高）。方向应在 v20 之前定，
否则 v20 又要换二进制重跑一遍。

---

### A-51 落地（方案 A，2026-10-21）：折叠已实施并做完产物级验收，实测把"折叠是否改采样"这个问题本身纠正了一次

用户裁定：**"按照方案A执行"**（代理写出时把 `KHR_texture_transform` 折进 `TEXCOORD_0`，
共用同一张图集页的材质因此可归并，**保纹理**）。以下每条都标了实测 / 未证实。

**1. 机制更正（本条根因链第 1 条写错了合并发生在哪儿）**
`SHARE_DUPLICATE_STATE` / `MERGE_GEOMETRY` **不在** `EXPORT_GLTF_OPTIMIZATIONS` 里
（`include/utils/GltfOptimizer.h:155-163` 实际是 `INDEX_MESH | REDUCE_DRAWCALL_OPTIMIZATIONS`），
所以它们不是代理图元合并的执行者。真正的合并全在写出侧：
`Osg2Gltf::getCurrentMaterial` 用 `tinygltf::Material::operator==`（**比较 `extensions`**）去重材质，
再由 `GltfMerger::mergeMeshes()`（`src/osgdb_gltf/GltfMerger.cpp:80`）按
`world matrix × primitive.material × 属性名集合` 分桶融合。
⇒ "同图不同子矩形合不动"的原因是变换进了 `extensions` 比较，折叠正是把这项差异消掉。
实测（同码 A/B，只差折叠开关）：代理 `prims max/mean` **32 / 12.7 → 24 / 9.8**、
`prims>24` **6 → 0**、叶瓦片 **11 / 3.6 不变**、代理无纹理 **2 → 2**（纹理一条没少）、
内容字节 **87 793 600 → 87 563 536 B（−230 064 B，−0.26 %）**。
A-51 表格里"上限 ≈ 图去重数 max 18"那句是**预测**，实测落在 24：图去重数本身在折叠后
的瓦片里被 `合并后上限` 卡住（`per_tile_prims.js`：代理 `图去重 max 18`、`合并后 max 24`），
两者不是一回事，取实测。

**2. 等价性从"一行推理"升级为构造性质 + 产物级测量**
写出侧 `Osg2Gltf::getOsgTexture2Material`（`src/osgdb_gltf/Osg2Gltf.cpp:843-872`）读
`TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME` 的 enable 键，键关就用局部默认 `(0,0,1,1)`，
然后**无条件** `flipGltfTextureYAxis`（`offsetY=1-oy, scaleY=-sy`）。
未折叠最终 V = `1-oy-sy·v`，折叠最终 V = `1-(sy·v+oy)` —— 逐值同式，只差一次乘加舍入
（实测最大逐顶点偏差 **2.98e-8**）。单测 `foldUv` 8 条把烘焙算式、原始 UV 未被污染、
同纹素等价、还原后指针同一、共用宿主必须全可折、BASECOLOR/NORMAL 不一致时拒绝、
`texCoord!=1` 拒绝、恒等折叠仍可撤销全部钉住。

**3. 产物级"折叠有没有改采样"：450 组 / 505 880 顶点，0 组有差**
尺子 `E:/tmp/diag_all.js`（本轮新写）：对每个代理瓦片的每张图集页（按 `images[].name`
= 内容哈希配对），把两侧**最终采样坐标** `offset + scale·uv` 量化到 `1e-5` 做多重集差。
结果 **450 组全部相等，最大计数偏差 0**；4 组因页换名无法按页配对（页尺寸两侧逐值相同）。
> **纠正一次自我误判**：先前那把按"排序后索引对齐"比较的尺子（`/tmp/fold_all.js`）报了
> "20 个图组超容差"，那是**尺子的错** —— 合并图元后相同 `(u,v)` 的并列顺序会变，索引对齐
> 就把同一批坐标错配成不同批。改成量化多重集后，最严重的两组（`t2` img5 188 顶点、
> `t21` img5 310 顶点）差集都是 **0 / 0**，再扩到全库仍是 0。

**4. 那 4 组换页不是折叠造成的 —— 新发现 A-52**
同一二进制、同一参数连跑两遍（`fold512` vs `fold512_b`，都不含守卫）出现**同类 5 组换页 +
3 个叶瓦片字节不同**；带守卫的 `guard512` vs `guard512_b` 也是 5 组 + 3 叶。
⇒ 图集页身份本来就不跨运行稳定。**原归因（`GltfOptimizer.cpp:2026` 按指针哈希遍历装配）已推翻**，
真因见 A-52 行的"实测机理"：图集画布缝隙未初始化。
**A-52 实测机理（2026-10-21 结案）**
判据来自 `WB_ATLAS_TRACE=1`（`GltfOptimizer.cpp:2015`，把每轮进打包器的图打印成
`全长 fileName + name() + s×t + 逐像素内容哈希`）+ `E:/tmp/prod_ab.sh` 逐字节差分。三对同二进制对照：

| 对照 | 配置 | 结果 |
|---|---|---|
| `lx_tr2_a` / `lx_tr2_b` | `WB_JOBS=1`，修画布前 | 138 轮 `keys=` 行（含内容哈希）**逐字节相同**、`packed=/dims=` 相同，**只有 7 个页名不同** |
| `lx_z_a` / `lx_z_b` | `WB_JOBS=1`，画布清零后 | trace 差异 **0 行**；产物 **246/246 逐字节相同**（PASS） |
| `lx_p_a` / `lx_p_b` | **默认并发作业数**（出货配置），清零后 | 同样 trace 差 **0 行**、产物 **246/246 相同**（PASS）；两遍 rc=0 |
| `lx_tr2_a` / `lx_tr2_b`（同上，作阳性对照） | `WB_JOBS=1`，清零**前** | 同一把尺子判 **FAIL**：6 ↔ 7 个孤儿页 ⇒ PASS 不是量尺失灵 |
| `lx_det_a/b`、`lx_ser_a/b`、`lx_t1_a/b` | 修画布前的并行 / 串行 / **标称** `WB_KTX2_THREADS=1`（2026-10-22 复核：该标称未生效，见下注） | 都差 6–7 页 + 6–8 瓦片 ⇒ 与作业数无关；**"与 basis 编码线程数无关"这半句本行支撑不了** |

> **2026-10-22 复核标注（不改写历史，只限定其证明力）**：上表的 `lx_t1_a/b` **不是有效的
> `WB_KTX2_THREADS=1` 对照**。当时部署的 `Release/osgdb_ktx.dll`（sha256 前缀 `1feb96f2…`，mtime 10-20 18:17）
> 里没有这个 getenv（对 DLL 搜字符串 0 命中，同文件正对照 `ktxTexture2_CompressBasisEx` 1 命中；
> `git show HEAD:src/osgdb_ktx/LoadTextureKTX.cpp` 查不到该变量、`git log -S'WB_KTX2_THREADS'` 无结果），
> 而 `osgdb_*` 是 MODULE 目标、不在 `model23dtiles` 的依赖图里，`--target model23dtiles` 不会重建它
> ⇒ 两侧执行的是同一份代码。**这一对作为"同二进制同参数的两跑"仍然有效**（它记录的 6–7 页 + 6–8 瓦片
> 差异是真实的非确定性证据，也正是 A-52 要解释的现象），但它**不能用来排除 basis 编码线程数的影响**。
> 2026-10-22 11:41:33 显式重建 `osgdb_ktx` 后（新 DLL 前缀 `1124d517…`，含 knob），线程配额的产物影响
> 才第一次可被验证；当前状态与真实证据见 PERFORMANCE_REPORT 的 L7 行与 STATUS 的"提速线"小节。

⇒ 根因不在收图、不在排序；"不在编码器线程"这半句当时由上面那对无效对照支撑，
**属未被检验**（A-52 的结案另有依据：清零后同二进制两遍 246/246 逐字节相同，见上表 `lx_z_*` / `lx_p_*` 两行）：
`TexturePacker::pack` 用
`osg::Image::allocateImage` 建图集画布后**从不初始化**（`TexturePacker.cpp:100-109`），
而 OSG 的 `allocateImage` 不清零。子矩形很少铺满整张画布，缝隙里留着的是上一次堆使用的
残留字节；`exportImage` 又按**整张画布**的内容哈希命名并压缩它 ⇒ 缝隙字节变 → 页名变 →
KTX2 编码字节/字节数变 → 引用该页的瓦片字节变。页**尺寸**两侧相同正是这个机理的特征
（布局没变，变的只是没被写过的字节）。修复：分配后 `memset(total->data(), 0, getImageSizeInBytes())`，
长度与 `computeImageHash` 读取的跨度一致（r=1），缝隙固定为透明黑。

**这条把"产物逐字节对照"从阴性对照恢复成可用尺子**：清零后串行档两遍全等，
所以 A-51/后续回归可以继续用逐字节 diff 做主判据；量化多重集尺子（`E:/tmp/diag_all.js`）
降级为交叉验证。`VALIDATION_REPORT.md:51` 那次"md5 完全一致"不是假象，而是画布恰好被
复用到相同残留字节——同一把尺子时好时坏，原因就是本节。

**5. 验收过程中修掉一个真实缺陷：折叠态跨瓦片外泄**
`writeContent` 里 `optimize()`（建图集）在 `foldBeforeWrite()` **之前**，而还原只发生在
下一次 `fold()` 开头和超限阶梯内 ⇒ 瓦片 N 清掉的变换键会被**瓦片 N+1 的图集装配**读到。
原代码在阶梯处已经写了"折叠态绝不能进下一轮图集"的注释，但只在同一个瓦片内守住了。
修复：`ProductionPipeline.cpp:545-556` 的作用域守卫，`writeContent` 一返回就还原。
守卫**保留的理由是瓦片隔离（正确性），不是性能**；它自己那一次 UV 还原值多少秒，这一轮没量到。

> **撤回一条我自己写错的数**：本节先前记过"加守卫让写出阶段 2.4×、墙钟 120 → 338 s"。
> 那个归因**错了**，错在拿两个相隔一小时的时间窗做 A/B。三条反证（全部实测）：
> ① **守卫不在的计时器也跟着翻倍**：`micro_evaluate` 在贴图写出**上游**、根本不读 UV，
> 130.9 s（无守卫）→ 269.2 s（有守卫）；若代价真来自守卫，它不该动。
> ② **同时段的无守卫对照和有守卫一样慢**：16:53 备份后把守卫整块摘掉重编（exe 16:54），
> 17:00 那次 `micro_evaluate=260.1 / micro_write=250.5 / micro_texture_export=154.0 / 墙钟 329.1 s`，
> 与 16:00、16:07 两次带守卫的运行（269.2/257.4/159.8/338.4 与 266.2/255.1/157.7/336.3）同档。
> ③ **工作量计数器五次运行逐项相同**（`micro_evaluate_samples` 48 963 414、
> `micro_texture_export_files` 121、`tileCount` 129）⇒ 不是多干了活，是同一台机器在不同时刻的速率差。
> 把当天全部 longxiang 运行按完成时刻排开，同一计数器下墙钟跨 **119.9 – 436.4 s**
> （09:14=391.8、10:29=436.4、10:38=122.2、14:55=119.9、16:00=338.4、17:00=329.1），
> 这正是本仓自己记过的"耗时不可跨会话比、A/B 必须同时段 + 对照 exe"
> （`PERFORMANCE_REPORT.md:215`、`:253`，`STATUS.md:706`：同一份 longxiang、同一二进制，
> `micro_evaluate_samples` 恒为 48 963 414，wall 仍在 122 ~ 436 s 之间跳）。
> **守卫的真实代价 = 未测**；要量它得在同一时间窗内交替跑两个二进制，
> 记到性能线（#10/#11）去做，不在此处用一组被污染的读数冒充。
守卫本身的结论不变：不加守卫时那份"折叠版更快"至少有一部分是**靠污染后续瓦片的贴图决策省下来的
编码**，与 A-48 立的"本瓦片之外的瓦片绝不能被这次降分辨率带小"同族。

**6. 预算闸（A-32/A-33 同族的通用缺口）已补，并且真的在开火**
`selfValidate` 现在解析每个内容文件的 glTF 容器（b3dm 28 B / i3dm 32 B 头 + 4 个表长 +
GLB chunk，`cmpt` 逐内层递归）数 primitive / 材质，超 `maxPrimitivesPerTile` /
`maxMaterialsPerTile` 即失败。单测 `budgetGate` 用假 b3dm 打正反两侧：真实写出的 b3dm
解析得动、正好卡上限放行、多 1 个 primitive 与多 1 个材质各自必拒。
**实测开火**：默认 `-dc 20`（`tools/model23dtiles/main.cpp:493`）跑折叠后的 longxiang →
**rc=3**，拒的是 `t0`=24、`t1`=22 两张代理（`-dc 24` 则 rc=0）。
⇒ **20 与 24 的差不是我能定的**：抬默认到手册 §3 的 24、或走方向 B 让分区器再切（会动树结构），
两条都归用户。

**7. validator（实测）**：`longxiang_guard512` 头 `0 ERROR / 121 WARN / 0 INFO`，
递归 `issues`+`causes` = **1514 WARNING / 837 INFO / 0 ERROR、ERROR 文件数 0**，
与折叠前 `longxiang_final512` 的头三列逐项相同。
> **口径更正（2026-10-22）**：上面那组 1514/837 是"所有带 severity 的节点"，其中
> **242 个是带 `causes` 的结构节点**（121 个告警文件 ×2 层），真叶 = **1272 WARNING / 837 INFO / 0 ERROR**。
> 尺子 `E:/tmp/val_leaf.js` 已改为"只数没有 causes 的节点"，并用已知答案的 `lugouqiao_v17`
> 复算对齐三个历史读数（真叶 ERROR **195**、告警文件 **691**、错误文件 **26**，均与文档记录逐项相同）。
> ERROR=0 这条结论不受口径影响；`lx_p_a`（A-52 清零后的当前二进制，默认 `-dc 24 -ptw 512`）
> 真叶读数 = **1272 / 837 / 0**，与上面两份折叠期产物逐项相同。

**8. 视觉验收（本轮做了，但覆盖不完整）**
无头 Edge + Cesium，`scripts/visual_capture.sh`，`far-to-near` 路径 4 帧采样，
对照 = **同码不折叠的暂存产物**（`longxiang_abnofold/.wb-staging`）。逐像素差
（`sharp` raw 缓冲，1280×720）：

| 机位 | coverage 像素（两侧） | 平均绝对差 | 最大差 | >8/255 的通道占比 |
|---|---|---|---|---|
| `dist_30.00R`（纯代理 LOD） | 0 / 0 | **0.000** | **0** | 0.000 % |
| `dist_11.42R` | 3 857 / 3 857 | 0.007 | 5 | 0.000 % |
| `dist_3.15R` | 57 430 / 57 430 | 0.096 | 5 | 0.000 % |

最大通道差 5/255、没有任何一处超过 8，且几何占比像素两侧逐值相同 ⇒ 与"折叠只挪坐标、
差异来自图集页重新归置后的 KTX2 再编码"一致，**未见错位采样该有的大块错纹理**。
**未覆盖（别把这条当成全量视觉验收）**：①该资产在这三个机位下 `chromaFraction ≈ 0.0002`，
画面近乎灰度，纹理判据灵敏度低；~~②折叠侧第 4 帧（`dist_1.20R` 近景）因超时未截到，
所以**折叠最相关的近景 LOD 边界没有像素级证据**~~ ⇒ **2026-10-22 已补**（见下）；
③发布资产 `lugouqiao` 的视觉验收由用户裁定**停做**（A-54 走明确失败）。

> **2026-10-22 复测（缺口 ② 闭合 + 一把尺子缺陷）**
>
> 上一段表的对照是**暂存产物**、且与折叠侧不同二进制档，匹配度不够。本轮改用
> **同二进制、同 `-dc 100`、只差折叠入口**的一对（`lx_fold_m100` / `lx_nofold_m100`，
> `maxMaterialsPerTile` 两侧都 100，只为让不折叠侧过材质闸）：
> `dist_1.20R` 平均 0.0564 / 最大 **10** / `>8/255` 通道 0.0033 % / `>16/255` **0 条**，
> coverage 285 856 / 285 856、`projPx` 519.6 / 519.6 逐值相同 ⇒ **近景 LOD 边界上折叠视觉中性**。
> 发布产物 `lx_p_a`（默认 `-dc 24`）对同窗重截的 `longxiang_guard512`：近景平均 0.0230 /
> 最大 **5** / `>8/255` 通道 **0 条** ⇒ A-52 清零与其后的改动无视觉变化。
>
> **两条口径更正**：
> ① 上表 `dist_30.00R` 的"差 0"是**两个未加载的洞相减**（两侧 coverage=0、`settleIters=12`），
> 不是证据；本轮 `lx_foldm100_near` 该帧同样是洞，而同窗的 `lx_p_a`/`lx_nofold_m100` 有 535 px。
> ② **跨时间窗的绝对亮度不可比**：同一份 `longxiang_guard512` 产物，10-21 16:35 截的
> `silLumAvg` = 141.8 / 138.0 / 131.8，10-22 09:39 重截 = 91.3 / 80.2，而两侧 coverage
> （3 857 / 57 430 / 285 856）、`projPx`、3.15R 可见瓦片清单（各 51 块，两侧 `only in` 均空）
> 逐项相同 ⇒ 差在渲染曝光不在产物。**逐像素判据只在同一时间窗内有效**；上表与其历史读数
> 各自内部成立，但不可跨窗做趋势。

**9. 回归**：`test_production` **263 / 0**（守卫落地后重编重跑），
`longxiang_guard512` 报告不变量与折叠前逐项相同（`tileCount` 129、`maxContentBytes`
2 535 648、`errorMaxMeters` 94.515 468 985 376 81、`nodesCertified` 2 / `nodesBoundLoose` 60）。
**（读数已过期）** A-52 清零 + `atlasCanvas` 回归用例落地后为 **277 / 0**（×2，rc 直接取）。

**未证实 / 待办**：~~近景与发布资产的视觉验收~~（近景已于 2026-10-22 补齐；`lugouqiao` 由用户裁定
停做，见 A-54）；**守卫的真实 CPU 代价（先前那组 2.4× 读数已撤回，
见第 5 条，需同时段交替 A/B 才能量）**；A-52 **已定位、修复并补了回归测试**（画布缝隙未初始化；
串行 `lx_z_a/b` 与默认并发 `lx_p_a/b` 两档各 246/246 逐字节相同，清零前的同尺子对照仍 FAIL；
`atlasCanvas` 用例的负对照实测：去掉清零 3/3 FAIL、装回 3/3 PASS）。

---

## 2026-10-28 轮：A-57 / A-59 落地与结案（代码在本轮工作区，**未提交**）

本轮范围由用户裁定：**A-57 + A-59 + 回归验证**；手册 §6/§7/§8/§9/§11-12 的审计执行推到后续轮。
所有实测都在本轮同一二进制上做（`Release/model23dtiles.exe` 与 `Release/test_production.exe`，
2026-10-28 11:48 重编），日志与产物一律落在仓库外新目录，不复用旧产物冒充新结果。

### A-57 原始 OBJ 面索引预检　**FIXED（未提交，2026-10-28 实测 + 变异对照）**

**1. 这条为什么必须存在（不是"再加一道校验"）**

A-56/Q2 那道守卫跑在 `SourceModel::build` 里，也就是**读模之后**。而 `osgdb_obj` 交付的不是
文件里的 `f` 索引，而是"按引用把 `v` 坐标逐个拷进几何自己的坐标数组"：文件里写第 N+1 条时，
它照样从自己的数组里取一个位置交给几何，于是**送进守卫的下标恒为本地 0..N-1 的合法值**。
守卫看不见，非有限性检查也看不见（错位窗口取出来的三个 float 数学上有限），`selfValidate` 同样
放行。唯一外露的指纹是**产物顶点数 > 源 `v` 数**，多出来的那颗是源 float 流的错位窗口
（A-56 探针实测形如 `-15.3588,-3.0000,13.6699` ← `(-15.358791, 13.669859, 3.0)`）。
证据：`F:\wb_stage0_ffbb3fe\20261026T092706\A56_ROOT_CAUSE.md`。
⇒ 拦截点只能在**文本层、reader 之前**。

**2. 判据（`ObjIntegrityPrecheck.h:73-80` 为准）**

三个索引空间（`v` / `vt` / `vn`）各自独立计数，一处越界不连坐其他空间。

| `why` | 含义 | 处置 |
|---|---|---|
| `beyond-total` | 引用 > 读完全文后该空间的总条数 —— **A-56/A-57 的确切形态** | 硬失败 |
| `index-zero` | OBJ 是 1 基（负数是相对引用），`0` 不是合法引用 | 硬失败 |
| `negative-underflow` | 相对引用回溯到第 1 条之前（`count + raw + 1 < 1`） | 硬失败 |
| `not-integer` | token 不是整数（`3abc` 不是"3"） | 硬失败 |
| `index-missing` | `f /2 …`：第一个 `/` 之前没有顶点索引 | 硬失败 |
| `forward-reference` | 引用 > 到此为止的条数但 ≤ 总数：导出器常见写法 | **只警告**，计入 `forwardRefs` |
| `pending-overflow` | 挂起表（>4096 条）溢出，判定不完整 | 按硬失败处理，不静默丢弃 |

单遍正向扫描分不开"合法前向引用"和"真越界"，所以正索引引用先挂起、EOF 再拿总数定性；
负（相对）索引当场可判。64 KB 缓冲顺序读，不整文件 split，不重实现转换器。

**边界（如实声明，不做超出能力的宣称）**：只扫顶层 `-i *.obj` 文本；材质、分组语义、曲线曲面、
经 `mtllib` / `.osg` 嵌套引用的子文件一概不管。**它是"引用越界"这一类确定性输入缺陷的拦截器，
不是 OBJ 合法性验证器。**

**3. 档位（用户裁定：硬拒 + 开关降级）**

`-objcheck reject|warn|off`，默认 **reject**；档位取值非法立刻 `rc=4`（不静默按默认跑——
"以为开了 reject 实际跑成 off"是这类开关最坏的失效方式）。
硬拒点选在 `main.cpp:574-606`：此刻 `osgDB::makeDirectory(output)`（`:736`）还没执行、模型还没读
⇒ **硬拒天然零产出**，不需要额外清理代码。

**4. 实测（本轮，全部同一二进制）**

*格式专项单元测试*（`tests/production/main.cpp::testObjIntegrityPrecheck`，文件头已写明
**不替代真实 FBX 端到端回归**）：14 例 / **55 条断言全过**；全量套件 **passed=383 failed=0**，
`rc=0`（rc 直接取，不经管道）。

*尺子有效性 —— 变异对照*：把 `ObjIntegrityPrecheck.cpp:351` 的 `if (p.resolved > total)` 短路成
`false &&`，重编重跑同一套断言 ⇒ **passed=43 failed=12、rc=1**（12 条 FAIL 覆盖 beyond-total 判定、
坏面计数、消息文本、以及"计数走进 conversion-report"那三条）；改回后文件与改动前 `diff` 为空，
重编重跑回到 55/0。**没有这一步，55/0 不能算证据。**

*CLI 三档 + 边界，退出码直接取*：

| 场景 | rc | 目标目录里 | 报告 |
|---|---|---|---|
| `bad_index.obj` 默认档（不带 `-objcheck`） | **7** | 只有 `run.log`（工具没建目录） | 无 |
| 同上，显式 `-objcheck reject` | **7** | 只有 `run.log` | 无 |
| `bad_vt.obj`（`vt` 引用第 9 条、只有 2 条） | **7** | 只有 `run.log` | 无 |
| `-objcheck warn` | 0 | 完整数据集 | `obj_precheck` stage + `objPrecheckFaces=1`/`CheckedRefs=3`/`BadFaces=1`/`Violations=1`/`ForwardRefs=0` + 2 条 warning（含 `beyond-total` 逐行文本） |
| `-objcheck off` | 0 | 完整数据集 | **无** `obj_precheck` stage、**无**任何 `objPrecheck*` 计数 |
| `clean_index.obj`（默认档） | 0 | 完整数据集 | stage 在，`Faces=1`/`CheckedRefs=3`/`Violations=0` —— **0 但有非零工作量，不像"没跑"** |
| `-objcheck nonsense` | **4** | — | — |
| `-i` 指向不存在的 `.obj` | **2** | — | 与"读模失败"同一口径 |

*同二进制、同夹具、同时段的成对矩阵*（`-objcheck off` vs 默认 reject，12 个 `test_assets/`）：
**12/12 rc 一致（全 0）**、`tileset=yes` 全同、**`outputBytes` 逐对相同**（如 t03 `6 779 848` /
t04 `10 012 221 9` / t12 `511 601`）⇒ 预检只读、不改产物；且**当前 11 个 OBJ 夹具无一被拒**，
这是 10-26 Q1 夹具修复的独立复证。t05 是 `.osg` ⇒ 无 `objPrecheck*` 计数（`ran=false`）。
引用扫描量：t03 `1 207 458`、t04 `129 600`、t12 `62 658`、t01 `27 378`、t02 `4 956`、t06 `21 708`、
t07 `17 304`、t08 `6 480`、t09 `4 428`、t10 `4 320`、t11 `16 416`。

**代价（实测，不是推断）**：`obj_precheck` stage 秒数 —— t03（9.43 MB）**1.657 s**、t04 0.258 s、
t12 0.085 s、t01 0.032 s、其余 ≤0.06 s。吞吐约 **5.7 MB/s**（单遍 + 逐 token 解析，含每行
`std::string` 分配）。t03 那轮 `totalSeconds` 35.53 s ⇒ **约 4.7 %**；壁钟两次读数
（36.58 vs 39.10 s）落在同一量级，但壁钟含噪声，**归因只看 stage 秒**。
> **两个分母不许混用**：`totalSeconds` 只覆盖 `ProductionPipeline::run()`（`ProductionPipeline.cpp:189`），
> 而 `stages` 求和（t03 = **1245.94 s**）含每瓦片并行条目 ⇒ 同一笔 1.657 s 对后者只有 **0.13 %**。
> 更要紧的是预检实际发生在 `run()` 计时窗**之前**（`main.cpp:574-606` 在 `readNodeFile` 前），
> 所以"对 `totalSeconds` 的 4.7 %"是**开销指示**，不是同一时间轴上的除法。

*产物级复验*：官方 `3d-tiles-validator` 对 reject 档 12 套产物 + `c2_naruto_a59` 共 **13 份**
`*.validator.json`（`F:/wb_a57_a59/validation/` 里现在是 14 个文件，多出的那个是下面那条
**尺子阳性对照**，不是产物报告）。

> **尺子失效与重测（本轮踩到，必须写下来）**：第一次统计叶层 ERROR 用的是
> `grep -o '"severity":"ERROR"'`，报告是**缩进 JSON**（`"severity": "ERROR"`，冒号后有空格），
> 这条模式**永远匹配不上** ⇒ 得到"0 条"。0 恰好也是表头 `numErrors` 的读数，所以错判没有暴露。
> 发现契机是同一把尺子数 WARNING 得到 0、而节点里明明白白写着 `"severity": "WARNING"`。
> **阳性对照**（`F:/wb_a57_a59/validation/_rulercontrol_missinggeometricerror.validator.json`，
> 故意给一个没有 `geometricError` 的 tileset）：紧凑模式 **0 命中**、容忍空格模式 **2 命中**
> 且表头 `numErrors=2` ⇒ 尺子本身修好并被钉住。
> **重测后的真实叶层读数**（`grep -o '"severity": *"ERROR"'`）：**ERROR 0 条**、WARNING **8 条**
> （`t12_big_texture` 4 + `c2_naruto_a59` 4，全在 `image/ktx2` 的 mimeType 与"图集未识别"那一族，
> 与历史一致）、INFO **766 条**。表头 `numWarnings` 各报 1，与叶层 4 **不成比例** —— 正是
> `scripts/run_validator.sh` 头注释里那条口径。结论（0 ERROR）不变，但**换成了一把活的尺子**。

*确定性*：t03、t11 各两次同参数重跑，产物分别 **7/7、15/15 个文件 sha256 逐字节相同**。

**5. 未证实 / 限制**

- **本轮没有任何真实 `.obj` 资产可跑**（审计资产树里只有 FBX/OSG）。OBJ 侧覆盖 =
  合成夹具 + 仓库 `test_assets/`，真实 OBJ 资产端到端 **未执行**。
- **分组（`g`/`o`/`usemtl`）局部重置的负索引语义未证实**：`osgdb_obj` 源码不在树内，无法核对它
  的相对引用是"全文累计"还是"分组内累计"。预检取**宽松的全文累计口径**，因此只会漏报不会误报；
  真出现分组局部语义的资产时，`negative-underflow` 可能判不出来 —— 已记为已知口径限制。
- 前向引用只警告不拒：这是设计选择（导出器常见），不是遗漏。
- `warn` 档放行的越界**不会**被下游任何一道守卫回收（reader 已吸收），报告里那条 warning 是唯一
  记账点 —— 已在 `ProductionPipeline.cpp:1495-1521` 写明措辞。

### A-59 UTF-8 路径 vs `std::filesystem` 的 ANSI 解码　**FIXED（未提交，2026-10-28 实测）**

**机理链条**（这一条以前被记在 A-58 头上，本轮更正）：

1. `main.cpp:551-555` 在 **Release** 下把 `-i`/`-o` 从当前代码页显式转成 UTF-8；
2. 项目内部统一按 UTF-8 传递路径字符串；
3. 但 `std::filesystem::path` / `std::ofstream` / `std::ifstream` 的**窄字符入口**会把同一个
   `std::string` 按 **C locale 代码页**再解一次 —— 两条路解出**不同**的路径；
4. `osgDB` 自己走宽字符，所以**模型读得动**，症状只出现在"我们自己碰文件系统"的地方：
   `file_size(sourceFile)` 失败 ⇒ A-58 的 `inputBytes` 恒 0 + `cannot stat` 警告。
   本轮审计的真实资产**无一不在中文目录下**，所以每条真实资产都中。

**修法**：`Utils::utf8ToPath()`（`Utils.h:302` / `Utils.cpp`）—— Windows 用
`osgDB::convertUTF8toUTF16` 解成 `wchar_t`（`path::value_type` 本就是 `wchar_t`），POSIX 用
`std::filesystem::u8path`。站点按用户裁定**一次改全**：`ProductionPipeline.cpp` **14 处**
（`:622`/`:660` `ifstream`、`:1029`/`:1325` `remove`、`:1414` `file_size`、`:1813`/`:1846`/`:1887`
`ofstream`、`:2015`/`:2049` `ifstream`、`:2453` `path` 构造、`:2500` `recursive_directory_iterator`、
`:2526` `path`、`:2536` `directory_iterator`）+ `main.cpp:811-814`/`:835-838` 两块（纹理缓存清理，
每块 3 个站点）+ `ObjIntegrityPrecheck.cpp:468`（新代码从一开始就走 `utf8ToPath`）。
`grep -c utf8ToPath src/3dtiles/production/ProductionPipeline.cpp` = **14**（本轮实测，不是估算）。

**实测**：

| 读数 | `c1_naruto_a58`（改前） | `c2_naruto_a59`（改后，同参数） |
|---|---|---|
| `inputBytes` | **0** | **29 205 344**（与外部 `stat` 逐项相等） |
| `cannot stat` 警告 | 1 条 | **0 条**（`warnings` 为空） |
| `outputBytes` | 190 915 | 190 915（**不变**） |
| `tileCount` / `maxContentBytes` | 1 / 188 832 | 1 / 188 832（**不变**） |
| 产物 sha256（`t0.b3dm`、`basecolor.ktx2`、`tileset.json`、`feature-manifest.json`） | — | **4/4 逐字节相同** |

中文**输入 + 输出双端**的端到端（`F:/wb_a57_a59/fixcjk/中文资产_clean.obj` →
`F:/wb_a57_a59/输出目录/子目录/out_a`）：`rc=0`，`inputFile`/`outputDir` 按 UTF-8 原样回读，
`inputBytes=32`、`outputBytes=2 542`，报告/清单/tileset 都写进了中文目录 ⇒ `ofstream`、
`directory_iterator`、`file_size`、`selfValidate` 的 tileset 回读四类站点同时通过。
越界那份中文命名资产在同目录被 reject 档判 **rc=7** 且**输出目录根本没被创建** ——
"硬拒零产出"与路径修复不互相掩盖。
单测第 13 例把这条钉死：CJK 临时目录里建夹具、`readable=true` 且 `bytes == file_size`。
改回窄字符 `path` 就必然变成 `readable=false`，不会静默通过。

**未证实 / 限制**：

- **失败路径上的三处站点未被 CJK 端到端触达**：`ProductionPipeline.cpp:1029`/`:1325` 的
  `filesystem::remove`、`main.cpp:811-814`/`:835-838` 的空纹理目录清理（这两块只在 `return 3`
  分支上）。本轮三次强行制造失败（`-tri 1`、`-tri 2`、`-dc 1`）**全部 rc=0**（预算闸按 A-31 只降级
  不判死），没走上去 ⇒ 记为**未证实**，不说"已覆盖"。同一函数内成功路径已证明 `utf8ToPath`
  解码本身正确。
- **Debug 构建不做 UTF-8 转换**（`#ifndef NDEBUG` 把 `:553-554` 整段跳掉），所以 A-59 的修复
  **只在 Release 下成立**；Debug 下中文路径依旧会解错（本轮未实测 Debug）。这是既有编码策略的
  分叉，不是本轮引入的，但没有它 A-59 无法在 Release 里复现 ⇒ 留给后续轮决定要不要把 CLI 层的
  编码统一。
- `main.cpp:736` 建的 `textures` 目录在**成功**跑里始终留下（可能是空目录），本轮实测 ASCII 与
  CJK 行为一致，与 A-59 无关（清理分支只管失败路径）。

### A-58 报告身份字段：本轮补的实测　**FIXED（未提交，代码在 `8abd48c` 之后的工作区）**

A-58 的代码（身份七格赋值、`outputBytes` 口径固定、投影失败 `exit(0)` → `rc=6`）在 A-57/A-59
之前已写进工作区，但**产物级复验当时被 A-59 挡住**（真实资产 `inputBytes` 恒 0）。本轮补齐：

- **矩阵 12/12 的 `inputBytes` 等于外部 `stat` 值**：t01 `188 180`、t03 `9 429 501`、
  t04 `2 521 028`、t11 `297 324`、t12 `441 961`（与 `ls -l` 逐项相同）。
- `outputBytes`/`processWallSeconds`/`processCpuSeconds`/`peakRssBytes` 全部非零（例：
  t04 `10 012 221 9` B / `wall` / `cpu` / `peakRss`；Naruto c2 `190 915` B / 7.88 s /
  8.61 s / 256 606 208 B）。
- **投影失败退出码**：`-epsg 999999` ⇒ **`rc=6`**，输出目录里除本人预建的 `run.log` 外
  **工具一个文件都没落**（以前这条路走 `exit(0)`，CI 与回归脚本一律读成成功）。
  日志同时给出可解释失败：`proj_create: crs not found: EPSG:999999` +
  `Error: cannot build transformation EPSG:999999 -> EPSG:4326`。
- `ObjPrecheckResult.ran=false`（非 OBJ 输入、`-objcheck off`）时报告里**不出现**任何
  `obj_precheck` stage / `objPrecheck*` 计数 —— 与 A-58 的"不可用要标不可用，不许用 0 冒充
  测量"是同一条纪律（t05 `.osg` 实测确认）。

