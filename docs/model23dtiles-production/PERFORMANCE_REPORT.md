# PERFORMANCE_REPORT.md — 性能报告

机器：Windows 10 19045 / x64 / 127 GiB 内存，TBB 8 线程上限。
测量方式：`conversion-report.json`（管线内计时 + `GetProcessMemoryInfo` 峰值 RSS）
+ bash/node 采集器（进程墙钟与产物体积）。
§1/§4 复测于 2026-10-19（HEAD `ee6499e`，产物
`F:\wb_3dtiles_out\matrix_texless\`、`F:\wb_3dtiles_out\longxiang_covfix\`）；
§5 是 2026-10-19 无头截图会话（`docs/visual-acceptance/runs/`，A-33/34/35 之前的
二进制产物）；§2/§3 保留 2026-10-16 的实测并注明。
**2026-10-20 增补**：§1 的 `lugouqiao_v17` 表、§2 的 v17 阶段行、§6.4 的峰值终值，
以及 §1 两处 `du -sb` 目录体积（v17 / v15 同法复测）。
本报告里凡"输入/输出字节"都**不来自 report 字段**——那四项恒空（STATUS **B-10**），
输入大小用 `stat`、输出体积用 `du -sb`，与 `run_matrix.py` 的外部采集同一口径。

## 1. 回归矩阵耗时、内存与产物（2026-10-19 复测）

| 资产 | 耗时 s | 峰值 RSS MiB | 输出 MB（tiles+tex+json） | 对比 2026-10-16 耗时 |
|---|---|---|---|---|
| t03_giant_single_mesh | 117.94 | 148.5 | 6.47 | 113.13 → 117.94（+4 %，代理改判 geo 字节后多压几轮） |
| t04_many_objects | 30.85 | 146.8 | 9.11 | 67.53 → 30.85（-54 %） |
| t06_nonuniform | 2.98 | 130.4 | 1.14 | 14.71 → 2.98（-80 %） |
| t11_sparse_outlier | 1.92 | 84.6 | 0.69 | 7.67 → 1.92 |
| t08_big_coords | 0.88 | 59.2 | 0.22 | 3.20 → 0.88 |
| t10_material_heavy | 0.65 | 56.7 | 0.15 | 3.19 → 0.65 |
| t12_big_texture | 2.09 | 232.4 | 0.81 | 3.34 → 2.09 |
| t05_instances | 0.02 | 18.1 | 0.21 | 2.05 → 0.02 |
| t02_thin_features | 0.38 | 32.5 | 0.17 | 1.94 → 0.38 |
| t01_plane_curved | 0.02 | 16.3 | 0.08 | 1.53 → 0.02 |
| t09_cross_tile_object | 0.91 | 55.1 | 0.15 | 3.09 → 0.91 |
| t07_zero_axis | 0.02 | 16.3 | 0.06 | 1.23 → 0.02 |

> 耗时口径提醒：旧列数字来自 `run_matrix.py` 的墙钟（含进程启动与 OSG/DLL 加载，
> 每个资产 ~1.2 s 固定开销）；本轮矩阵 11 个轻资产的 `totalSeconds` 都 < 3 s，
> 差值主要是那笔启动开销，不是管线提速。**只有 t03/t04/t06 这类
> `totalSeconds` 本身在几十秒以上的资产，跨轮对比才有意义**：
> t04 67.53 s→30.85 s、t06 14.71 s→2.98 s 是真实收益（A-26 让合并后的多
> primitiveSet 几何第一次真正被简化，此前是整段 return 后走别的兜底路径）。

### 真实 FBX（同机器同命令，唯一变量是管线）

| 指标 | longxiang 2026-10-16（bfa43a0） | longxiang 2026-10-19（ee6499e） |
|---|---|---|
| 管线耗时 s | 629.9 | **93.4** |
| 峰值 RSS MiB | 650 | **2 401**（回退，见 §6.4） |
| tiles / 最大内容瓦片 | 69 / 7.91 MiB | 129 / 2.17 MiB |
| 产物体积 | — | 96.3 MB（tiles 81.9 + KTX2 14.1 + json 0.3） |

耗时降 6.7 倍的直接原因即 A-30：远景代理不再因为"纹理字节"被拒绝，
聚合 fallback 不必把 5 个 cell 尺寸 × 2 种 merge 模式全部白跑一遍。
内存上升 3.7 倍尚未定位到单一分配点，本机 127 GiB 下无实际风险，
但生产并发预算必须按 2.4 GiB/任务规划。

### 真实 FBX 最大一档：`lugouqiao_v17`（2026-10-20 出盘；二进制 = A-41 相对帧 exe 23:53 + A-42 插件 `osgdb_gltf.dll` 00:16）

| 指标 | 读数 | 来源 |
|---|---|---|
| 输入 | `20240529卢沟桥分洪枢纽.fbx` 335 435 216 B | `stat`（report 的 `inputFile/inputBytes` 恒空，见 STATUS B-10） |
| 管线耗时 | **28 485.37 s**（00:35→08:35，7.9 h） | `report.totalSeconds` |
| 阶段分布 | `proxy_encode_error` **28 471.08（99.95 %）**、`seam_registry` 9.58、`import_normalize` 2.71、`partition` 1.97 | `report.stages` |
| 峰值 RSS | **9 830 379 520 B = 9.15 GiB** | `report.peakRssBytes` |
| 瓦片 | `tileCount=1059`、1057 个内容文件（931 b3dm + 126 i3dm）、`maxDepth=18` | report + `ls` |
| 最大内容瓦片 | 3 237 360 B（3.09 MiB，硬限 8 MiB） | `report.maxContentBytes` |
| 最大三角形/瓦片 | 119 840（硬限 120 000；`glb_prims.js` 的 A-33 实例展开口径实测同为 119 840，读数见 VALIDATION_REPORT §6.2 的"v17 电池读数"表） | report + `glb_prims.js` |
| 产物体积 | `du -sb` **1 006 423 166 B**（2026-10-20 实测：tiles 867 628 648 + textures 133 554 986 = 471 个 `.ktx2` + tileset.json 1 544 575 + feature-manifest.json 3 032 490 + conversion-report.json 662 467）；**同法复测 `lugouqiao_v15` = 1 012 679 026 B**（tiles 864 159 856 / 1057 个文件，textures 143 982 629 / **520** 个文件），v17/v15 = **0.994**（净差 −6.26 MB，两个方向相反：纹理少 10.43 MB、瓦片反而多 3.47 MB）。**这 49 个纹理文件为什么少，本轮没有查**，不当成收益记录（若下一轮要按"体积下降"汇报，先归因）。对同一 335 435 216 B 输入的倍率 **3.000 / 3.019**。`run_matrix.py` 那列"输出 MB"量的是矩阵小资产的目录体积，**与这两个数不同源**，不要混读 | 实测 |
| 误差统计 | `errorMax=8 857.99 m`、`errorMean=117.70`、`errorP95=3 502.99`、`tilesetGeometricError=8 857.99`；`nodesCertified=450`、`nodesBoundLoose=332`。**口径注（2026-10-20 下午）**：这一行是**修复前**的读数，`MeshBVH` 叶子索引缺陷（见 §2.1）会把最近距离放大到最多 3.7 倍，因此 v17 及更早的所有 `errorMax/geometricError/p95` 与 `nodesCertified` **都不可与修复后的产物互相比较** | report |
| 454 条 warning | 与 v15 **逐类相同**：362× `far LOD proxy built without textures to fit budget` + 92× `seam vertices preserved n/m`，`errors=[]` | report |

两点如实标注：**① 8 857.99 m 的 `errorMax` 没有被任何发布闸拦下**（手册 §9 只要求误差
是实测保守上界，不卡绝对值；t08 大坐标资产同样出过 `errMax=1.06e7`）。它的含义是"某个
代理相对源表面的最坏双向偏差接近 9 km"，对这份 4.3 km 跨度的资产来说是**根级粗代理**的量级，
运行时靠 SSE 提前细化；**是否视觉可接受属于未做的视觉验收，本报告不下结论**。
**② 单任务 9.15 GiB / 7.9 h 意味着 335 MB 档不能按"多任务并发"排产**，
本轮没有测并发下的 RSS 叠加，只测了单跑。

## 2. 阶段耗时分布

以最重的资产为例（conversion-report.json `stages`）：

- t03（134k 三角形单体网格）117.94 s = `import_normalize` 0.02 + `seam_registry` 0.10
  + `partition` 0.05 + **`proxy_encode_error` 117.78（99.9 %）**；
- t04（127 对象 572 三角形）30.85 s：`proxy_encode_error` 30.74（99.6 %），
  其余阶段合计 < 0.1 s；
- longxiang.fbx 93.43 s：`proxy_encode_error` 93.15（99.7 %）。
- **`lugouqiao_v17`（335 MB，本轮最大）28 485.37 s** = `proxy_encode_error` **28 471.08
  （99.95 %）** + `seam_registry` 9.58 + `import_normalize` 2.71 + `partition` 1.97。
  同输入的 **v15 是 21 621.13 s**（`proxy_encode_error` 21 610.47），两份产物的
  `tileCount=1059` / `contentTileCount=1057` / `warnings=454` / 峰值 RSS 9.17 vs 9.16 GiB
  **完全同量级**，即 **+31.7 % 的时间没有换来任何结构变化**，成本全部落在同一个阶段里。
  代码侧能确定的是并行形状：只有 `processTileHeavy` 走 `std::async`
  （`ProductionPipeline.cpp:903-907`，滑动窗口 `jobs*2`、默认 `jobs=8`，见 `:883-886`），
  `finalizeTileFromWork`→`writeContent`（含 `TileFeatureMapper::addTile` 与 report 聚合）
  **为了与串行版逐字节一致而留在主线程串行**（`:909-913` 的注释就是这个约束）。
  **本轮不给 CPU 利用率结论**：上一次会话观测到的"平均约 2.25 / 24 核"没有留档，
  不能当证据；要定位这 6 千秒去哪了，需要在 `proxy_encode_error` 内部加子阶段计时
  （简化 / 聚合 fallback / 收集 / 双向误差评估各自的墙钟），下一轮再做。
  **状态更新（2026-10-20）**：工作区里已有一版**未提交**的这类仪表
  （`ProductionPipeline.h` 的 `_micro*` 成员 + `ProxyMeshBuilder` 的 `ProxyResult::Metrics`
  + `processTileHeavy` 里的 `Timer`），但 `ConversionReport` 那侧**还没有对应字段**
  （`grep -n micro src/3dtiles/production/ProductionProfile.cpp` 为空），所以现在构建了
  也读不到数；同一份未提交改动还动了误差评估器，
  见 STATUS 的"**工作区里那份未提交的改动**"一节。
  > **上面"还没有对应字段 / 读不到数"这句于 2026-10-20 下午作废**：`micro_*` 已经通过
  > `ProductionPipeline.cpp:1219-1226` 的 `_report.addStage(...)` 进到了 `stages`，
  > `lugouqiao_v19` 是首份读数（见 §2.2）。当时判断错的直接后果是"6 千秒去哪了"
  > 这件事被推迟了一轮；现在的口径是**已归属 8 244.8 s、仍有 3 923.8 s（32 %）无归属**，
  > 缺口在 `materialize`，见 AUDIT **A-47**。

### 2.1 #6 乱序收割 + #7 节点内并行：同时段 CPU 秒 A/B（2026-10-20 下午，**未提交**）

上面那段"并行形状"已随本轮改动，先把口径钉住：
`processTileHeavy` 的滑动窗口现在**乱序收割**（任务按子树三角形数降序投递，
任一 job 完成即进 `ready`，写出仍严格按瓦片 id），窗口仍是 `jobs×2`、`jobs=8`，
且新增 `inflight + ready <= 窗口` 的合并限流以保持内存中性；
`finalizeTileFromWork`→`writeContent` **仍然主线程串行**（未闭合项见 STATUS 未闭合一节第 10 条）。
误差评估器内部新增第二层并行：初始采样按 65 536 三角形分块，
线程数 = `hardware_concurrency / 活跃瓦片数`（`TileTaskScope` 计数），
`WB_EVAL_THREADS=0/1` 关闭。**并行段只算距离数组、结果按原序串行入堆**，
所以 off/adp 的验收字段实测逐字相同（下表最后一列），不是"牺牲确定性换速度"。

计时用**进程 CPU 秒**为主指标（`F:/wb_3dtiles_out/cpu_ab.sh`，原始读数
`F:/wb_3dtiles_out/cpuab.log`），wall 只作同轮参考——本机 wall 受外部负载漂移 2~3 倍，
跨会话不可比；脚本里 `mode=adp` 必须删掉环境变量而不是设空串（空串 `atoi`→0 会静默关并行）。

| 资产 | off CPU 秒 / wall | adp CPU 秒 / wall | wall 比值 | 验收字段是否变 |
|---|---|---|---|---|
| t03（7 瓦片） | 453.3 / 184.34 s；425.9 / 160.40 s | 460.0 / **112.54 s**；436.1 / **96.42 s** | **1.64× / 1.66×** | 否（`errMax=105.5606 loose=3`） |
| t04（127 瓦片） | 107.4 / 37.77 s；110.7 / 39.38 s | 105.1 / 34.66 s；107.2 / 35.64 s | 1.08× / 1.11× | 否（`errMax=17.3600 loose=63`） |
| longxiang.fbx（129 瓦片） | 606.4 / 144.46 s；637.8 / 146.53 s | 557.0 / 133.20 s；616.5 / 144.50 s | 1.08× / 1.01× | 否（`errMax=94.5155 cert=2 loose=60 warn=52`） |

**读法与不读的东西**：t03 的 1.6× 是在 CPU 秒持平的前提下拿到的（453→460、426→436），
即把工作摊到多核而非增加总工作量，也没出现超线程抢 CPU 导致 CPU 秒上升的退化；
t04/longxiang 只有 ~8 %，说明这两个资产的耗时不集中在"单节点初始采样"上。
**这份二进制已在同日 15:36 跑完 `lugouqiao_v19`**：28 485 s → **4 597.57 s（6.2×）**，
1~2 h 目标**达成**。t03 那个 1.66× 比值确实不能外推到 335 MB 档——实测比它好得多，
因为 straggler 效应在这一档最强（1057 瓦片、单瓦片三角形数大得多）。读数见 §2.2。

同一轮对照还查出误差侧缺陷并修掉（详见 STATUS 同名小节与 AUDIT **A-44 / A-45 / A-46**）：
**D1**（A-45）覆盖半径系数在采样密度被预算降级到"只剩顶点"时仍按 `L/3` 取，正确值是 `L/√3`，
HEAD 里就存在，阳性对照修复前精确失败（`upper=0.577350` = `L/3`，`L = √3`，vs 真值 ≥ 0.98
⇒ 旧口径是**把上界写小**）；
**D2**（A-44）`MeshBVH::buildRecursive` 重排的是 `order`、叶子查询读的是 `_tris[i]`，
置换从未落到数组上 → 最近距离只偏大不偏小（**没有**"上界写小"的违规），
实测最多放大 3.7 倍，历史所有误差字段因此虚高 1~3 个数量级。
两处修复的 12 资产矩阵复跑（`F:/wb_3dtiles_out/mx_bvh`，总 ≈148 s）：
215 瓦片中自身上界 **100 变小 / 0 变大 / 115 不变**，`upper<lower` 违例 0，
**b3dm/i3dm 内容文件改动 0**，`tileset_bounds.js` 12/12 干净，
官方 validator 12/12 **errors=0**。副作用是一条待裁定项：距离算对后
`nodesCertified` 在这 12 个小资产上变为 0（t04 63→0），因为 `CERTIFIED` 要求
"上界−下界 ≤ 1 mm 或 ≤ 上界 2 %"，而这个差在 40 万样本预算内数学上够不着。
**但"全线归零"不是普适结论**：`lugouqiao_v19` 仍有 **63** 个 `CERTIFIED`
（450→63，迁移明细见 §2.2），合格证掉多少是**资产相关**的。
**合法杠杆只有"加大 `maxSamplesPerNode`（纯烧时间）"或"接受诚实的 `BOUND_LOOSE`"**，
调 `toleranceMeters`/`certifyRatio` 属放宽验收，本轮没有动。
`test_production` 现在 **181/181**（含 `bvh`、`errorSoundness` 两组新用例及其阳性对照）。

热点分析结论不变：误差评估的采样密度（perTri 起始 7、自适应递减）是主要成本，
当前是正确性优先的实现；手册 9.4 的"评估成本控制"已实现自适应采样，
进一步优化空间在按屏幕重要性筛选拒绝评估（待做，见"未决"）。

## 2.2 `lugouqiao_v19` 实测 + 下一轮提速的杠杆排序（2026-10-20 下午）

**质量口径先钉住**（用户原话："不影响输出文件的质量（主要是 3dtiles 的性能）"）：
这里的质量 = **运行时 3D Tiles 行为**，具体是瓦片层级、`geometricError`/SSE 细化、
三角形与字节预算、纹理保真、格式合规。判定方式是逐字段对照 + 产物字节对照，
不是"看起来差不多"。

### v19 读数（335 MB 档，同机同轮）

| 项 | v17 | v19 |
|---|---|---|
| `totalSeconds` | 28 485.37（7.91 h） | **4 597.57（1.28 h）** = 6.2× |
| `peakRssBytes` | 9.15 GiB | **6.93 GiB** |
| 进程 CPU 秒（监视器累计，末样本 15:32:31） | 未采 | ≈ **12 168.6 s** / 4 549 s wall ⇒ **平均 2.7 核 / 24 核** |
| `errorMaxMeters` / `p95` / `mean` | 8 857.99 / 3 502.99 / 117.70 | 2 475.43 / 878.01 / 26.45（**修复后诚实读数，禁止与 v17 比大小**） |
| `nodesCertified` / `nodesBoundLoose` | 450 / 332 | **63 / 464**（迁移：CERT→LOOSE 387、CERT→CERT 63、LOOSE→LOOSE 15、EXACT_LEAF 466、NOT_EVALUATED 128） |
| 结构/硬限字段 | — | `tileCount`、`contentTileCount`、`instanceTileCount`、`maxDepth`、`maxTrianglesInTile`、`maxContentBytes`、`aggregateProxyCount`、`sourceFeatureCount`、warning 条数、`proxySkippedCount`、`featuresMissingFromLeaves`、`proxyDroppedFeatureCount` **逐值相同** |
| 逐瓦片负载 | — | 1 059 条记录里 `triangles`/`contentBytes`/`uri`/`refine`/`hasContent`/`isProxy`/`aggregate`/`aggregateSources`/`sourceFeatures` **差异 0**；只有 5 个误差字段变（上界 450 变小 / 15 变大 / 594 不变，`upper<lower` 违例 0） |

结论：**这一轮 6.2× 是"质量中性"的**——运行时细化行为与负载字节一个数都没动，
变的只有误差标注（且方向由 A-44/A-45 的机制证明解释）。

### `proxy_encode_error` 内部占比（v19 首份读数，`stages` 里的 `micro_*`）

| 条目 | 值 | 口径 |
|---|---|---|
| `proxy_encode_error`（阶段 wall） | 4 592.42 s | 占总 wall 99.9 %，与旧结论一致 |
| `micro_evaluate` | **7 510.16 s** | 跨线程累加 ⇒ 平均只吃到 **1.63 核**（8 job / 16 窗口） |
| `micro_finalize_encode` | 564.71 s | **主线程串行**（`writeContent` + 状态聚合 + seam 校验） |
| `micro_bvh_build` | 89.80 s | 在 `micro_evaluate` 之内 |
| `micro_proxy_simplify` | 44.59 s | |
| `micro_proxy_aggregate` | 25.55 s | |
| `micro_collect_triangles` | 10.02 s | |
| `micro_evaluate_samples` | 438 681 617 | **计数，不是秒**（见下面 L0 的仪表缺陷 ③） |
| `micro_aggregate_candidates` | 122 | 同上 |

三条从这张表直接读出来的事实：
1. **评估内核是绝对大头**（7 510 s = 已归属 worker 秒的 97 %），不是 IO、不是编码。
2. **机器没用完**：进程只用 2.7/24 核，`micro_*` 累加 8 244.8 s 对进程 CPU 秒 12 168.6 s
   ⇒ **≈3 924 s（32 %）无归属**。**这条已于 2026-10-21 闭合，但结论换了对象**：
   补了落盘仪表之后实测，无归属的不是
   `_model.materialize()`（它只值 0.09 CPU 秒），而是写 `.ktx2` 时 libktx 自起的
   basis 压缩线程（`threadCount = hardware_concurrency`，`LoadTextureKTX.cpp:630`）。
   详见 AUDIT A-47 的"闭合读数"段与下面 L5/L7 两行。
3. **读不出关键路径**：瓦片级 `micro_evaluateSec` 只累加、没进 `report.tiles`，
   所以现在无法回答"最后一个瓦片跑了多久"——而这恰好是 straggler 策略的上限。
   （②③同样在 2026-10-21 闭合：129/129 瓦片有 `timing.evaluateSec`，计数搬进 `counters`。）

### 杠杆排序（按"证据强度 × 上限 / 风险"，不是按好写）

| # | 杠杆 | 证据 | 上限 | 质量风险 | 判 |
|---|---|---|---|---|---|
| **L0** | 补三个仪表洞：materialize 计时、瓦片级 evaluate 秒进 `report.tiles`、两个计数搬出 `stages` | 上面第 2、3 条：32 % CPU 无归属、关键路径不可读 | 不提速，但**决定后面五条改哪** | 无（纯观测） | **已做（2026-10-21，未提交）**。判据①在 65 MB 档闭合到 **−0.8 %**，并且**换了结论**：无归属的 34.7 % 是 libktx basis 压缩线程，不是 `materialize`（0.09 s）。335 MB 档要用新二进制随 v20 复测。读数见 AUDIT A-47 |
| **L1** | `MeshBVH::distanceSquaredRecursive` 改成**近孩子优先**（现在固定先左后右，`GeometryErrorEvaluator.cpp:186-189`），配合进入三角形前的粗剪枝 | 最大单项 7 510 s；BVH 最近点查询按盒子距离降序访问是教科书级 1.5~3× | 若拿 2× ⇒ 少 ≈3 700 worker 秒，wall 至少受益同量级 | **可证明为零**：`best` 收敛更快只减少*被访问*的三角形；剪枝用的是合法下界，最小值对同一候选集不变，IEEE754 下 `min` 与顺序无关 ⇒ 逐位相同 | **已做（2026-10-21，未提交）**。产物中性靠"同网格两遍历逐位相等"单测（`bvhOrder`）证死；**提速没有证据**：同一资产、同一输入的 8 跑里`micro_evaluate_samples` **全是 48 963 414**（工作量完全相同），而 `micro_evaluate_cpu` 在**145.22 ~ 310.73 s** 之间跳（2.14×），且与 wall 同步跳（122 s↔436 s）——这个量测的是机器状态，不是代码。拿它做 A/B 主指标不成立，**2× 的收益要在 335 MB 档 + ≥3 次交错重复才有读数** |
| **L2** | 并发余量：`WB_JOBS` 从 8 往上抬、并把 `refill()` 的 `inflight+ready ≤ window`（`:916-921`）换成按字节的闸门 | 2.7/24 核、peak RSS 6.93 GiB / 127 GiB | 大（机器还有 21 核空着） | 内存峰值可测；不改算法 ⇒ 输出应逐位相同 | **必做但要与 L3 一起调**：`intraNodeThreads()` 现在是 `hardware_concurrency / activeTileTasks`（`:218-225`），16 个在制品时每瓦片只剩 1 线程，抬 job 会**饿死尾巴** |
| **L3** | 线程配额按剩余工作量分，而不是一律平摊；同时重估 `n*perTri >= 50000` 这个并行闸门（`:363`） | 平摊除法 + 闸门是"评估只吃到 1.63 核"的直接嫌疑，但**目前是嫌疑不是证明** | 中 | 无（改的是调度，不改口径） | L0 落地后用瓦片级读数判：如果 tail 瓦片的 evaluate 秒 ≫ 其它，就是它 |
| **L4** | 细化堆循环并行（现在只有初始采样块并行，`while` 取 priority 的那段仍是单线程） | 单瓦片内部串行 ⇒ 直接设 wall 下限 | 未知（要先有 L0 的瓦片级读数） | 可用"并行查询 + 按 key 顺序入堆"复现 #7 的逐位相同性质 | 排在 L1/L2 之后 |
| **L5** | `finalizeTileFromWork`→`writeContent` 并行（编码到各自 staging，提交仍严格按 id 升序） | 335 MB 档：主线程串行 wall 564.71 s；**65 MB 档新读数**：写出阶段 wall 126.06 s 占阶段总 wall 188.09 s 的 **67 %** | **上限是资产相关的**：335 MB 档 12 %（564.71/4 592.42，wall 对 wall），纹理/几何比高的输入远不止此 | 只要提交顺序不变，`_mapper.addTile`/report 聚合顺序/staging 文件名不变 | **别在 L7 之前做**：写出并行的第一层副作用是把 basis 压缩线程从"1 张图 × 24 线程"变成"N 张图 × 24 线程"，机器会立刻过载（见 L7）。335 MB 档的 12 % 上限仍然成立，但那是**这一个**资产的比例，不是普遍结论 |
| **L7** | 纹理落盘（libktx basis 压缩）的线程配额：`ktxBasisParams.threadCount` 现在等于 `hardware_concurrency`（24），每张图一套；顺带看纹理去重命中率（65 MB 档 121 个文件 / 129 个瓦片） | A-47 闭合读数：写出阶段调用线程 64.77 s，同窗口进程口径 **257.75 s** ⇒ **第三方线程 192.98 s = 全进程 CPU 的 35.6 %**，是所有 `micro_*` 都记不到的一块（补仪表前它与第 2 条那 34.7 % 无归属基本对上） | 未知——**这是本轮新发现的量，还没有 A/B**。上限的粗算：若 `threadCount` 从 24 降到与在制品数相称的量，wall 不一定变（压缩本身是 CPU-bound），但**并发预算才谈得上去做 L5** | **未验证（2026-10-22 同日更正：原"已实测为字节中性"撤回）**。撤回原因：`lx_ktx_t1` / `lx_ktx_t24`（10-22 10:03 / 10:08，脚本 `F:/wb_3dtiles_out/ktx_threads_ab.sh`，读数 `lx_ktx_t1.sha256` / `lx_ktx_t24.sha256` 保留）**不构成有效的线程配额对照** —— 当时部署的 `Release/osgdb_ktx.dll`（sha256 前缀 `1feb96f2…`，mtime 10-20 18:17）里没有 `WB_KTX2_THREADS` 这个 getenv：`git show HEAD:src/osgdb_ktx/LoadTextureKTX.cpp` 查不到、`git log -S'WB_KTX2_THREADS'` 也无结果 ⇒ 该变量从未进过任何提交，两跑执行的是同一份代码。于是"244/244 逐位相同"只等价于"同码两跑相同"，`threadCount` 会不会改 ETC1S 压缩结果**回到未知**。**新证据（knob 生效后，2026-10-22，两侧各 n=1）**：`F:/wb_probe/ktxA`（默认）vs `ktxB`（`WB_KTX2_THREADS=1`）都 rc=0、129 tiles、`errors=[]`；进程峰值线程数 **43 → 20**（已确认参数确实改变运行时线程数，属**初步证据**）；产物 `tiles/`+`textures/` 244 个文件逐位相同，连同 `tileset.json`/`feature-manifest.json` 共 **246 个非报告文件逐位相同**（每侧 247 个文件，唯一不同的是 `conversion-report.json`；把计时/RSS/时间戳字段剔除后两报告差异 **0 行**）；与 10-21 交付 `lx_p_a` 除报告外亦 246/246 相同。**仍待验证**：性能收益、≥3 次交替运行的可复现性、更大资产上的行为；wall 118.06 vs 213.77 s 各只有 1 个样本，**不得用来判快慢** | **335 MB 档的复测已被 A-54 挡死**（当前二进制在该档出不了盘：`lugouqiao_v20` 被 primitive 闸拒、`-dc 100` 又被材质闸拒，报告写在 `selfValidate` 之后 ⇒ 没有 `conversion-report.json` 可读）。⇒ 65 MB 档那一对（`lx_ktx_t1` / `lx_ktx_t24`）**既不是风险解除、也不是收益数字**，因为要对照的变量当时根本没进入可执行代码；要拿任何 L7 结论必须用**含 knob 的二进制**（10-22 11:41:33 起重建产物为 `1124d517…`）做 ≥3 次交替 A/B，且只能在能出盘的资产上做 |
| **L6** | BVH 建树比较器里质心重复计算（`:142-152` 每次比较重算两个质心） | `micro_bvh_build` 只有 89.80 s | ≤ 89.8 s（≈1.9 %） | 预计算同一表达式 ⇒ 逐位相同 | 顺手可带，**不值得单独跑一轮** |

### 明确不提的"提速"（都会动质量闸门）

- 调 `toleranceMeters` / `certifyRatio` / 降 `maxSamplesPerNode` / 降采样密度 /
  跳过"看起来不重要"的瓦片评估 —— 前两条是放宽验收（手册禁止、且归用户裁定），
  后三条直接改变 `geometricError`，也就是改变运行时细化行为。
- **§2 旧结论里"按屏幕重要性筛选拒绝评估"那条待做项就此关闭**：它本质是把某些瓦片的
  上界算小或不算，与"质量中性"冲突。要做得先作为**显式 profile 选项**并由用户裁定。

> **2026-10-22（同日撤回）：下面这对 `lx_ktx_t1` / `lx_ktx_t24` 是无效对照 —— 原判"L7 前置问题已实测回答：不会"作废，L7 回到未验证**
>
> **作废原因**：脚本 `F:/wb_3dtiles_out/ktx_threads_ab.sh`（09:57）确实在环境里给了 `WB_KTX2_THREADS=1` 与 `=24`
> 两个值（产物 10:03 / 10:08 落盘），但当时加载的 `Release/osgdb_ktx.dll`（sha256 前缀 `1feb96f2…`，
> mtime 10-20 18:17）**里没有这个 getenv**：对该 DLL 搜字符串 0 命中（同文件正对照
> `ktxTexture2_CompressBasisEx` 1 命中，排除编码误检），且 `git show HEAD:src/osgdb_ktx/LoadTextureKTX.cpp`
> 查不到该变量、`git log -S'WB_KTX2_THREADS'` 也无结果 ⇒ 它从未进过任何提交。
> **两跑执行的是同一份代码**，所以这是"同码两跑"，不是"1 线程 vs 24 线程"。
> 下面原始读数与推论**保留不删**，仅作历史记录，其结论一律不得引用。
>
> | 跑 | rc | wall s | 进程 CPU s | 纹理导出窗口进程 CPU s | `micro_finalize_encode` s | 产物 sha256 |
> |---|---|---|---|---|---|---|
> | `lx_ktx_t1`（标称 basis 1 线程 —— **实际未生效**） | 0 | 319.61 | 678.05 | 358.30 | 232.43 | 244 个文件 |
> | `lx_ktx_t24`（标称 `=hardware_concurrency` —— **实际与左行同一配置**） | 0 | 294.96 | 757.94 | 428.61 | 225.85 | 244 个文件 |
>
> - **撤回"L7 表格里未验证那格可以划掉"**：244 个文件 sha256 相同这件事本身是真的，但它只能证明
>   "同一份代码两跑产物相同"，证明不了 `threadCount` 的产物中性；`report_tile_diff.js` PASS 同理。
>   注：`LoadTextureKTX.cpp:635-638` 那条源码注释当时被顺手改成"实测 1 与 24 线程逐位相同"，
>   引用的是本段这个已作废实验，且把口径说成"244/244 个产物文件"（实际只数了 `tiles/`+`textures/`）。
>   **源码注释属源码，不在本轮文档授权范围内，保留不动，待用户批准后更正。**
> - **撤回"CPU +11.8 % ⇒ L7 的价值是把并发预算还给别的活"**：678.05 vs 757.94 s 的差出现在
>   **同一份代码**的两跑之间，只能是机器状态漂移，不能读成"24 线程比 1 线程多烧 CPU"。
>   同理 wall 319.61 vs 294.96 s（1.08×）也不含任何线程配额信息。
> - **仍然成立的一条**：335 MB 档的复测被 A-54 挡死 —— 当前二进制在该档出不了盘，而
>   `conversion-report.json` 写在 `selfValidate` 之后 ⇒ 没有报告可读。与 knob 无关，判据不动就拿不到读数。
>
> **2026-10-22 11:41:33 重建 `osgdb_ktx` 后，这个前置问题第一次具备被有效验证的条件**
> （新 DLL 前缀 `1124d517…`，含 knob；`Release/osgPlugins-3.6.5/` 内的 ktx 件被 POST_BUILD 显式删除
> ——`CMakeLists.txt:143-144`——，所以 `Release/osgdb_ktx.dll` 是唯一可加载副本）：
>
> | 跑 | 配置 | rc | tiles | errors | 进程峰值线程 | wall s | 进程 CPU s | peak RSS B |
> |---|---|---|---|---|---|---|---|---|
> | `F:/wb_probe/ktxA` | 默认（`threadCount = hardware_concurrency`） | 0 | 129 | `[]` | **43** | 118.06 | 359.59 | 3 809 947 648 |
> | `F:/wb_probe/ktxB` | `WB_KTX2_THREADS=1` | 0 | 129 | `[]` | **20** | 213.77 | 334.53 | 3 809 755 136 |
>
> - **已确认**：`WB_KTX2_THREADS` 已进入实际使用的 DLL；设置后进程峰值线程数 43 → 20；两侧各 247 个文件、
>   `tiles/`+`textures/` 244 个逐位相同；连同 `tileset.json`、`feature-manifest.json` 共 **246 个非报告文件逐位相同**
>   （唯一不同的是 `conversion-report.json`；把其中的计时/RSS/时间戳字段剔除后两报告差异 **0 行**）；
>   与 10-21 交付 `F:/wb_3dtiles_out/lx_p_a` 除报告外也是 246/246 相同。
>   **口径**：只能说"246 个非报告文件逐位相同"，不能说"247 个文件全部逐位相同"。
> - **初步证据**：线程配额确实改变了运行时线程数（峰值 43 vs 20）。探针在 DLL 里存在 ≠ 生效，
>   这条是行为学观测，不依赖字符串。
> - **待验证**：≥3 轮交替 A/B 的跨运行可复现性；耗时分布（本表每格 n=1，wall 118.06 vs 213.77 s
>   **不得用来判快慢**）；"线程配额不改产物"目前只有 1 对样本，尚未证伪也尚未成立；
>   335 MB 档行为**未被验证**（A-54 预算闸未绕过、预算未改）。
> - **不允许宣称**：L7 已带来性能提升、线程配额绝不影响产物、335 MB 资产已验证。
>
### 每一根杠杆的验收闸（复用 v19 这套，一次都不许省）

1. **逐字段对照**：`node scripts/report_tile_diff.js <基线report.json> <候选report.json>`
   —— 瓦片记录按 `id` 对齐，`triangles`/`contentBytes`/`uri`/`refine`/`hasContent`/`isProxy`/
   `aggregate`/`aggregateSources`/`sourceFeatures` 差异数必须 = 0；五个误差字段在纯性能改动下
   也必须 **差异 = 0**（逐位相同），只有正当的正确性修复才允许加 `--allow-error-diff`，
   且方向仍要人工论证。退出码 0/1/2/3 区分"通过/违例/用法错/**对齐失败**"，
   对齐失败绝不当成通过。
   这把尺子本轮已做过双向对照，不是恒绿：
   v19 vs 自身 ⇒ PASS（差异 0）；v17 vs v19 **不带** flag ⇒ FAIL（5 个误差字段，`rc=1`）、
   带 `--allow-error-diff` ⇒ PASS（负载/结构差异 0）；
   **v15 vs v17 ⇒ FAIL on `contentBytes=693`**（A-41/A-42 修的是负载，正应该被这道闸拦下），
   而同一轮的误差字段差异是 **0**（1059 条上界逐值不变）——两个方向都对得上预期。
2. **产物字节对照**：`tiles/` 全量摘要与基线一致。
3. **官方 validator** `errors=0` + `tileset_bounds.js` 全 0 + `glb_prims.js` 违例 0 +
   `check_manifest_assertions.js` PASS。
4. **同时段交错 A/B**：主指标进程 CPU 秒，同时记 wall、`peakRssBytes`、`LoadPercentage`
   （跨会话 wall 不可比）。
   **本轮给这条补了个量化**：65 MB 档同一输入 8 跑，`micro_evaluate_samples` 恒为
   48 963 414（工作量一模一样），`micro_evaluate_cpu` 却在 **145.22~310.73 s**（2.14×）、
   wall 在 **122~436 s**（3.6×）之间跳。
   ⇒ **单跑读数不能判任何杠杆**；交错重复至少 3 次，并且要把
   `micro_evaluate_samples` 当作"工作量没变"的对照列一起看（它现在在 `counters` 里）。
5. `test_production` 全绿，且新加的差分测试必须有**阳性对照**（先证明它能在错误实现下失败）。

> **这套闸有一个前提必须先说清：AUDIT A-40（同一二进制同一输入产物不逐位一致）仍是 OPEN。**
> 它实测过"哪份几何落到哪个瓦片 id"会随时序变，`errMax` 也跟着变（t09：386.77 ↔ 331.62）。
> 后果直接落在本轮计划上：
> - **第 1、2 条闸不能作为一次性判据**。抬 `WB_JOBS`、改遍历顺序、改线程配额**全都是改时序的改动**，
>   一旦撞上 A-40 的变体分支，"产物不同"就分不清是杠杆引入的还是既有非确定性。
>   所以每根杠杆的 A/B 要**同一杠杆重复 ≥3 次**，先取出该配置自身的变体集合，
>   再判它是否落在基线变体集合内（v19 对 v17 的 0 差异是**单次样本**，
>   按 A-40 累计 127 份的统计看是"命中基线"，不等于"确定性已证"）。
> - **L1（近孩子优先遍历）是唯一能绕开这个困境的**：它不改时序、只改访问顺序，
>   且有"同网格两遍历逐位相等"的**解析论证**，可以在单测层面证死，不需要跑产物对照。
>   所以顺序是 **L1 先做**（可证明）、再动 L2/L3（只能统计性验收）。
> - **A-40 本身该排在提速之前还是之后，是需要用户定的取舍**：把它放前面，提速轮次全部要等；
>   放后面，每一轮的"质量中性"结论都只能给到统计口径。本轮我按"先做可证明的那根"走。

## 3. 与旧管线（Phase 0 基线）的对比



| 指标 | 旧管线基线 | 生产管线 | 说明 |
|---|---|---|---|
| t03 tiles | 1（不切分） | 7（深度 2） | 旧管线无空间划分 |
| t04 tiles | 1 | 127（深度 6） | 同上 |
| 几何重复份数 | 每级 LOD 全量拷贝（3 份） | 父=子树并集简化 1 份 | A-06/A-20 |
| 输出体积 t04 | （基线 outBytes，见 BASELINE §5） | 9.11 MB | 含全部 LOD |
| 误差报告 | 无 | 每瓦片 [lower, upper] | 新增能力 |

注：不做"新管线更快"的声称——新管线多了误差评估与代理简化，
同等质量门槛下（同 SSE、同误差上限）的公平对比需要旧管线先具备
相同的误差/特征保留能力才有意义（手册 13.3 要求）。

## 4. 资源预算执行

硬上限默认值（`ResourceBudget`，与手册 §3 一致，代码实测回显于
`conversion-report.json` 的 `profile.budget`）：
内容 8 MiB/瓦片（软目标 4 MiB）、**120 000 三角形/瓦片**（软目标 80 000）、
**24 个 glTF primitive/瓦片**（软目标 12）、24 材质/瓦片、树深 ≤ 48、
i3dm ≤ 512 实例/瓦片、顶点/索引 ≤ 4 000 000。

- S2 场景实测：`-mb 4096` 下 597 个瓦片超限 → **明确失败**，
  无超限瓦片被输出（手册 0.2：不可行时必须明确失败）；
- 2026-10-19 矩阵 12/12：单瓦片最大 1 162 032 B（13.9 % 硬限）、
  最大 34 296 三角形（28.6 % 硬限）、最大深度 6，违例 0；
- longxiang.fbx：127 个内容瓦片最大 2 226 920 B、最大 34 675 三角形、
  深度 12，违例 0；
- 纹理：默认输出 KTX2（S1 场景实测 b3dm 内引用全为 .ktx2）；
  远景代理按 A-30 契约可整体不带纹理（longxiang 实测 30 个瓦片如此，
  每个都记 warning，不静默）。

> 本文档旧版把默认值写成"200k 三角形/瓦片、20 drawcall/瓦片"，与代码和
> 手册 §3 都不符，已按实测回显改正。


## 5. 视觉链路性能

当前采集链路（`scripts/visual_capture.sh` + `docs/visual-acceptance/index.html`，
CesiumJS 1.119 + 无头 Edge + SwiftShader 软件 WebGL）实测三批：

| 会话 | 数据集 | 帧 | 出图 | 空洞 | 判不了 | 报错 |
|---|---|---|---|---|---|---|
| `fa_t01_plane_curved` | t01（1 瓦片） | 69 | 69 | 0 | 0 | 0 |
| `full_t02` | t02（7 瓦片） | 34 | 30 | 0 | 4 | 0 |
| `full_longxiang` | longxiang（129 瓦片） | 34 | 34 | 0 | 0 | 0 |

（`node scripts/va_paths.js <filter>` 复核，判据口径见
`docs/visual-acceptance/README.md`；`full_*` 两批早于 `axis-sweep` 路径注册，
是 9 条路径。）

本节**不给帧率/每帧耗时**：无头环境的 `Date.now()` 是虚拟时钟
（`--virtual-time-budget` 语义是"页面跑完再把虚拟时间烧掉"），
页面里记录的 `seconds` 只能用于同批次相对比较，当真机性能数据上报会造假。
软件渲染的帧率不代表真机性能，真机 GPU 帧率仍是 BLOCKED 项。

## 6. 未决 / 风险

1. t03 的 117.94 s 里 117.78 s 耗在 `proxy_encode_error`（误差评估 + 代理简化）
   ——大批量生产场景需要按手册 9.4 进一步做"重要性筛选 + 采样预算上限"；
   **2026-10-20 下午**：该阶段已落两段并行（#6 乱序收割、#7 节点内分块），
   t03 wall 降到 96~112 s 而 CPU 秒持平，读数与口径见 §2.1；
   "重要性筛选"那条**仍未做**。
2. 纹理图集/去重（hash-addressed KTX2 共享）在真实 FBX（123 张纹理）
   上观察到缓存去重生效（`textures/` 每张哈希一个文件，本轮 85 个 / 14.1 MB），
   但图集合并尚未在真实 FBX 上测量收益；
3. 峰值 RSS 逐资产分布：t12_big_texture 232.4 MiB（大纹理解码）、
   t03 148.5 / t04 146.8 / t06 130.4 MiB——后三者比 2026-10-16 的
   93.1 / 77.3 / 38.9 MiB 明显上涨，与代理重写的多轮整体挤压有关，
   KTX2 压缩器的流式处理可再降；
4. **真实 FBX 内存回退（B-7）**：longxiang 峰值 RSS 650 MiB → 2 401 MiB，
   同时耗时 629.9 s → 93.4 s。尚未定位到单一分配点（候选：聚合代理
   `buildAggregate` 的多轮候选常驻、`DEEP_COPY_ALL` 子树在简化前的整份拷贝）。
   本机 127 GiB 无风险，但生产并发需按 2.4 GiB/任务规划。
   335 MB 的 `20240529卢沟桥分洪枢纽.fbx` 把这条风险抬高了一个量级，**本轮出盘后有终值了**：
   `lugouqiao_v17` 的 `report.peakRssBytes = 9 830 379 520`（**9.15 GiB**，与 08:35 出盘
   同批），此前"`wmic` 在 36 min 采到 5.49–6.56 GiB、尚未出最终峰值"的采样口径已被
   终值取代。**生产并发预算必须按 ~9.2 GiB/任务**算，而不是 longxiang 的 2.4 GiB——
   按 2.4 GiB 规划会在 335 MB 这一档直接超估近 4 倍（本机 127 GiB 下单跑无风险）。
   耗时同比例放大：28 485 s（7.9 h）里 `proxy_encode_error` 占 28 471.08 s（**99.95 %**），
   见 §1 的 v17 表与 §2。
   > **2026-10-21 更新（65 MB 档自身也涨了，且未归因）**：同一份 `longxiang.fbx`
   > 现在 `report.peakRssBytes` = **3 637 / 3 654 MiB**（`final512` / `l0b` 两跑），
   > 不再是这条原来写的 2 401 MiB。跳的那一档（2 401 → 3 631）**不能记在代理纹理改动上**：
   > 中间还夹着 A-41/A-42/A-43 与 #6 乱序收割、#7 节点内并行三批改动，
   > 没有做过单独的二分归因，状态记为**未证实**。
   > 并发预算的口径因此要按"335 MB 档 9.2 GiB、65 MB 档 3.6 GiB"两档分别规划。
5. 树深硬限（48）此前只有报告值、不在发布闸内；`b267ca1` 已把
   `selfValidate` 的 3b 检查补上（超深即拒绝发布），并用
   `test_production.exe proxySkip`（构造 `maxTreeDepth=1` 的不可再分资产）
   实测触发；A-34/A-35 的 manifest 断言并入后当前为 118/118 通过；
6. A-31 之后，**内节点**远景代理放不下时降级为"仅子树覆盖"的内容空节点并记
   warning（`report.proxySkippedCount`），不再让整批数据报废；叶子瓦片超限
   仍然是硬失败。降级节点在 CesiumJS 1.119 里不会造成远景空洞：
   内容空节点 `hasRenderableContent=false` 会强制继续 refine 到子节点（源码级核对）。
7. ~~**子阶段占比至今读不到**~~ —— **2026-10-20 下午作废**：`micro_*` 已进
   `ConversionReport.stages`，`lugouqiao_v19` 读到了六项秒数（见 §2.2 那张表）。
   剩下的三个洞也换了性质，见下面第 **9** 条（materialize 无计时、
   瓦片级 evaluate 秒不可读、两个计数混在秒数组里）。
   §2.1 里"写出与最终化可能是 t04/longxiang 只提升 8 % 的原因"现在**在 335 MB 档可以判了**：
   `micro_finalize_encode` 564.71 s 对 4 592.42 s 阶段 wall ⇒ **串行上限 12 %**，
   它不是这一档的主因，主因是评估并发只吃到 1.63 核。
   **t04/longxiang 那 8 % 的原因仍未证**——小资产没有对应的 `micro_*` 读数。
8. ~~**335 MB 档的提速与内存都未验证**~~ —— **2026-10-20 下午闭合**：
   `lugouqiao_v19` 用 8 job / 16 窗口的形状跑完，`totalSeconds=4 597.57`（6.2×）、
   `peakRssBytes=6.93 GiB`（比 v17 单跑的 9.15 GiB **低 24 %**）。
   §6.4 那个 **9.2 GiB/任务** 从此不能当并发排产依据：**并发不是把内存往上推的方向**。
   仍未证的：更高 job 数下的内存曲线（L2），以及"最大那一档单瓦片评估要多久"（L0 才有读数）。
9. **仪表还剩三个洞（L0，下一轮提速的前置）**：
   ① 叶瓦片 `_model.materialize()`（`ProductionPipeline.cpp:613`）没有计时器，
      代理 `ProxyMeshBuilder::build()` 只计了 simplify/aggregate 两项 ⇒
      v19 的 `micro_*` 累加 8 244.8 s 对进程 CPU 秒 12 168.6 s，**≈3 924 s（32 %）无归属**；
   ② 瓦片级 `micro_evaluateSec` 只汇总进 `_microEvaluateSec`，没落到 `report.tiles` ⇒
      读不出关键路径，stragger 策略的上限因此不可测；
   ③ `micro_evaluate_samples`（438 681 617）与 `micro_aggregate_candidates`（122）是**计数**，
      却通过 `_report.addStage(...)` 写进了 `stages` 这个**秒**数组
      （`ProductionPipeline.cpp:1219-1226`）——读表时这两行不是耗时。

---

## 2026-10-28（第二轮）：longxiang 档的真实归因与一次 SAH 提速

口径先说清（不然后面的数会被误读）：

1. 本节的秒数**全部来自 `conversion-report.json` 的 `stages`**，不是外部秒表；`totalSeconds`
   只覆盖 `ProductionPipeline::run()`，`input_read` 单独一行。
2. `micro_*` 是**瓦片并行**段：`micro_total` 是各瓦片 `processTileHeavy` 的**壁钟累加**，
   8 路并行时它约等于 1 份工作量，不能与 `totalSeconds` 相除当占比；对账要用
   `micro_total_cpu`（CPU 秒）。
3. `micro_write` / `micro_finalize_encode` 是**主线程串行**段，它与并行的 `micro_total`
   是**先后**关系，两者相加会超过 `totalSeconds` 的错觉来自第 2 条。

### 1. 基线归因（HEAD，机器空闲，n=1）

`longxiang.fbx`，129 瓦片 / 127 有内容，`errors=[]`，`totalSeconds=99.2`、进程 CPU 285.8 s、
峰值 RSS 3 741 MB（`peakRssBytes`）。

| 段 | 秒 | 说明 |
|---|---|---|
| `micro_write` | 86.9 | 主线程串行写：Osg2Gltf 编码 + b3dm 组装（127 个内容瓦片） |
| └ `micro_texture_export` | 52.0 | 其中 KTX2 压缩（libktx 自带 `threadCount`），117 个文件 |
| `micro_evaluate` | 51.9 | 双向误差评估，在并行段内 |
| └ `micro_eval_init` | 29.7 | 初始采样（每三角形 3~7 个点） |
| └ `micro_eval_refine` | 19.0 | 自适应细分（每次 1 个中点查询） |
| └ `micro_bvh_build` | 2.2 | 双向建树 |
| `input_read` | 2.3 | `readNodeFile`（在 `run()` 之外） |

结论：**这一档的主成本是"串行写盘 + KTX2 压缩"（86.9 s，占 `totalSeconds` 的 88 %），
不是误差评估（51.9 s，且与瓦片并行重叠）**。这与历史上"评估占 97 %"的档（卢沟桥）不是同一回事，
不能把那条结论套到这一档。

### 2. 真实查询计数（A-61）：样本数不是查询数

同一次运行（候选 B，含计数器）：

| 指标 | 读数 | 含义 |
|---|---|---|
| `micro_evaluate_samples` | 48 963 414 | 逻辑采样记录（细分一次记 6 条） |
| `micro_eval_distance_calls` | **16 283 784** | 真实 `distance()` 调用 = init + refine |
| └ `micro_eval_init_distance_calls` | 9 747 858 | 初始采样（≈ 源三角形 × perTri） |
| └ `micro_eval_refine_distance_calls` | 6 535 926 | 细分迭代（每次恰 1 次） |
| `micro_eval_refine_iterations` | 6 535 926 | 与上一行相等 ⇒ 恒等式成立 |
| `micro_eval_bvh_node_visits` | 1.12e9（改前 3.19e9） | 平均 **69** 节点/查询（改前 196） |
| `micro_eval_bvh_triangle_tests` | 7.18e8（改前 2.31e9） | 平均 **44** 测试/查询（改前 142） |

⇒ "样本 4 896 万"被当成"查询 4 896 万次"会把查询成本高估 **3 倍**。两个数从这轮起成对出现在
`counters` 里，报告消费者不该只读其中一个。

### 3. 分段计时的真实语义（A-55 遗留的边界）

- `micro_eval_init` = 分块循环的整体墙钟，**含**建线程与 join 等待；本轮把它进一步拆成
  `micro_eval_init_parallel`（建完线程到 join 返回，**含 worker 的计算时间**）与
  `micro_eval_init_thread_create`（只算 `std::thread` 构造）。**不要**把前者当"instrumentation
  开销"——第一版就是这么误读的（29.1 s 看着像纯开销，实际主要是 worker 在算）。
- `micro_eval_push` = 建 patch + 算 key + 入堆/剪枝，串行；`micro_eval_quantile` = `all` 排序取
  p95/p99，**不参与任何判定**。
- 瓦片级 `timing` 里同样有 `evaluateSec` / `evalInitSec` / … 逐瓦片一份，`micro_*` 是它们的和。

### 4. SAH 切分（A-62）：ABBA 四次运行

配置：`-sp -i D:\BaiduNetdiskDownload\longxiang.fbx -o <新目录>`（默认预算/参数），
A = 干净 HEAD，B = 候选；同一台机器、同一套编译命令行，按 **A→B→B→A** 交替。

| 轮次 | total | 进程CPU | 峰值RSS | micro_evaluate | init | refine | micro_write | 纹理导出 |
|---|---|---|---|---|---|---|---|---|
| A1 | 99.2 | 285.8 | 3 741 MB | 51.9 | 29.7 | 19.0 | 86.9 | 52.0 |
| B1 | 112.7 | 257.1 | 3 733 MB | **23.0** | 12.9 | 7.0 | 105.9 | 65.8 |
| B2 | 92.5 | 251.0 | 3 733 MB | **21.6** | 12.5 | 6.0 | 86.1 | 51.7 |
| A2 | 137.3 | 301.5 | 3 745 MB | 70.5 | 42.9 | 23.7 | 122.5 | 79.8 |

- **评估段**：A 51.9 / 70.5（均值 61.2），B 23.0 / 21.6（均值 22.3）⇒ **−64 %**；
  单侧看 init −57 %/−58 %、refine −63 %/−68 %。
- **total**：A 均值 118.3，B 均值 102.6（−13 %），但 `micro_write` 在 86.1~122.5 s 之间漂移
  （同一二进制、同一输入），**这段波动比评估收益还大** ⇒ 本轮**只在"评估段"上下结论**，
  不声称整体提速百分比。要拿到可信的整体数，需要把写盘段的方差压下去（串行单线程 + KTX2
  线程竞争），那是另一个改动。
- **产物**：4 次运行 246 个文件 sha256 **全等**（清单 md5 `9a4326be…`），
  `report_tile_diff` 判 PASS ⇒ 提速没有换来任何数值/结构变化。
- 新计数器的开销**含在 B 的时间里**（每次查询/节点/三角形各 +1，合计约 1.9e9 次自增），
  所以 −64 % 是净收益，不是"把计数器关掉才有的数"。

### 5. 没动的杠杆（附理由，避免下轮重复调研）

| 杠杆 | 现状读数 | 为什么本轮不动 |
|---|---|---|
| 串行写盘（86.9 s） | 127 瓦片 / 0.68 s 每片 | 并行写出会碰共享纹理缓存、`mapper`、`report` 与 A-40/A-52 的确定性结论；无同批复测预算时不碰 |
| KTX2 压缩（52.0 s，117 文件） | libktx 自带 24 线程 | 第三方线程，`WB_KTX2_THREADS` 已有；改它属"重写线程池"，缺证据 |
| 重复评估（`eval_source_triangles` 1.76 M vs 源 234 K） | 7.5× 重复 | 祖先瓦片各自重评子树是被 A-31 的"子树覆盖"语义决定的，改它等于改质量口径 |
| 代理简化/聚合 | 4.0 s / 2.1 s（< 5 %） | 不是热点 |

