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
| 误差统计 | `errorMax=8 857.99 m`、`errorMean=117.70`、`errorP95=3 502.99`、`tilesetGeometricError=8 857.99`；`nodesCertified=450`、`nodesBoundLoose=332` | report |
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

热点分析结论不变：误差评估的采样密度（perTri 起始 7、自适应递减）是主要成本，
当前是正确性优先的实现；手册 9.4 的"评估成本控制"已实现自适应采样，
进一步优化空间在按屏幕重要性筛选拒绝评估（待做，见"未决"）。

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
5. 树深硬限（48）此前只有报告值、不在发布闸内；`b267ca1` 已把
   `selfValidate` 的 3b 检查补上（超深即拒绝发布），并用
   `test_production.exe proxySkip`（构造 `maxTreeDepth=1` 的不可再分资产）
   实测触发；A-34/A-35 的 manifest 断言并入后当前为 118/118 通过；
6. A-31 之后，**内节点**远景代理放不下时降级为"仅子树覆盖"的内容空节点并记
   warning（`report.proxySkippedCount`），不再让整批数据报废；叶子瓦片超限
   仍然是硬失败。降级节点在 CesiumJS 1.119 里不会造成远景空洞：
   内容空节点 `hasRenderableContent=false` 会强制继续 refine 到子节点（源码级核对）。
