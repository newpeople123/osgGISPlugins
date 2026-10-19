# VALIDATION_REPORT.md — 验证报告

所有数据均为本机实测（§1/§3/§5/§6 复测于 2026-10-19，HEAD `ee6499e`；
§2/§4 标注为 2026-10-16 及以前），不包含推断或估算值。
运行环境：Windows 10 19045 / MSVC 14.44 / OSG 3.6.5 / TBB。

## 1. 测试矩阵（手册 14 节，12 资产回归）

命令（本机 `python`/`python3` 是 Windows Store 占位程序，`rc=49` 且无任何输出，
`scripts/run_matrix.py` 无法执行 → BLOCKED；改用**仓库内** bash+node 采集器，
逐资产跑同一命令行并读工具自己写出的 `conversion-report.json`）：

```bash
bash scripts/wb_build.sh model23dtiles
bash scripts/run_matrix.sh E:/dev/C++/osgGISPlugins-gpt/test_assets \
                           E:/dev/C++/osgGISPlugins-gpt/docs/model23dtiles-production/runs/matrix_final
node scripts/collect_matrix.js <同一个 outDir>     # 汇总硬限/误差/覆盖计数
```

结果 **12/12 发布**（判定依据：`errors==0` 且 `tileset.json` 落盘；退出码在本环境
被宿主改写，不作判据）。§3 硬限违例 **0**（2026-10-19 12:26，最终二进制
12:22:48，产物 `docs/model23dtiles-production/runs/matrix_final/`，
原始逐资产 stdout 见 `runs/matrix_final.log`、汇总表 `runs/matrix_final_table.txt`）：

| 资产 | rc | 耗时 s | 峰值 MiB | tiles | 深度 | 内容 | 最大字节 | 最大三角形 | 源feature/出现 | miss/drop | 代理降级 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| t01_plane_curved | 0 | 0.02 | 16.4 | 1 | 0 | 1 | 77 184 | 3 042 | 1/1 | 0/0 | 0 |
| t02_thin_features | 0 | 0.30 | 32.5 | 7 | 2 | 7 | 42 808 | 572 | 54/162 | 0/0 | 0 |
| t03_giant_single_mesh | 0 | 99.00 | 148.2 | 7 | 2 | 7 | 1 162 032 | 34 296 | 1/11 | 0/0 | 0 |
| t04_many_objects | 0 | 27.49 | 146.9 | 127 | 6 | 127 | 1 143 360 | 14 400 | 1202/8414 | 0/0 | 0 |
| t05_instances | 0 | 0.02 | 18.0 | 4 | 2 | 2 | 24 704 | 12 | 300/2 | 0/0 | 0 |
| t06_nonuniform | 0 | 2.71 | 132.3 | 31 | 4 | 31 | 193 624 | 2 412 | 203/1015 | 0/0 | 0 |
| t07_zero_axis | 0 | 0.02 | 16.3 | 1 | 0 | 1 | 60 016 | 2 404 | 2/2 | 0/0 | 0 |
| t08_big_coords | 0 | 0.79 | 56.7 | 7 | 2 | 7 | 59 080 | 720 | 62/186 | 0/0 | 0 |
| t09_cross_tile_object | 0 | 0.79 | 54.5 | 7 | 2 | 7 | 41 152 | 492 | 43/129 | 0/0 | 0 |
| t10_material_heavy | 0 | 0.56 | 55.7 | 7 | 2 | 7 | 40 184 | 480 | 42/126 | 0/0 | 0 |
| t11_sparse_outlier | 0 | 1.73 | 81.4 | 15 | 3 | 15 | 146 768 | 1 824 | 154/616 | 0/0 | 0 |
| t12_big_texture | 0 | 1.82 | 232.2 | 1 | 0 | 1 | 510 432 | 6 962 | 1/1 | 0/0 | 0 |

「代理降级」= `proxySkippedCount`（A-31）：12 个资产的远景代理都在预算内，
所以这一列全 0；该计数由 §3 的 `proxySkip` 用例负责非零验证，不在此列制造。
本轮 `t03 99.00 s` 比上一轮（`ee6499e` 二进制，`F:\wb_3dtiles_out\matrix_texless`）
的 117.94 s 快，是因为这两个数都在与 335 MB 真实 FBX 转换**并发**的环境里测的，
CPU 抢占比例不固定——耗时列只作量级参考，静默机器复测见 PERFORMANCE_REPORT §1。
`miss/drop` = `featuresMissingFromLeaves` / `proxyDroppedFeatureCount`（A-28 新增
的源身份覆盖不变量，全 0 即"没有任何部件在远景 LOD 上消失"）。
表中「最大字节」是工具报告的估算值（`200 B/tri + 48 B/vert + 纹理/8`）；
发布闸 `selfValidate` 是对**落盘文件实测**的，两者不一致时以实测为准并由
`finalizeTileFromWork` / `selfValidate` 判失败或降级（A-31）。
t08/t09/t12 的最大字节与上一轮差 16～88 B：差异来自二进制本身（本轮额外带
`needsTextureless` 判据）。当时据此写的"同一二进制内逐文件 md5 完全一致
（`runs/det_t08_{1,2}`，两次独立运行 tile 目录 md5 相同）"**已被 §1b 的复跑推翻**：
样本=2 的串行轻载不足以保证可复现，同一二进制同一输入累计 127 次复跑里有 6 次整棵瓦片树
变化（AUDIT A-40；变体同时出现在默认并行、`WB_JOBS=4`、`WB_PROXY_DEBUG=1` 三种配置）。
t08 那两次恰好相同，不代表"划分确定"。

对比 Phase 0 基线（同一资产集，BASELINE.md §5）：

| 缺陷 | 基线 | 现在 |
|---|---|---|
| box 负半轴 | t07 实测出现 | 无（validator 0 error） |
| 父子包围盒不覆盖 | 48/6/2/3/1 个 ERROR | 0（double 精度 + 相对容差） |
| 巨型单网格不切分 | t03 134k 三角形 1 个 tile | 切成 7 tiles（对象/图元/三角形级） |
| 父子 geometricError 差恒 0.1 | 是 | 无（测量误差自底向上聚合） |
| tileset 误差 = root×1.5 | 是 | 无 |
| 远景代理整簇丢失（A-27） | 12 个占用 cell 只剩 6 个有几何 | 全覆盖，`proxyDroppedFeatureCount=0` |

## 1b. 复跑 `matrix_a34`（A-33/A-34/A-35 落地后，二进制 15:47:04）

```bash
bash scripts/run_matrix.sh E:/dev/C++/osgGISPlugins-gpt/test_assets \
                           E:/dev/C++/osgGISPlugins-gpt/docs/model23dtiles-production/runs/matrix_a34
node scripts/collect_matrix.js <同一个 outDir>
bash scripts/run_validator.sh <同一个 outDir> \
     E:/dev/C++/osgGISPlugins-gpt/docs/model23dtiles-production/validation-matrix-a34
```

结果 **12/12 发布**、`limitViolations=0`（`runs/matrix_a34/`，原始 stdout
`runs/matrix_a34.log`）。与 §1 的 `matrix_final` 是**同一资产集、不同二进制**：
这一轮带上了 A-33（i3dm 三角形预算）、A-34（聚合代理身份映射）、A-35（占用数
单一口径），所以只列变化列，其余列见 §1：

| 资产 | 耗时 s | 峰值 MiB | 最大字节 | 最大三角形 | cert/loose | agg代理 | warn |
|---|---|---|---|---|---|---|---|
| t01_plane_curved | 0.03 | 19.2 | 77 184 | 3 042 | 0/0 | 0 | 0 |
| t02_thin_features | 2.21 | 47.6 | 42 840 | 572 | 1/3 | 0 | 0 |
| t03_giant_single_mesh | 104.07 | 154.6 | 1 162 032 | 34 296 | 0/3 | 3 | 3 |
| t04_many_objects | 33.06 | 154.2 | 1 143 360 | 14 400 | 63/59 | 0 | 0 |
| t05_instances | 0.03 | 21.2 | 24 704 | **3 588** | 0/0 | 0 | 0 |
| t06_nonuniform | 3.05 | 140.3 | 193 624 | 2 412 | 13/13 | 0 | 0 |
| t07_zero_axis | 0.02 | 16.4 | 60 016 | 2 404 | 0/0 | 0 | 0 |
| t08_big_coords | 0.85 | 57.7 | 59 080 | 720 | 3/3 | 0 | 0 |
| t09_cross_tile_object | 0.85 | 55.4 | 41 120 | 492 | 3/3 | 0 | 0 |
| t10_material_heavy | 0.59 | 59.1 | 40 184 | 480 | 3/3 | 0 | 0 |
| t11_sparse_outlier | 1.93 | 85.5 | 146 768 | 1 824 | 7/6 | 0 | 0 |
| t12_big_texture | 2.04 | 234.3 | 510 432 | 6 962 | 0/0 | 0 | 0 |

**这一轮真正被测到的东西**（不是"报告说做了"，是从产物里读出来的）：

| 断言 | 实测 |
|---|---|
| A-33 i3dm 瓦片按**展开**三角形计数并受预算约束 | `t05/i2.i3dm` 报 `triangles=3588` = 原型 12 三角形 × 299 实例，≤ 120 000 硬限；上一轮这里报的是 12 |
| A-34 聚合代理身份映射可查 | `manifest.aggregateProxies` 在 t03 有 **3 个瓦片 / 7 条 BATCHID→源 feature**（`tiles/t0、t1、t4`），其余 11 个资产为 0 —— 与 `report.aggregateProxyCount`（t03=3，其余 0）**逐个对得上** |
| A-35 占用数单一口径 | 12/12 资产 `report.manifestOccurrences == manifest.occurrenceCount`（例如 t04 8414/8414、t06 1015/1015、t11 616/616） |
| A-28 源身份覆盖 | 12/12 `featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0` |

**外部验证**：`validation-matrix-a34/` 12 个 tileset 合计 **3 error / 1 warning**。
唯一 1 warning 是 t12 的 KTX2/`KHR_texture_basisu`（validator 自带 glTF schema 早于
该扩展，见 §7）；**3 个 error 全在 t02**，路径 `/meshes/1/primitives/0/attributes/NORMAL`、
`Vector3 at accessor indices 234..236 / 246..248 / 258..260 is not of unit length: 0`——
即 **A-38 的产物缺陷**，与 §7 里"14/14 零 error"（上一轮 `validation/`，同一
validator 0.6.1，t02 报 0/0）对照，坐实 A-38 是间歇缺陷。t02 的 2.21 s / 47.6 MiB /
`errMax=5.8e24` / `cert:loose=1:3` 全部是这个顶点的下游症状（正常值应是 0.30 s /
32.5 MiB / ≈23 m）。

> **这一轮的"12/12 发布"不能当作 A-36～A-38 发布闸的验收证据**：产物由 15:47 的
> 二进制写出，而 1b（有限性/父覆盖子）与 2b（源范围）都在那之后才写进源码，
> 当时还没编译进 `Release/model23dtiles.exe`。t02 被发布恰恰证明**旧二进制没有
> 这道闸**；带闸的二进制对真实数据的实测还欠着（见 §8b 末尾与 STATUS 待办）。

**可复现性（§1 的原结论据此修正）**：同一二进制（`Release/model23dtiles.exe` 15:47:04）、
同一 `t02` 输入累计 **127** 份产物（§5 的 30 次默认并行滑窗 + §7 的 6 次 `WB_JOBS=1` +
matrix_a34 本体 + 追加 `node .workbuddy/a38_rate/drive.js 30` 的 3 配置 × 30 = 90 次：
默认并行 / `WB_PROXY_DEBUG=1` / `WB_JOBS=4`）。其中 **121 份 `tiles/*.b3dm` 逐字节相同**，
**6 份整棵树变了**（共 5 个不同哈希；7 个瓦片字节数全不同、`nodesBoundLoose` 在 0/1/3、
`nodesCertified` 在 1/2/3 跳、峰值 RSS 34.4→47.3 MiB），matrix_a34 那一份同时就是 A-38
的污染产物；**追加 90 次里 0 次重现污染顶点**，污染字节全库扫描（58 133 个文件）也只命中
那 3 个瓦片。一条重要修正：`WB_JOBS=4` 跑出的变体哈希与旧"只有 `WB_PROXY_DEBUG` 第 3 跑"
的那个 `fe354714…` **完全相同**，所以"变体绑调试开关""并行度不是变量（n=6 串行）"两条旧
结论都作废——变体既不绑调试开关也不绑并行度。**本轮又拿到第二个、且 0.75 s 一轮的样本**：
`t09_cross_tile_object` 用 22:40 二进制单跑 8 次 → `t0.b3dm` 落在 3 个变体上
（6×`95b36adf74` / 1×`792a9422a0` / 1×`8e8f5da801`），而嵌在 `matrix_a41fix2` 里那次是
**第 4 个**变体（`maxBytes` 41152、`errMax` 331.62 而不是 386.77）——**误差指标本身**也随变体动。
6/8 次与上一版（22:14）二进制逐字节相同，所以"矩阵之间 t09 变了"不能算成 A-41 改动的后果，
而且 t09 的 `instanceTileCount=0` 根本不进那条代码路径。产物不逐位一致这件事按 **AUDIT A-40**
单列，逐配置/逐瓦片哈希对比见 `evidence/a38-forensics.txt` §5/§7/§8/§9。

## 1c. 当前权威矩阵 `matrix_a42fix`（A-41 相对帧 + A-42 路径泄漏修复，exe 00:12 / `osgdb_gltf.dll` 00:16）

```bash
cmake --build build --config Release --target model23dtiles test_production -j 8
# osgdb_gltf 是 MODULE 插件，不在上一行的依赖图里：改到插件源码后必须单独重建，
# 否则 exe 用的是旧 DLL（本轮踩过：DLL 停在 22:14，重跑看不出修复生效）
cmake --build build --config Release --target osgdb_gltf -j 8
bash scripts/run_matrix.sh E:/dev/C++/osgGISPlugins-gpt/test_assets \
                           F:/wb_3dtiles_out/matrix_a42fix
node scripts/collect_matrix.js F:/wb_3dtiles_out/matrix_a42fix
node scripts/i3dm_pose_check.js <outDir>/t05_instances     # A-41 位姿验收（相对帧口径）
node scripts/path_leak_scan.js  <outDir>                   # A-42 交付物不含本地绝对路径
```
构建与转换**不要并行**：转换进程会占住 `Release/osgPlugins-3.6.5` 里的 DLL，
post-build 的 `vcpkg z-applocal` 拷贝会失败（MSB3073，本轮实测撞到过一次）。

结果 **12/12 rc=0、`limitViolations=0`、`path_leak_scan` 0/213 瓦片 0 命中**
（`F:/wb_3dtiles_out/matrix_a42fix/`）。这一轮与 §1/§1b 的差别是**口径**而不是列数：
`selfValidate` 的 2b 参考系已改成"全部 `_model.objects` aabb 的并集"（A-41 记录的两条实测），
i3dm 位姿第一次进入验收，且验收口径是 `inverse(原型世界)·实例世界` 而**不是**绝对平移。

| 资产 | rc | 耗时 s | 峰值 MiB | tiles | 最大字节 | 最大三角形 | errMax | cert/loose | inst | src/occ | miss/drop |
|---|---|---|---|---|---|---|---|---|---|---|---|
| t01_plane_curved | 0 | 0.02 | 16.4 | 1 | 77 184 | 3 042 | 0.00 | 0/0 | 0 | 1/1 | 0/0 |
| t02_thin_features | 0 | 0.30 | 33.2 | 7 | 42 808 | 572 | 40.35 | 3/1 | 0 | 54/162 | 0/0 |
| t03_giant_single_mesh | 0 | 261.54 | 149 | 7 | 1 162 032 | 34 296 | 131.27 | 0/3 | 0 | 1/4 | 0/0 |
| t04_many_objects | 0 | 67.54 | 149.2 | 127 | 1 143 360 | 14 400 | 317.14 | 63/59 | 0 | 1202/8414 | 0/0 |
| **t05_instances** | **0** | 0.02 | 18.1 | 4 | 24 712 | 3 588 | 0.00 | 0/0 | **1** | 300/2 | 0/0 |
| t06_nonuniform | 0 | 2.66 | 143.9 | 31 | 193 624 | 2 412 | 754.27 | 13/13 | 0 | 203/1015 | 0/0 |
| t07_zero_axis | 0 | 0.02 | 16.3 | 1 | 60 016 | 2 404 | 0.00 | 0/0 | 0 | 2/2 | 0/0 |
| t08_big_coords | 0 | 0.79 | 61.4 | 7 | 59 080 | 720 | **10 633 297.47** | 3/3 | 0 | 62/186 | 0/0 |
| t09_cross_tile_object | 0 | 0.80 | 53.8 | 7 | 41 120 | 492 | 386.77 | 3/3 | 0 | 43/129 | 0/0 |
| t10_material_heavy | 0 | 0.56 | 52.6 | 7 | 40 184 | 480 | 123.29 | 3/3 | 0 | 42/126 | 0/0 |
| t11_sparse_outlier | 0 | 1.85 | 81.9 | 15 | 146 768 | 1 824 | 5643.16 | 7/6 | 0 | 154/616 | 0/0 |
| t12_big_texture | 0 | 1.85 | 232.2 | 1 | **510 224** | 6 962 | 0.00 | 0/0 | 0 | 1/1 | 0/0 |

四条必须一起读：

1. **t05 是这一轮的实质变化**：`instanceTileCount=1` 且 `i3dm_pose_check` 判
   **299/299 = 100 % 对上相对帧 `inverse(原型世界)·实例世界`**（最近邻残差最大 **9.47e-6 m**，
   float32 在 \|v\|≈150 m 的舍入量级）、按绝对平移比对 **0/299**（命中数必须为 0，
   命中恰恰是双变换）、POSITION 非零 299/299、GLB 节点 TRS=0 → PASS。
   §1/§1b 里的 t05 虽然 rc=0，但位姿全是 0（A-41），那两份产物按"格式通过、内容作废"读；
   `a41fix_t05` 与 `matrix_a41fix2` 那两批"对上绝对平移就算 PASS"的中间产物同样作废。
2. **t12 的 `最大字节` 从 510 440 变成 510 224，是 A-42 不是 A-41**：GLB 的
   `bufferView.name`/`image.name` 原来写的是纹理的**完整输出路径**，现在只剩文件名。
   这条同时解释了历史上 t12 在 510 432/510 440/510 344/510 304/510 296 之间跳——
   **瓦片字节数随输出目录字符串长度变化**（JSON chunk 按 8 字节补齐），过去被误当成 A-40 的噪声。
3. **t08 大坐标 `errMax=1.06e7` 没有被 2b 误伤**——闸门对"合法但量级离谱"的资产放行，
   对 A-41 那种"内容离开源范围"才拒（gate-only 那轮 t05 的 rc=3 就是真阳性）。
   ① "正常数据集不因 2b 误伤"这一条到现在为止的证据是 12 个合成资产，
   真实 FBX 要等 `lugouqiao_v17` 才算验过。
4. **t09 这一轮回到了 §1/§1b 的基线**（41 120 / 386.77 / cert-loose 3/3，与
   `matrix_a41fix` 逐字节同列）：`matrix_a41fix2` 那次是 A-40 的第 4 个变体，
   `maxBytes/errMax/cert-loose` 随变体动。本表列的是这一次运行的实测值，
   **t09 不是该资产的稳定值**，其余 11 个资产的稳定值与本表一致。

## 2. 扩展场景（矩阵缺口补齐）

命令：`python scripts/run_extra_scenarios.py`
结果 **4/4 PASS**（`docs/model23dtiles-production/runs/extra_scenarios/summary.json`，
2026-10-16 实测）：

| 场景 | 内容 | 结果 |
|---|---|---|
| S1 | `-tf ktx2` 全链路：KTX magic 校验 + b3dm 内引用全为 .ktx2 + validator | PASS |
| S2 | `-mb 4096` 超硬限：报 597 条明确 ERROR，**outputDir 零文件残留**（14.16/14.17） | PASS |
| S3 | 实例瓦片格式：内容文件 magic = `i3dm` | PASS（**仅容器 magic**）；"每实例位姿落盘"这一子项 2026-10-16 产物为 **FAIL**，代码已修，**合成资产 t05 已重采并 PASS（§1c），真实 FBX 场景待 `v17`** |
| S4 | 常规 b3dm + BATCH_LENGTH 一致性（validator BATCH 检查） | PASS |

> S3 的 PASS **只覆盖容器 magic**（文件头是 `i3dm`）。"每个实例的位姿有没有落盘"这一项
> 历史上从未验收过，本轮实测发现它**是坏的**：v15 的 126/126 个 i3dm 瓦片特征表 POSITION
> 全为 0，实例位姿全部丢失，且原型几何被同组实例矩阵就地累乘到最大 1.22e8 m——见 AUDIT
> **A-41**（阻断级；flatten 掩码 exe 22:40 生效，"相对原型"那半 exe 23:53 才生效）。
> 修好之后新增验收工具 `node scripts/i3dm_pose_check.js <数据集目录|文件.i3dm>`：
> 它把特征表 POSITION 逐条与 `inverse(原型世界矩阵)·实例世界矩阵` 的 `(x,−z,y)` 置换做
> 最近邻对集合（**不是**对绝对平移——对绝对平移全命中恰恰是双变换的签名，本轮一开始就用错
> 过一次这个口径，已作废那批中间产物重跑）。§1c 的 t05 实测 **299/299 命中、残差最大
> 9.47e-6 m、绝对帧比对 0/299**、GLB 节点 TRS=0、原型 span 回到 2.00 m；同一工具对
> `matrix_a34/t05` 判 0/299、对 `lugouqiao_v15` 判 126/126 全 0（exit 1），
> 对 22:40 那批"位姿落的是绝对平移"的产物也判 FAIL，所以这不是一个"永远 PASS"的检查。
> §1 矩阵与 §2 扩展场景里凡涉及实例资产的结论，在 v17 之前仍只在"计数/格式"口径上成立，
> 不能读成"实例渲染正确"；S1–S4 表格本身是 2026-10-16 的产物，**S3 行的 FAIL→PASS 目前
> 只有合成资产 t05 那一份证据**（见 STATUS 未闭合第 5 条）。

> BLOCKED（2026-10-19）：本机 `python` 是 Windows Store 占位程序，S1–S4 的自动化
> 采集器 `run_extra_scenarios.py` 无法重跑（`rc=49`、零输出）。上表是 2026-10-16
> 的真实产物，未重新生成，因此**不包含**此后 A-26…A-31 改动的回归覆盖；
> 该缺口由 §1 矩阵（12 资产，含 `-tf ktx2` 默认链路）+ §3 单测共同承担。
> 恢复命令：安装真实 Python 3 后执行
> `python scripts/run_extra_scenarios.py`。
> cmpt（混合实例容器）场景仍未触发：需要"同瓦片既有 b3dm 又有 i3dm"的输入，
> 现有 12 资产与 4 个 FBX 样本都不产生该形态（手册 8.4 允许，非缺陷）。

## 3. 单元测试

```bash
bash scripts/wb_build.sh test_production
./Release/test_production.exe            # 全部
./Release/test_production.exe pipeline   # 只跑端到端覆盖用例
./Release/test_production.exe proxySkip  # 只跑远景代理降级 + 失败关闭用例
./Release/test_production.exe bounds     # 只跑包围体发布闸用例
```

**82/82 通过**（2026-10-19 12:16:54，HEAD `ee6499e` + A-31/A-32 工作树，
`test_production_result.txt` 同步落盘，防退出阶段丢缓冲）：

> 当前工作树（A-33 i3dm 三角形预算 + A-34/A-35 manifest 断言 + A-36/A-37 包围体
> 发布闸单测并入）实测 **130/130 通过**（2026-10-19 17:51，
> `bash scripts/wb_build.sh test_production && ./Release/test_production.exe`；
> 逐用例标题与逐用例计数落在 `evidence/test_production-130.txt`，
> `test_production_result.txt` 只有末行计数，它是 `tests/production/main.cpp`
> 自己 truncate 写的，别把 stdout 重定向到同名文件）。
> 上一档 118/118 的证据保留在 `evidence/test_production-118.txt`。
>
> **2026-10-20 00:22 复测：150/150**。新增的是 A-41 的端到端位姿用例 `instancePoses`
> （11 条，含"特征表 POSITION 逐条 = `inverse(原型世界)·实例世界`"、"GLB 原型几何不被连乘"、
> "GLB 节点不带 TRS"）与 A-42 的 `noLocalPaths`（9 条，其中 3 条是路径探测器的**自检正例**）。
> 计数轨迹 118 → 130 → 141 → 150，各档证据文件同目录保留。
>
> **harness 的一次真 bug**：加进包围体用例后全量跑一度只报 71 条、后来干脆
> 中途没输出。原因不是"用例互相干扰"，而是 `is >> json` 解析 `1e999` 会抛
> `json.exception.out_of_range.406`（nlohmann 不把溢出折成 +inf），异常穿出用例
> 函数 → `terminate()` → 进程直接没了，stdout 块缓冲的计数一起丢。现在 main 的
> 每个用例外面包了 try/catch 并记 `TERMINATE` 兜底，另外逐用例进度写
> `test_production_progress.txt`：**任何一条用例崩溃都会留下 `>> name` 而没有
> `<< name`**，再也不会出现"少了几十条但没人报错"。用例本身改成 NaN→`null`
> 的真实序列化路径（JSON 里根本没有 inf/NaN 字面量，见 AUDIT A-36）。
> 新增的是 `testPipelineLeafCoverageInvariant` 里按 tileUri 反查聚合代理身份的一组断言，
> 详见 AUDIT A-34「测试覆盖与变异测试」。
- ProductionProfile 硬上限校验（超限抛异常）
- SourceModel 世界变换烘焙正确性
- 分区器 coverage + 确定性（同一输入两次划分结果一致）
- 巨型单网格三角形级切分
- 误差求值器解析用例（同面=0 / 平行偏移 0.5m / 单侧为空=保守上界）
- feature manifest 跨瓦片回查
- **BatchIdVisitor 回归**：属性 0 占用→搬到空闲槽并写 `_ATTRIBUTE_<n>`；
  属性 0 空闲→直接写 _BATCHID；二次处理→不再搬动
- **Simplifier 多 primitiveSet 回归**（A-26）：合并几何必须真降面且属性/索引不错位，
  绝不清空 vertex array
- **聚合代理覆盖回归**（A-27）：每个占用 cell 都要有代理三角形，且误差在
  单个 cell 尺度内（旧行为 lower=497 m 的整块空洞）
- **端到端管线覆盖不变量**（A-28，首个跑完整 `run()+finalize()` 的用例）：
  合成 8×8 网格断言 64 个源 feature 全部落在叶瓦片且 `tileset.json` 发布；
  对照组用不可能的字节硬限断言 `finalize()` 返回 false 且输出目录**不留**
  `tileset.json`（失败关闭非空转，另用注入单个覆盖缺口的实验验证过该闸会报
  `1 source features are absent from every leaf tile`）
- **远景代理降级 + 失败关闭**（A-31，`proxySkip`）：合成 8×8 网格（64 对象 512 面）、
  `strictIdentity + lockSeamBorder`、硬限 150 面 / 代理目标 100 面。实测根代理压不到
  目标 → `proxySkippedCount=1`，该节点 `hasContent=false`、产物目录与 `tileset.json`
  都不含 `t0.b3dm`，其余 14 个瓦片正常发布、`maxTrianglesInTile=96`、
  `featuresMissingFromLeaves=0`、warning 原文记录"children still cover 64 source
  features"（手册"不能静默缺内容"）。对照组把 `maxTreeDepth` 压到 1 →
  `finalize()=false` 且不留 `tileset.json`。
  写盘后实测超限那条分支（会删除超限文件）是"估算达标但实测超限"的防御网，
  本用例命中的是构建期分支，没有为该文案伪造超限代理，见 AUDIT A-31 验证段。
- **包围体发布闸**（A-36/A-37，`bounds`，12 条断言）：合成 4×4 网格只 `run()`
  不 `finalize()`（暂存内容文件必须留在盘上，`selfValidate` 的 URI 检查才真实），
  然后把临时 tileset 交给 `selfValidate` 逐条探针。覆盖：三轴同时紧贴父盒内壁
  必须放行（`eps` 是 1e-6 相对容差，等号不能误伤）、X/Y/Z 各自单独越界必须**报对
  轴号**、负半轴拒绝、NaN 半轴序列化后拒绝、几何合法时零命中。
  探针只统计包围体类失败，否则 URI/预算/覆盖检查会把"闸有没有生效"淹成"总共
  报了几条"。

## 3b. 包围体发布闸的边界：JSON 装不下非有限值

第一版用例想用 `[0,0,0,1e999,...]` 直接喂一个 +inf 半轴给 `selfValidate`，结果
解析阶段就抛 `json.exception.out_of_range.406`。这不是闸的漏洞，是 JSON 的性质：

- `inf`/`NaN` 没有合法字面量，`1e999` 这类溢出 nlohmann 直接抛异常，不折成 `inf`；
- 反过来 `dump()` 会把非有限 double 写成 `null`。

所以管线内部产生的 NaN/inf 到磁盘上只会以 `null` 出现，`readBox` 的
`is_number()` 检查正好拦住它——用例因此改成断言真实链条：
NaN 半轴 → `dump()` 出 `null` → 解析回来 → 发布闸拒绝。`selfValidate` 里
`std::isfinite` 那几条不再能被合成数据触发，它防的是**别的写入者**（手写
tileset、上游工具）；这条限制如实记录，不假装单测覆盖到了。


## 4. 视觉验收（手册 15 节）

采集工具是无头 Edge + 验收页，不是 python 脚本（本机 `python` 是 Store 存根，
见 STATUS B-5；`scripts/visual_capture.py` 已废弃）：

```bash
bash scripts/visual_capture.sh <session> <tileset-url> [paths] [maxFrames] [port]  # 单数据集
bash scripts/visual_sweep.sh                                                     # 13 个数据集 × 10 条路径
node scripts/va_summary.js <sessionFilter>                                       # 会话级汇总
node scripts/va_paths.js <sessionFilter>                                         # 逐相机路径明细
```

页面注册了 10 条固定相机路径（`docs/visual-acceptance/index.html` 的 `PATHS`），
每帧落 PNG + 一条 JSONL 记录，判据口径见 `docs/visual-acceptance/README.md`
「每帧量化度量」：投影半径 ≤20 px 的帧没资格判空洞，记 `tooSmall`。

实测（CesiumJS 1.119 + 无头 Edge + SwiftShader 软件 WebGL，1280×720）：

| 会话 | 数据集 | 帧 | 出图 | 空洞 | 判不了 | cameraFinite=false | 报错 |
|---|---|---|---|---|---|---|---|
| `fa_t01_plane_curved` | t01（10 条路径全跑） | 69 | 69 | 0 | 0 | 0 | 0 |
| `full_t02` | t02_thin_features（9 条路径） | 34 | 30 | 0 | 4 | 0 | 0 |
| `full_longxiang` | longxiang_covfix（9 条路径） | 34 | 34 | 0 | 0 | 0 | 0 |

- 截图目录 `docs/visual-acceptance/runs/<会话>/`，每个会话另有 `summary.json`
  与 `manifest.jsonl`（逐帧相机经纬高、可见 tile URI、geometricError、
  `tileStates`、`env.gpuRenderer`）；
- `full_t02` 的 4 个 `tooSmall` 全是 `lod-threshold` 路径贴在 refine 距离 ±6 %
  上的帧：薄特征在那个距离投影不到 20 px，按口径不能判空洞；
- 相机 NaN 缺陷（`up ∥ direction` 时 Cesium 重算 `right` 得 NaN 并污染后续所有帧）的
  阳性对照**实测只覆盖 t01 一个数据集**：`fa_t01_plane_curved`（69 帧、空洞 0，12 个
  `ax_*` 帧中位剪影 65 674–819 868 px）。
  **本节此前把这张表的适用范围与数值都写宽了**。原文"修复前 `ax_t05_instances` /
  `ax_t08_big_coords` 各 12 帧 `cameraFinite=false` 12、空洞 12；修复后同样 12 帧
  `cameraFinite=false` 0、空洞 0（中位剪影 65 674–819 868 px）"，与当前产物对不上：
  `node scripts/va_summary.js ax_` 实测 `ax_t05_instances` = 12 帧 / 空洞 **6** /
  tooSmall **6** / camBad **0**，`ax_t08_big_coords` = 12 帧 / 空洞 12 / camBad **0**；
  NaN 在这批记录里的形态是 6/12 帧 `camera.lon|height = null` + `maxProj=1e6`，
  **不是** `cameraFinite=false` 计数。而"修复后…空洞 0、中位剪影 65 674–819 868 px"
  那两个数出自 `fa_t01_plane_curved`，**属于 t01、不属于 t05/t08**。
  所以：t05/t08 的"六轴修复后视觉验收"**未做**，不能引用为已通过；
  `cameraFinite` 这个判据字段本身也漏报了 NaN（camBad=0 而相机确实是 null），
  属于采集页的判据缺陷，记在下面的未做项里一起补。
- `t01` 的 `lod-threshold` 路径产生 0 帧不是漏跑：该数据集只有 1 个根瓦片且
  `geometricError=0`，没有 refine 距离可采样。
- **A-41 对本节的约束（重要）**：即便补跑 t05 的视觉验收，"空洞/coverage"判据也
  **验不出实例位姿**。判据看的是"投影视锥里有没有几何"，而 A-41 的症状是 299 个实例被叠到
  **同一个位置**——画面照样有几何、剪影照样非零，空洞判据天然通过。所以本节里 t05/任何
  i3dm 资产的"无空洞"只证明**能渲染**，不证明**渲染得对**；位姿正确性由 §2 S3 的
  `scripts/i3dm_pose_check.js` 负责（逐实例位置，不看画面）。
  现存所有 t05/t08/longxiang/v15 的视觉批次都产自**修复前**的数据集，画面本身即 A-41
  的错版，这些截图不得作为"实例渲染正确"的证据引用。

尚未完成（不是缺陷，是本机 CPU 被 335 MB 转换占住）：用最终二进制把
13 个数据集 × 10 条路径的整批扫一遍（`bash scripts/visual_sweep.sh`，
一条命令含起服务和汇总）。当前三列结论分别来自修复前后不同批次，
表里已注明路径条数差异。
新增两项（本轮复核产物时发现，见上面 §4 的更正段）：① `ax_t05_instances` /
`ax_t08_big_coords` 的**修复后**六轴批次要重跑——现存只有修复前批次，而此前文档
把它们当成了"修复后 0 空洞"的证据；② 采集页的 `cameraFinite` 判据在相机为
`null` 时仍报 `camBad=0`，这个漏报要先修（否则任何"相机健康"结论都不可信），
修完再谈重跑。两项都必须用 **A-41 修复后的二进制**产出的数据集做，
旧截图（含 `full_longxiang`、`mx_*`、`d_*`）都是 A-41 错版，不能续用。

环境限定：headless Edge 只能拿到 SwiftShader 软件 WebGL
（`env.gpuRenderer` 每条记录里都如实带着），**人眼级接缝/LOD_POP 主观判定
需在有物理 GPU 的机器上复跑**（命令见 `docs/visual-acceptance/README.md`），
该项标 BLOCKED-VISUAL；软件渲染的每帧耗时也不得当性能数据上报。

## 5. 逐资产误差实测（conversion-report.json，2026-10-19 复测）

`rel%` = 误差 / 根瓦片包围盒对角线 × 100（由 `tileset.json` 的 `boundingVolume.box`
实测算出，不是估计）。绝对值单独看没有意义，必须按场景量纲归一。

| 资产 | 对角线 (m) | errMax (m) | errP95 (m) | errMean (m) | rel errMax% | rel p95% | certified | boundLoose |
|---|---|---|---|---|---|---|---|---|
| t01_plane_curved | 141.5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| t02_thin_features | 142.4 | 40.35 | 40.00 | 13.03 | 28.33 | 28.08 | 3 | 1 |
| t03_giant_single_mesh | 565.8 | 131.27 | 75.46 | 54.21 | 23.20 | 13.34 | 0 | 3 |
| t04_many_objects | 568.6 | 317.14 | 264.92 | 81.92 | 55.77 | 46.60 | 63 | 59 |
| t05_instances | 3 469 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| t06_nonuniform | 2 000 | 754.27 | 556.35 | 75.64 | 37.71 | 27.81 | 13 | 13 |
| t07_zero_axis | 89.5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| t08_big_coords | 1.606e+7 | 1.607e+7 | 1.393e+7 | 4.591e+6 | **100.03** | 86.72 | 3 | 3 |
| t09_cross_tile_object | 1 241 | 386.77 | 308.13 | 136.03 | 31.16 | 24.82 | 3 | 3 |
| t10_material_heavy | 279.5 | 123.29 | 97.40 | 34.31 | 44.10 | 34.84 | 3 | 3 |
| t11_sparse_outlier | 5 692 | 5 643.16 | 5 640.41 | 1 132.52 | **99.14** | 99.09 | 7 | 6 |
| t12_big_texture | 169.7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

判读与如实说明：
- `certified` 表示上界含真实采样；`boundLoose` 表示未采样面片用了保守全局上界
  （手册 9.3 允许）；
- 顶层 `errorMax` 是**根代理**的误差，即"远景 LOD 完全不渲染源几何时的偏差"，
  3D Tiles 语义下它等于根 `geometricError`，量纲本来就到场景尺度；细化到
  `maximumScreenSpaceError = 4.0` 阈值以内才会渲染叶层全量源几何；
- **t08（rel 100 %）与 t11（rel 99 %）不是可以忽略的"量纲放大"**：t11 是专门
  设计的稀疏离群资产，根代理无法用聚合抽稀表达一个孤立的远处部件，误差就
  等于"到该部件的距离"≈整个包围盒；t08 是大坐标资产，同一现象。二者结构
  合法、覆盖不变量 0 缺口、细化阈值仍受 SSE 控制，但**远景是否出现可感知的
  部件瞬现（pop-in）必须靠真实 GPU 人眼验收**，本机只有 SwiftShader
  软件 WebGL → 该项记 STATUS.md B-6，标 BLOCKED-VISUAL，不写"符合预期"；
- 其余 10 个资产 rel errMax ≤ 56 %，与场景量级一致。


## 6. 真实 FBX 端到端（输出到 F 盘）

### 6.1 longxiang.fbx（66 MB，123 张纹理）— 2026-10-19 复测，HEAD `ee6499e`

```bash
cd Release && ./model23dtiles.exe -sp \
  -i "D:/BaiduNetdiskDownload/longxiang.fbx" -o F:/wb_3dtiles_out/longxiang_covfix
```

**结果：`errors=0` 且 `tileset.json` 发布**（产物 129 瓦片 + 85 张 KTX2 纹理，
`F:\wb_3dtiles_out\longxiang_covfix\`，日志 `longxiang_covfix.log` 3 524 行）。

| 指标 | 实测值 | 2026-10-16 旧值（commit bfa43a0） |
|---|---|---|
| tiles / 内容瓦片 | 129 / 127（深度 12） | 69 / 67（深度 11） |
| 单瓦片最大内容字节 | 2 226 920（8 MiB 的 26.5 % ✓） | 7.91 MiB（贴硬限） |
| 单瓦片最大三角形 | 34 675（≤ 120 000 ✓） | 74 434 |
| 源 feature / manifest 出现 | 145 / 623 | 145 / 1 013 |
| **叶覆盖缺口** `featuresMissingFromLeaves` | **0** | 无此指标 |
| **代理抽稀丢失** `proxyDroppedFeatureCount` | **0** | 无此指标 |
| 聚合代理 / 实例瓦片 | 10 / 6（instanceCount 10） | — / 6 |
| 误差 max / p95 / mean | 168.37 m / 100.51 m / 15.10 m | 168.4 / 78.4 / 19.7 m |
| 误差状态 | certified 58 / boundLoose 41 | 30 / 23 |
| KTX2 纹理（hash 去重） | 85 个 / 14.1 MiB | 186 个 / 31.6 MB |
| 耗时 / 峰值 RSS | 93.4 s / 2 401 MiB | 629.9 s / 650 MiB |

三点如实说明（都不是"看起来更好"的数字）：
1. 瓦片数 69→129、最大瓦片 7.91 MiB→2.17 MiB：A-30 之后预算判据只卡几何字节，
   聚合代理能在硬限内收敛，分区器因此可以按更小的目标继续切，不再出现贴边瓦片；
2. manifest 出现次数 1 013→623：切分更细 → 同一 feature 被复制进更少的祖先瓦片；
   源 feature 集合不变（145），覆盖不变量仍为 0 缺口；
3. **峰值 RSS 650 MiB→2 401 MiB** 是本轮改动带来的内存回退，耗时则从 629.9 s
   降到 93.4 s（远景代理不再因纹理超限反复整轮白跑）。内存回退尚未定位到
   单一分配点，记 STATUS.md B-7 跟踪，PERFORMANCE_REPORT.md 如实并列两组数字。

该模型历史上暴露并修复的缺陷：commit bfa43a0 的三项（字节估算低估 4 倍、
`meshopt_simplify` 因误差上限过宽不削减、代理只判三角形预算），以及本轮的
A-30（纹理字节被当硬约束导致聚合 fallback 永不收敛）。

### 6.2 其余真实 FBX

| 输入 | 大小 | 状态 | 证据 |
|---|---|---|---|
| `20240529卢沟桥分洪枢纽.fbx` | 335 MB | 进行中（`lugouqiao_v14`） | A-31 之前 7 个内节点代理超 120 000 面 → 整批判失败；`lugouqiao_v11.log` 是失败关闭的真实证据 |
| `芜湖水厂总装.fbx` | 132 MB | 未跑 | 命令同 6.1 |
| `jianzhu+tietu.FBX` | 433 MB | 历史产物 `jianzhu_tietu_v5`（旧二进制，不含 A-26…A-31） | 需用当前 HEAD 复跑 |

## 7. OGC 3D Tiles Validator（官方，B-3 已解除）

不是自研等价校验，是 CesiumGS 官方 `3d-tiles-validator@0.6.1`：

```bash
# 第二个参数是报告目录，缺省 docs/model23dtiles-production/validation
bash scripts/run_validator.sh F:/wb_3dtiles_out/matrix_texless
bash scripts/run_validator.sh F:/wb_3dtiles_out/longxiang_covfix
bash scripts/run_validator.sh F:/wb_3dtiles_out/cmpt_probe
```

脚本内含本机踩过的安装坑（写进脚本而不是让人重踩）：validator 依赖链里的
`sharp@0.32` 用 prebuild-install 从 **github.com** 拉 libvips，本机 github 不可达 →
先 `npm i --ignore-scripts` 装 validator，再从 registry 装 `sharp@0.33.5`
并覆盖三处嵌套的 `node_modules/*/node_modules/sharp`。

结果（2026-10-19 实测，报告与原始 stdout 全在
`docs/model23dtiles-production/validation/`）：

| 数据集 | errors | warnings | infos |
|---|---|---|---|
| t01_plane_curved | 0 | 0 | 1 |
| t02_thin_features | 0 | 0 | 7 |
| t03_giant_single_mesh | 0 | 0 | 7 |
| t04_many_objects | 0 | 0 | 127 |
| t05_instances | 0 | 0 | 0 |
| t06_nonuniform | 0 | 0 | 31 |
| t07_zero_axis | 0 | 0 | 1 |
| t08_big_coords | 0 | 0 | 7 |
| t09_cross_tile_object | 0 | 0 | 7 |
| t10_material_heavy | 0 | 0 | 7 |
| t11_sparse_outlier | 0 | 0 | 15 |
| t12_big_texture | 0 | 1 | 0 |
| longxiang_covfix（真实 FBX） | 0 | 81 | 40 |
| cmpt_probe/tileset.json（`mixed.cmpt`） | 0 | 0 | 0 |

**14/14 `numErrors=0`**，包括真实 FBX 数据集和 cmpt 混合容器探针。

> **复跑口径更新（同日 18:00Z+，报告目录 `validation-matrix-a34/`）**：同一
> validator 对 `runs/matrix_a34/` 的 12 个资产跑出的不是 0——t02 报 **3 个 ERROR**
> （`/meshes/1/primitives/0/attributes/NORMAL`，`Vector3 at accessor indices
> 234..236 / 246..248 / 258..260 is not of unit length: 0`），其余 11 个资产 0 error，
> t12 仍是那条 KTX2 warning。也就是说"14/14 零 error"对**上一批产物**成立，但
> **不能**读成"产物永远不会触发它"：这条 error 正是 AUDIT A-38 那个污染顶点的
> 独立佐证，同一资产同一二进制既能 0 error 也能 3 error，说明缺陷是间歇的。
> 官方 validator 因此从"通过过一次"降级为"每轮必跑、且必须逐资产看错误条数"。

warning 全部同源、且不成立为缺陷：validator 自带的 glTF schema 早于
`KHR_texture_basisu`，对 `mimeType: image/ktx2` 报
`Invalid value "image/ktx2"` / `Image format not recognized`
（longxiang 81 条 = 85 张 KTX2 里去重后仍被逐图引用的部分，t12 1 条）。
CesiumJS 1.119 侧同一批 `.ktx2` 在 §4 的截图链路里实际解码并贴上了纹理
（判据 `colorSpread.chromaFraction`，见 `docs/visual-acceptance/README.md`）。
升级 validator 依赖链里的 glTF schema 才会消掉这批 warning，本轮不为此改动产物。

## 8. 写盘后回读核对（B-9 已解除）

手册 §3 有四条硬限，工具内发布闸只覆盖字节 / 三角形 / 树深三条，
「每瓦片 ≤ 24 个 glTF primitive」过去只有分区侧的**估算**参与切分决策，
没人读回真实产物。新增 `scripts/glb_prims.js`：从 `tileset.json` 取全部
`content.uri`，剥 b3dm / i3dm 外壳（**b3dm 头 28 B，i3dm 头 32 B**，多一个
`glTFFormat`；GLB 起点 = 头 + feature/batch 四张表长度之和，实测 `mixed.cmpt`
的 b3dm 因表占 260 B，GLB 落在 288 而不是固定 28），解析 GLB 的 JSON chunk，
统计 `mesh.primitives` 之和与 accessor 面数；i3dm 再乘 feature table 的
`INSTANCES_LENGTH` 得到运行时真实绘制量。cmpt 也能拆开逐内层瓦片判定。

```bash
node scripts/glb_prims.js docs/model23dtiles-production/runs/matrix_final/t0* \
                            docs/model23dtiles-production/runs/matrix_final/t1* \
                            F:/wb_3dtiles_out/longxiang_covfix F:/wb_3dtiles_out/cmpt_probe
```

结果 **341 个内容文件、0 违例、0 解析失败**（上限 prims=24 / expandedTriangles=120 000）：

| dataset | tiles | max prims/tile | max tris（去重） | max tris（实例展开） | 展开合计 tris |
|---|---|---|---|---|---|
| t01_plane_curved | 1 | 1 | 3 042 | 3 042 | 3 042 |
| t02_thin_features | 7 | 2 | 572 | 572 | 1 716 |
| t03_giant_single_mesh | 7 | 1 | 34 296 | 34 296 | 235 845 |
| t04_many_objects | 127 | 2 | 14 400 | 14 400 | 100 800 |
| t05_instances | 2 | 1 | 12 | **3 588** | 3 600 |
| t06_nonuniform | 31 | 2 | 2 412 | 2 412 | 12 060 |
| t07_zero_axis | 1 | 2 | 2 404 | 2 404 | 2 404 |
| t08_big_coords | 7 | 2 | 720 | 720 | 2 160 |
| t09_cross_tile_object | 7 | 2 | 492 | 492 | 1 476 |
| t10_material_heavy | 7 | 2 | 480 | 480 | 1 440 |
| t11_sparse_outlier | 15 | 2 | 1 824 | 1 824 | 7 296 |
| t12_big_texture | 1 | 1 | 6 962 | 6 962 | 6 962 |
| longxiang_covfix（真实 FBX） | 127 | 11 | 34 675 | 34 675 | 737 115 |
| cmpt_probe（`mixed.cmpt`） | 1 | 1 | 12 | 3 588 | 3 600 |

三点读法：
1. **硬限核对全部通过**，而且是**读回真实产物**得到的，不是工具自报；
   b3dm 的"去重"列与 `conversion-report.json` 的 `maxTrianglesInTile` 逐个相等
   （t01 3 042 / t03 34 296 / t04 14 400 / longxiang 34 675 …），
   说明写出器与统计口径没有分叉；
2. t05 的 `i2.i3dm` 去重只有 12 面、实例展开 3 588 面（feature table 里
   `INSTANCES_LENGTH=299`）——这两列的差就是 A-33 的量纲错配本身。
   硬限必须按展开列判；旧二进制只看字节，才会让 lugouqiao 的 507 822 面
   实例瓦片静默入库；
3. `cmpt_probe` 一行同时验证了容器拆解与 i3dm 展开口径。

## 8b. 包围体发布闸（A-36 / A-37）

`boundingVolume.box` 此前**不在任何发布检查里**：`boxJson` 会把 NaN 半轴经
`std::max(0.0, nan)` 折成 0、把未初始化包围盒折成 12 个 0，产物 schema 完全合法、
外部 validator 也挑不出错，但 Cesium 用一个塌到原点的零尺寸盒做视锥剔除会把
整棵子树剔掉。手册 17 节明确要求"NaN bounds 为零 / 无错误 bounds 剔除"。

```bash
node scripts/tileset_bounds.js F:/wb_3dtiles_out F:/wb_3dtiles_out/matrix_texless
```

回扫既有 **18 个数据集**（输出存 `evidence/tileset_bounds-before-a37.txt`）：
NaN box 0、负半轴 0、非法 `geometricError` 0、**父盒不覆盖子盒 0**。
据此把两条检查写进 `selfValidate` 1b（有限性 + 三轴覆盖关系，违例即拒绝发布），
而不是留在注释里。

实现过程里检查本身错过一次：第一版把半轴下标写成 `b[4]/b[8]`（盒布局其实是
`[cx,cy,cz, hx,0,0, 0,hy,0, 0,0,hz]`，半轴在 3/7/11），合成夹具当场报 23 条
"父不覆盖子"。是独立的 node 回扫器对同一批真实产物报 0 才把根因定位到检查代码，
没有把这笔账算到产品上（详见 AUDIT A-37 的踩坑记录）。

**源范围拒绝（A-38，2b）与它的取证工具**：1b 的三条检查对这个缺陷全部放行——
污染值是**有限**的、父盒与子盒同源所以"父覆盖子"成立。于是补 2b：有内容的瓦片其
`contentBounds` 必须落在**源模型范围 ± 0.5×源对角线**内，越界即拒绝发布。
取证工具 `node scripts/b3dm_attr_probe.js <数据集目录|文件.b3dm>` 逐 primitive
关联 POSITION/NORMAL/索引并按 `@count-k` 相对下标报位置，用它 + 官方 validator
把 A-38 从"随机堆损坏"收窄成"一份被多个瓦片共享的顶点尾巴里，个别槽位装着另一份
数据"（完整测量与仍未定位的部分见 AUDIT A-38/A-40）。

> **2b 的实测边界**：① 没有合成单测——`contentBounds`/`_model.bounds` 是私有
> 构建期状态，测试无注入点；② 没有在真实数据上跑过一次拦截——写出 `matrix_a34`
> 的二进制（15:47）还没有 2b。因此"2b 会拒掉 t08/t02 那种越界"目前是数值推演
> （6.555e10 ≫ 源 ±50 m + slack 87 m），不是实测记录。带闸二进制的真实数据实测
> 记在 STATUS 待办里，不能用本节文字代替。（注意 2b 比的是**源模型自身**的范围，
> 所以 t08_big_coords 那种"源坐标本来就大"的数据集不会被误伤，它的越界判据是
> 相对源尺度而非绝对值。）

## 9. 验证结论

- **实例位姿（A-41，本轮新增口径）**：代码分两段修好（flatten 掩码 exe 22:40 /
  相对帧 exe 23:53），§1c 的 `matrix_a42fix` t05 用 `scripts/i3dm_pose_check.js` 逐条对上
  `inverse(原型世界)·实例世界` 的 `(x,−z,y)` 置换（**299/299、残差 ≤9.47e-6 m、GLB 无节点 TRS**）；
  **按绝对平移比对必须 0/299**，全命中绝对平移恰恰是"相对帧没生效"的签名——本轮一开始就把
  这条判据写反过，`a41fix_t05`、`matrix_a41fix2` 两批"299/299 命中绝对平移"的 PASS 已作废。
  同一工具对 `matrix_a34/t05`、`lugouqiao_v15` 与上述两批中间产物都判 exit 1。
  **结论只到合成资产**：真实 FBX 的 i3dm 位姿要等 `lugouqiao_v17`（23:53 exe + 00:16 插件；
  `v16` 用的是"相对帧为空操作"的 22:40 二进制，已主动终止）跑完才算验过，
  在此之前**任何带 `.i3dm` 的既有产物一律不可发布**（v15、longxiang*、matrix_a34 等）。
- 结构合法性：12 资产 + 真实 FBX + cmpt 探针在**官方** OGC validator 下
  上一批产物 `numErrors=0`（§7），但同日复跑 `matrix_a34` 的 t02 报 3 个
  zero-length-normal ERROR（§1b/§7）——**"0 error"不是不变量**，缺陷按 A-38 记，
  内发布闸 `selfValidate` 对那批产物同样全 0 error（当时还没有 2b）；
- 预算控制：超硬限明确失败、零残留（S2 实测）；真实 FBX 127 个内容瓦片
  最大 2.17 MiB，矩阵 12/12 违例 0；实例瓦片的三角形预算见 §6.2 的 A-33；
- 身份映射：manifest 跨瓦片回查单测通过；真实 FBX 145 feature；
  **注意** `longxiang_covfix` 是 A-35 修复前的产物，它的
  `report.manifestOccurrences=623` 与 `manifest.occurrenceCount=571` 那对
  不一致的数字正是 A-35 的现场（差的 52 条是聚合代理条目，本就不该算源出现次数），
  不是可接受的现状；新二进制复跑后两个数必须由 `sourceOccurrenceCount` 同出一个定义
  （`node scripts/check_manifest_assertions.js` 第 1 条断言）；
  `featuresMissingFromLeaves=0` / `proxyDroppedFeatureCount=0`；
  `aggregateProxies` 补全后可由源 feature 反查代理 ID 与其代表的源集合；
- 视觉链路：CesiumJS 加载渲染真实出图，自动化 0 错误；
- 未决：真实 GPU 人眼验收（BLOCKED-VISUAL）、Python 采集器（BLOCKED B-5）、
  lugouqiao/芜湖水厂端到端、longxiang 内存回退（B-7）、
  写盘后 glTF primitive 数回读（B-9）。
