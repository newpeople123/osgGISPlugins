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
| A-35 占用数单一口径 | 12/12 资产 `report.manifestOccurrences == manifest.occurrenceCount`（例如 t04 8414/8414、t06 1015/1015、t11 616/616）。这一轮扩扫到**全部 373 份产物**：修复提交 `262dc77` 之后 0 处不一致，之前残留 5 处（`longxiang_covfix` 571/623、`wuhu_v1` 228/263、`matrix_texless/t03` 4/11、`matrix_covfix/t03` 6/9、`matrix_final/t03` 4/11），差额即落到聚合代理身份上的条目数，与上一行 t03 的"7 条"互相印证；`evidence/manifest_occurrences-a35-boundary.txt` |
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

六条必须一起读：

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
   ① "正常数据集不因 2b 误伤"这一条的证据：12 个合成资产 + **真实 FBX 已有 `lugouqiao_v17`**
   （带闸 + 相对帧 + A-42 插件二进制，`rc=0`、`errors=[]`、1059 个瓦片全部登记、
   没有任何一条 2b 拒发；它是这份 335 MB 输入历史上第一次出盘）。
4. **t09 这一轮回到了 §1/§1b 的基线**（41 120 / 386.77 / cert-loose 3/3，与
   `matrix_a41fix` 逐字节同列）：`matrix_a41fix2` 那次是 A-40 的第 4 个变体，
   `maxBytes/errMax/cert-loose` 随变体动。本表列的是这一次运行的实测值，
   **t09 不是该资产的稳定值**，其余 11 个资产的稳定值与本表一致。
5. **本矩阵里 4 个资产的所有瓦片 `geometricError` 都是 0**（t01/t05/t07/t12）：
   按手册 9.6 `G(n) = max(measuredProxyError(n), max G(child))` 不加 epsilon，
   单细节、没被简化过的 fixture 必然算出 0（叶节点 SHOULD 为 0）。这不是缺陷，
   但**这些资产没有资格用来判 LOD 选择**——能判的只有 t02/t03/t04/t06/t08/t09/t10/t11
   和真实数据集（`longxiang_a42` 根瓦片 168.37 m、67 个叶瓦片全为 0，符合规范）。
   本轮给 `scripts/tileset_bounds.js` 加了"**子级误差大于父级**"这一列并复扫全部产物：
   `node scripts/tileset_bounds.js F:/wb_3dtiles_out/matrix_a42fix F:/wb_3dtiles_out/longxiang_a42`
   → **13 个数据集各列全 0**（父覆盖子 / NaN box / 负半轴 / 非法 ge / ge 递增），rc=0。
   新列做过阳性对照：临时 tileset 把子级 9 塞到父级 5 下面，该列报 **1** 且 rc=1。
   tileset 级 `geometricError` = max(根包围球半径, 根 G)，13 份全部非 0。
   **给探针作者的一个坑**：`boxJson` 把三个半轴按列向量排在 12 元数组的下标 **3 / 7 / 11**
   （`ProductionPipeline.cpp:197-200`），按下标 1..3 读会得到 `span=[0,0,0]`——
   本轮差点据此判成"根包围盒塌陷"的产品缺陷，实际是临时探针读错位置
   （仓库里的 `tileset_bounds.js` 一直读对了，该用工具而不是现写探针）。
6. **几何侧的三条判据已经从"抽样"改成"全量"**：新增
   `node scripts/accessor_extents_scan.js <数据集|容器目录> [...]`，对每个瓦片的
   VEC3 accessor 逐条做**声明 min/max vs 实算**、**POSITION 非有限分量**、
   **`|coord|>1e6` 幅度**三项检查，并把 b3dm 与 i3dm 分开计数。复扫修复前后
   **124 个数据集 / 3 687 个瓦片 / 14 884 个 accessor**：
   前两列**全 0**；幅度列只有两类命中——`matrix_a34/t02` 的 3 个瓦片（就是 A-38
   那唯一一次，t0/t4/t6 与 `evidence/a38-forensics.txt` 逐个对上）和 fixture
   `t08_big_coords`（大坐标用例，本就该 1e7，`--abs 1e8` 即分开）。
   三条判据**互相拦不住**，所以都得留：A-38 那次连声明的 `max` 一起被污染
   （min/max 全等 = 0 命中）；把顶点改成 1234.5 只有 min/max 那条报警；
   改成 `NaN` 只有"非有限"那条报警（NaN 参与 `<`/`>` 恒为 false）。
   三个阳性对照与全部读数见 `evidence/accessor_extents-baseline.txt`。
   **真实 FBX 的口径由此钉死**：`lugouqiao_v15` 全量跑完后，931 个 b3dm 最大只有
   4 337 m、幅度命中 0；`>1e6` 的 4 处全在 i3dm（最大 1.223e8 m = `i1018.i3dm`），
   按 `--abs 1e4` 复扫得 6 个 i3dm 文件，正好等于 A-41 记录的"6 个瓦片烘焙坐标 >1e4"。
   如果两类瓦片混在一个判据里，A-41 的 6 个会被读成"A-38 在真实 FBX 上命中 6 次"。
   **v17 复扫把这条 A-41 签名钉成"随修复消失"**：同一工具对 `lugouqiao_v17` 报
   `i3dm >1e6 瓦片 = 0`、`i3dm max|c| = 383.5 m`（v15 是 6 个命中 / 最大 1.223e8 m），
   b3dm 两列与 v15 一致（0 命中 / 4 337 m），所以归零来自 A-41 而不是判据被改松。

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
> **2026-10-20（下午）复测：passed=181 failed=0**，退出码 **0**（`./Release/test_production.exe`；
> 二进制 13:08，含未提交的 #6/#7 与误差侧两处修复，见 STATUS 同名小节）。
> 计数上升来自两组**新用例 + 阳性对照**：
> `testMeshBVHMatchesBruteForce`（64 三角形 × 6 探针，oracle = 每面一个单三角形
> BVH 的最小距离——单三角形必为叶子，所以精确）与
> `testErrorBoundSoundness`（三段：① 环带 + 内接等边三角形，`maxSamplesPerNode=3`
> 把密度压到"只剩顶点"、细分预算 0，此时上界完全由覆盖半径撑起，口径错就露馅；
> ② 一个 20×20 平面上 60 001 个完全吻合的三角形 + 1 个 z=50 的离群面，
> 验证"密集抽样会漏掉的那一面必须同时进上下界"（旧实现按固定步长抽样，步长没落在
> 它头上时 50 m 的偏差会被整条吞掉）；
> ③ 饥饿预算必须 `BOUND_LOOSE`、放宽 `toleranceMeters=0.05` 必须 `CERTIFIED`）。
> **两组都有阳性对照，不是空跑**：A-45 修复前 ① 精确失败在
> `upper=0.577350 want>=0.980000` —— `0.577350` 就是 `L/3`（`L = √3`），
> 而只剩顶点时合法系数是 `L/√3 = 1.000`，**旧口径把上界写小了**；
> A-44 修复前失败的是 `testMeshBVHMatchesBruteForce`，6 个探针里 **4 个**
> 报出的最近距离大于暴力法真值。
> ③ 用 0.05 m 而非默认 1 mm 是有意的：1 mm 绝对收敛在 1 m 三角形上要 ~2^19 个子块，
> 单测跑不完，这条用例的职责是"两种状态都到得了"，不是"默认门槛能过"。
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

> **2026-10-21 复测：passed=228 failed=0**，退出码 **0**（二进制 11:06，含未提交的
> A-47/A-48/A-49/A-50 与 L1；`bash scripts/wb_build.sh test_production && ./Release/test_production.exe`）。
> 本轮新增/常驻的四组与逐组计数（用过滤器单跑得出来的）：
> - `proxyTexture` **13 条**（用这个过滤器跑会**连带命中** `proxyTextureLadder`，
>   所以 13 = 本体 7 + ladder 6）：代理纹理字节按 `osg::Image*` 去重、下限分辨率折算、
>   阳性对照（不去重就会判成超限）。
> - `proxyTextureLadder` **6 条**：字节超限时的降分辨率档位链，且**叶瓦片必须保留全部档位**。
> - `texResize` **11 条**：把上一轮"用完即删"的克隆语义探针换成常驻回归——
>   先钉住前提（`osg::clone(StateSet, …|DEEP_COPY_TEXTURES)` 出来的纹理
>   `getImage()` **仍是同一张图**），再钉 `resizeImageToPowerOfTwo` 的
>   copy-on-write：源像素/尺寸不动、不往共享图上写 `ORIGIN_*`、副本记录原始尺寸、
>   达上限的图原样返回、300 px 的"低于上限但非 2 的幂"必须仍然拷贝（阳性对照）、
>   空指针透传。见 AUDIT A-49。
> - `bvhOrder` **16 条**（L1）：同一批样本、任意遍历顺序，结果逐位一致。
> - `pipelineCoverage` 里 **5 条仪表接线断言**（A-47）：`micro_texture_export*` 三项阶段存在、
>   `micro_materialize`/`micro_total_cpu` 存在、逐瓦片 `timing` 非零、
>   `micro_evaluate_samples` 在 `counters`、`stages` 里不含 `samples`/`candidates` 字样。
>   **这 5 条只证"接上线"，不证"算得准"**——数值闭合是产物级读数（AUDIT A-47 闭合读数段）。
>
> **一处没有对上的账，如实记**：上一档 181（10-20 13:08 二进制）与本轮 228 之间差 47 条，
> **无法逐条对账**：`proxyTexture` 这个过滤器会**同时**命中 `proxyTextureLadder`
> （13 = 两组之和，不是 13+6），而工作区在 10-20 下午之后还被并发改动过，
> 会话里另有 212 / 223 两次中途读数（223 是 `texResize` 落地后拿的）。
> 所以这里只报"当前实测 228/228 + 逐组过滤器读数"，**不声称能拆到每一条**。
> 要拆账得把 HEAD 的版本单独取出来在别处构建基线（**共享工作区不要用 `git stash`**）。


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

- 上表最后一列在 `full_*` 这批里读作 0 是**字段缺失**造成的，不是"相机全部健康"：
  那 68 帧压根没写 `cameraFinite`（现存 720 帧共 547 帧没这个字段，机制见本节末）。
  `node scripts/va_paths.js` 现在同时给 `camBad`（页面自报）/`camNaN`（按记录里的相机值补判）
  /`noFlag`（缺字段），全量口径实测
  `shots=720 holes=222 tooSmall=27 camBad=9 camNaN=39 noFlag=547`。
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
  所以：t05/t08 的"六轴修复后视觉验收"**未做**，不能引用为已通过。
  **camBad=0 的机制本轮已查清并改掉**（此前我写成"flag 报 true 而相机是 null"，
  逐条核对产物后确认不是这样）：`runs/*` 的 720 条 `kind:"shot"` 记录里
  `cameraFinite` **字段缺失 547 条**（含全部 `ax_*`/`mx_*`/`full_*` 批次），
  而 `va_paths.js` 判的是 `=== false`，"缺字段"因此被读成"没问题"；旧判据又只查
  `positionWC`/`directionWC`，**没查真正落盘的 `camera.lon/lat/height`**，
  NaN 经 `JSON.stringify` 静默变成 `null`。NaN 记录 39 条，
  而"flag=true 且相机非有限"的记录 **0 条**。
  修法：采集页改用 `cameraFiniteNow()`（世界坐标 + 视线方向 + cartographic 经纬度高程
  一起查，`positionCartographic` 缺失时判 false 而不再抛异常），落盘数值统一走 `fin()`；
  `va_paths.js` 增加 `camNaN`/`noFlag` 两列按记录值补判（`camBad` 列语义不变，
  历史引用数字仍可读），旧批次不重跑也能看出损伤面——
  `node scripts/va_paths.js ax_` 现报 `shots=60 holes=60 camBad=0 camNaN=30 noFlag=60`。
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
把它们当成了"修复后 0 空洞"的证据；② 采集页的相机判据漏报**本轮已修**（机制见上：
缺字段 + 判据没覆盖落盘的经纬高），`scripts/check_page_js.js` 校验通过，
剩下的只是用修好的采集页重跑。两项都必须用 **A-41 修复后的二进制**产出的数据集做，
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

> **本节整张表的误差列已作废（2026-10-20 下午）**。上表是**修复前**二进制的读数，
> 当时 `MeshBVH` 的叶子按数组位置取三角形（`buildRecursive` 只重排了 `order`），
> 最近距离被系统性放大到最多 **3.7 倍**。修复后的同输入矩阵读数见 **§5b**。
> 表中两条依赖绝对误差大小的判读要按新数重述：
> ① "t08 rel 100 % / t11 rel 99 % = 根代理无法表达孤立部件"——**不是**真实几何关系，
> 是新数 **0.037 % / 0.11 %** 说明那两行主要是索引缺陷的产物；
> ② `certified` 那一列的"上界含真实采样"在这份代码下**不可能达成**（见 §5b 末段），
> 所以旧表的 63/13/3 这类合格证计数**不是**可比指标。

## 5b. 修复后逐资产误差实测（`F:/wb_3dtiles_out/mx_bvh`，exe 13:09，2026-10-20 下午）

同一 12 资产输入、同一命令，唯一变量是两处误差侧修复（`MeshBVH` 叶子索引 = AUDIT **A-44** +
覆盖半径口径 = AUDIT **A-45**；另有 **A-46** 那条 `maxSamplesPerNode=0` 的加固，
默认配置下不参与本表差分）。机制与阳性对照见 STATUS 的"2026-10-20（下午）"一节、§3。

| 资产 | errMax 修复前 → **后** (m) | rel errMax% 后 | certified 前 → **后** | boundLoose 前 → **后** |
|---|---|---|---|---|
| t01 / t05 / t07 / t12 | 0 → **0** | 0 | 0 → 0 | 0 → 0 |
| t02_thin_features | 40.35 → **1.276** | 0.90 | 3 → **0** | 1 → **3** |
| t03_giant_single_mesh | 131.27 → **105.561** | 18.66 | 0 → 0 | 3 → 3 |
| t04_many_objects | 317.14 → **17.360** | 3.05 | 63 → **0** | 59 → **63** |
| t06_nonuniform | 754.27 → **8.699** | 0.43 | 13 → **0** | 13 → **15** |
| t08_big_coords | 1.607e+7 → **5 993.585** | **0.037** | 3 → **0** | 3 → 3 |
| t09_cross_tile_object | 386.77 → **4.513** | 0.36 | 3 → **0** | 3 → 3 |
| t10_material_heavy | 123.29 → **1.104** | 0.40 | 3 → **0** | 3 → 3 |
| t11_sparse_outlier | 5 643.16 → **6.387** | **0.11** | 7 → **0** | 6 → **7** |

（`rel errMax%` 用 §5 的对角线列重算，量纲不变。）

**结构侧完全没动**（这是本次修复的关键安全点）：
`tileCount` 12/12 与修复前逐个相同；b3dm/i3dm **内容文件字节级改动 0 个**；
逐瓦片差分 215 个瓦片里**自身上界 100 个变小、0 个变大、115 个不变**，
`upper < lower` 违例 **0**——即修复只把以前虚高的保守上界往下收，
**没有**任何一处把上界写得比实际更接近源表面（不违反手册"不得私自降低上界"，
方向是反的：以前是过保守）。
`node scripts/tileset_bounds.js F:/wb_3dtiles_out/mx_bvh` → **12/12 数据集干净**；
官方 validator `bash scripts/run_validator.sh F:/wb_3dtiles_out/mx_bvh F:/wb_3dtiles_out/validation_mx_bvh`
→ 12 个 tileset **errors=0**（仅 t12_big_texture 1 条 warning），逐份报告在
`F:/wb_3dtiles_out/validation_mx_bvh/*.validator.json`。

**待裁定（不由我代为决定）**：距离算对之后 `nodesCertified` 在 12 个资产上**全部为 0**，
所有有内容的节点都是 `BOUND_LOOSE`。原因是 `CERTIFIED` 判据要求
"上界 − 下界 ≤ `toleranceMeters`=1 mm 绝对值，或 ≤ 上界的 `certifyRatio`=2 %",
而上界与下界之差就是 patch 的覆盖半径 ≈ patch 尺寸，在 `maxSamplesPerNode`=40 万的预算内
数学上够不到。产物本身仍然安全（上界只会偏大），但"合格证"这一指标在这份代码下失效。
合法出路只有两条：加大 `maxSamplesPerNode`（纯烧时间，与 1~2 h 目标直接冲突），
或接受诚实的 `BOUND_LOOSE` 并改判据描述。**调低 `toleranceMeters` / 放宽 `certifyRatio`
属于放宽验收，按手册禁止，本轮没有动。**


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
| `20240529卢沟桥分洪枢纽.fbx` | 335 MB（335 435 216 B） | **已跑通 `lugouqiao_v17`**（10-20 00:35→08:35，`totalSeconds=28 485.37`，rc=0；`tileCount=1059`、1057 个分发文件 = 931 `.b3dm` + 126 `.i3dm`、471 个 `.ktx2`；`errors=[]`、`maxTrianglesInTile=119 840`、`maxContentBytes=3 237 360`、`maxDepth=18`、`peakRssBytes=9.83 GB`） | 仓库电池**逐条 rc=0**（退出码不带管道取），明细见下面"v17 电池读数"段。**仍未做/未过**：官方 validator 已跑但 **rc=1 / 26 个内容文件 ERROR**（A-43 零长度 NORMAL，见 §7 的 2026-10-20 复跑），视觉验收还没做 |
| `芜湖水厂总装.fbx` | 132 MB（132 484 352 B） | **跑过但作废**：`wuhu_v1` 用 A-41/A-42 之前的二进制（report mtime 10-19 12:34 本机，`totalSeconds=207.53`，`tileCount=62` / 内容瓦片 `60` = 55 `.b3dm` + 5 `.i3dm`，`errors=[]`），manifest 仍是 `schema v1` | 本轮盘上实测两条 FAIL：`path_leak_structural` **rc=1**，瓦片 JSON 区 **226 处**泄漏（`glbJSON_0/bufferViews/N/name`、`images/N/name` = `F:/wb_3dtiles_out/wuhu_v1\textures\…`）+ 旁证 json 87 处 `D:/BaiduNetdiskDownload/芜湖水厂总装.fbx/…`；`i3dm_pose_check` **rc=1**，5/5 个 i3dm `POSITION` 全 0、19 条实例位姿 0 命中（工具直接判"manifest 缺 prototypeTransform，v1 产物无法验收，必须用修正后的二进制重跑"）、原型 `\|v\|` 最大 **52 494 m**。`report.errorMaxMeters=37 508.98` 是同一污染的连带读数，不能当误差结论。**需用当前 HEAD 复跑**（命令同 6.1） |
| `jianzhu+tietu.FBX` | 433 MB（432 952 384 B） | **从未产出过数据集**——旧文案"历史产物 `jianzhu_tietu_v5`"是失真，已更正 | 5 个目录（`jianzhu_tietu`/`_v2`/`_v3`/`_v4`/`_v5`，`du -sb` 实测 238 862 431–1 008 345 919 B）**没有一个有 `tiles/`、`tileset.json` 或 `conversion-report.json`**；只有 `textures/`（57/66/73/93/85 个）与 `.wb-staging/tiles/` 里的半成品 b3dm（60/77/310/343/84 个，5 份 i3dm 计数全为 0）→ 5 次都停在写出中途。`v5/stderr_live.log` **0 字节**；`v3`（170 435 B）与 `v4`（186 245 B）末尾分别是 `evaluate pn=286 srcT=330 206 proxT=22 961` 和 `pn=221 srcT=1 320 823 proxT=19 380`。staging 有残留说明这几轮**没走到 §14.17 的失败清理**（候选：进程被外部终止），终止原因本轮未查。**结论：这份输入没有任何可验收产物**，与"旧二进制产物"是两回事 |

**v17 电池读数**（2026-10-20 本机逐条跑、逐条取 `$?`，命令都在 `scripts/`）：

| 判据 | 读数 | rc |
|---|---|---|
| `i3dm_pose_check.js` | 相对帧 **1787/1787** 对上、残差最大 **1.85e-5 m**、绝对帧比对 0 命中、POSITION 全 0 的瓦片 **0/126**、GLB 节点 TRS 0、原型 `\|v\|` 最大 383.48 m | 0 |
| `accessor_extents_scan.js` | 931 b3dm / 126 i3dm / 4430 个 VEC3 accessor：声明≠实算 **0**、非有限分量 **0**、`>1e6` b3dm **0** 且 i3dm **0**；`\|c\|` 最大 b3dm 4 337 m、i3dm 383.5 m | 0 |
| `path_leak_structural.js` | 1057 瓦片 + 3 旁证 **0 泄漏**、尺子失效 0（`path_leak_scan.js` 那 4 处是 BIN chunk 字节噪声，见 `evidence/a42-path-leak-v17-adjudication.txt`） | 0 |
| `tileset_bounds.js` | 父覆盖子 / NaN box / 负半轴 / 非法 ge / ge 递增 全 0 | 0 |
| `glb_prims.js` | 1057 个内容文件 **违例 + 解析失败 = 0**：`max prims/tile=11`（`t53.b3dm`）；A-33 口径的**实例展开**三角形最大 **119 840**（`i1042.i3dm`）与 `report.maxTrianglesInTile` 逐值对上，去重侧最大 37 885，展开合计 11 350 646 | 0 |
| `check_manifest_assertions.js` | `aggProxies=122 / aggBuilt=122 / aggSkipped=0`、`occurrences=4250`（= `report.manifestOccurrences`）、`maxTris/tile=119 840` | 0 |
| `zero_normal_triage.js` + `zero_normal_tile_class.js` | 195 条零长度 NORMAL / 140 退化 / 55 非退化 / 19 个文件——与 v15 同签名，A-43 判为**源侧自带**、不是回归信号。**注意这把自研尺子只统计不拦**：同样 195 条在官方 validator 里是 **ERROR**（见 §7） | 0 |

这七项**不含**官方 validator 与视觉验收：官方 validator 已在 2026-10-20 对 v17 跑过，
**判 26 个内容文件 ERROR（全部是 A-43 的零长度 NORMAL），rc=1**，读数与拆分见 §7；
视觉验收的判据本身还在重做（STATUS 未闭合 7）。所以 v17 目前的准确表述是
"仓库自研判据全绿 + `rc=0` 出盘，**官方 validator 未过闸**"，不能写成"发布验收通过"。
上面三行的原始读数（含命令、退出码、jianzhu 的 5 目录清单、`wuhu_v1` 两条 FAIL 全文、
220 份 report 的字段扫描）在 `evidence/real-fbx-status-2026-10-20.txt`。

## 7. OGC 3D Tiles Validator（官方，B-3 已解除）

不是自研等价校验，是 CesiumGS 官方 `3d-tiles-validator@0.6.1`：

```bash
# 第二个参数是报告目录，缺省 docs/model23dtiles-production/validation
bash scripts/run_validator.sh F:/wb_3dtiles_out/matrix_texless
bash scripts/run_validator.sh F:/wb_3dtiles_out/longxiang_covfix
bash scripts/run_validator.sh F:/wb_3dtiles_out/cmpt_probe
# 2026-10-20 本轮（结论见下面的"2026-10-20 复跑"；报告必须显式给第二个参数，
# 否则会覆盖 validation/ 里 10-19 那批原件——本轮跑完已把产物迁到 validation-2026-10-20/）
bash scripts/run_validator.sh F:/wb_3dtiles_out/lugouqiao_v17      docs/model23dtiles-production/validation-2026-10-20   # rc=1，26 error
bash scripts/run_validator.sh F:/wb_3dtiles_out/longxiang_a42      docs/model23dtiles-production/validation-2026-10-20   # rc=0
bash scripts/run_validator.sh F:/wb_3dtiles_out/matrix_a42fix      docs/model23dtiles-production/validation-2026-10-20   # rc=0，容器目录逐资产
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

### 2026-10-20 复跑：`lugouqiao_v17` **未通过**（26 个内容文件 ERROR）

同一 `run_validator.sh`、同一 `3d-tiles-validator@0.6.1`，对当前三份产物逐条跑、逐条取 `$?`。
本轮 28 份报告（12 资产 + 两份真实 FBX 的 json/log）在
`docs/model23dtiles-production/validation-2026-10-20/`；`validation/` 保持 10-19 那批不动
（脚本默认输出目录会指向前者，本轮先跑后迁，防止覆盖历史原件）。

| 数据集 | errors | warnings | infos | 脚本 rc |
|---|---:|---:|---:|---:|
| `matrix_a42fix`（t01…t12 逐个） | **12/12 全 0** | 仅 t12 有 1 条 | 0…127 | 0 |
| `longxiang_a42`（真实 FBX 65 MB） | **0** | 81 | 40 | 0 |
| `lugouqiao_v17`（真实 FBX 335 MB） | **26** | 682 | 349 | **1** |

**v17 的 26 条 ERROR 全部是同一签名，且与自研尺子逐顶点对上**：叶层 ERROR **195 条**，
消息全为 `Vector3 at accessor indices N..N+2 is not of unit length: 0`，落在
`/meshes/*/primitives/0/attributes/NORMAL`；**非 unit-length 之外的 ERROR = 0**。
按文件拆：7 个文件各 20 条（`t0`/`t57`/`t69`/`t70`/`t551`/`t552`/`t553` = 140 条）+
19 个文件各 2–3 条（`i952`/`i956`/`i957`/`t126`/`t141`/`t158`…`t716` = 55 条），
合计 26 个文件 / 195 条。`zero_normal_triage.js` 对同一数据集独立测得
**零长度 NORMAL 顶点 195 = 只落退化三角形 140 + 落非退化三角形 55，非退化侧涉及 19 个文件**
——两个数量（195 与 140/55、19）与官方 validator 的拆分逐项相同，
所以这 26 条 ERROR 就是 **A-43 的零长度法线**，不是新问题。
旁证：同一工具对 `longxiang_a42` 判 **0 error**，说明它是**输入相关**而非管线必然产物。

**这条结果把 A-43 的定级推回待定**：AUDIT/STATUS 记的是"源侧自带、不需要动构建"，
源侧起源的解释仍然成立（构建没有制造它），但**官方发布闸按 ERROR 计**，
携带它的 `lugouqiao_v17` 因此**不能出盘即发布**。处置方向是写出侧对零长度 NORMAL
重算为面法线（任务 #13），**不得**改用"关掉 NORMAL 属性""把 validator 判 ERROR 的
输入剔除"或"把 ERROR 降成 warning"这类过闸手段。

**处置已于同日落地到代码**（`49b6d3c`）：`SourceModel::repairZeroNormals`
在 `materialize()` 的 `setNormalArray` 之前按三角形拓扑重建零长度法线——入射非退化面优先
（v17 实测 55 条），自身全退化时退到本图元面积加权平均面法线（140 条），整图元都退化则
**保持为零并 `OSG_WARN`**（v17 实测 **0 条**，所以不需要编造方向）。可用法线一律不动。
证据是**单测级**的：`test_production nrmRepair` 17/17、全量 **passed=167 failed=0**、rc=0。
**本节上面那份 rc=1 属于 `lugouqiao_v17`，它不是带修复出的盘**；10:16 起跑的 `lugouqiao_v18`
用的 exe 时间戳 **10:00**（早于 `49b6d3c`），同样不算验收轮。验收判据因此仍是
**重新出盘 → `run_validator.sh` 逐资产看 `severity=ERROR` 叶层条数 → 自研电池复跑**，
在这之前"v17/v18 未过发布闸"的记录不作废。

### 2026-10-20（下午）复跑：误差侧修复后的矩阵 `mx_bvh` → 12/12 **errors=0**

同一条 `bash scripts/run_validator.sh F:/wb_3dtiles_out/mx_bvh F:/wb_3dtiles_out/validation_mx_bvh`
（`3d-tiles-validator@0.6.1`，逐资产取 `$?`，12 份报告落
`F:/wb_3dtiles_out/validation_mx_bvh/*.validator.json`）：
**12 个 tileset 全部 `numErrors=0`**，warnings 只有 t12_big_texture 那 1 条 KTX2 老面孔。
这轮的变量是 §5b 的两处误差侧修复 + 未提交的 #6/#7 并行，
所以它证明的是**这两处修复没有把格式合规性弄坏**（旁证：b3dm/i3dm 内容文件字节级改动 0 个）。
**它不是 A-43 的产物级复验**——`mx_bvh` 的小资产本来就不含零长度法线（修复前也是 0 error），
拦不住也验不出 A-43；v17 那一档仍需"用带 `49b6d3c` 之后代码的二进制重新出盘 → 逐资产数
`severity=ERROR` 条数"，而那一次到现在**还没跑**（`lugouqiao_v18` 已按指令终止，
盘上只有 `textures/`）。
> **上面这句作废于 2026-10-20 下午**：`lugouqiao_v19` 就是那份"带修复重新出盘"，
> A-43 的产物级复验已闭合，见下面一节。

### 2026-10-20（下午）复跑：`lugouqiao_v19` **通过**，A-43 产物级复验闭合

同一条 `bash scripts/run_validator.sh`，两份产物**同一轮、同一台机、同一份 validator**：

| 数据集 | 头 numErrors | 叶层 `severity=ERROR` | 其中 `is not of unit length: 0` | 归组的瓦片数 | `run_validator.sh` 退出码 |
|---|---|---|---|---|---|
| `lugouqiao_v17`（`validation_v17_control/`，阳性对照） | 26 | **195** | **195** | 26 | 1 |
| `lugouqiao_v19`（`validation_v19/`） | **0** | **0** | **0** | 0 | **0** |

- ERROR 叶层逐条路径都是 `/meshes/N/primitives/0/attributes/NORMAL`，
  消息 `Vector3 at accessor indices X..X+2 is not of unit length: 0.` ——与 §7 上面
  2026-10-20 那份 v17 读数同一签名；v19 侧该模式**命中 0**。
- **除 ERROR 之外的叶层逐类计数在两份产物之间完全相同**：
  `This object may be unused` 1969、`Invalid value 'image/ktx2'` 1569、
  `Image format not recognized` 1569、`KHR_texture_basisu` 不支持 691、
  degenerate triangles 7。⇒ 这一轮改动只把零长度法线换成面法线，
  没有顺带改变纹理、图元或索引侧任何东西（这比"v19 全绿"更有说服力）。
- 自研尺同轮对照：`zero_normal_triage.js` 对 v17 报 **195**（退化 140 + 非退化 55），
  对 v19 报 **0**；`zero_normal_tile_class.js` v19 `files=0 zeros=0`。
  这条把"自研尺只统计不拦"的空档补上了：现在两侧数字与官方 validator 逐条对上。
- v19 其余六项电池（`i3dm_pose_check` / `accessor_extents_scan` / `tileset_bounds` /
  `glb_prims` / `check_manifest_assertions` / `path_leak_structural`）**全 rc=0 且读数达标**，
  明细表在 STATUS 的"lugouqiao_v19 全量重跑"一节。
- `path_leak_structural.js` 的**非空洞性**这轮用 fixture 证了：拿 v19 自己的 `t0.b3dm`
  注入盘符 ⇒ `rc=1 LEAK（1 处）`、命中区 `glbJSON_0`；只注入相对 uri 的阴性对照 ⇒ `rc=0`。
  v19 的 4 处裸正则命中全部落在 `glbBIN_1`（GLB 二进制 chunk），按脚本口径是字节噪声、
  不算泄漏；`path_leak_scan.js` 报的 `t688/t840` 同一回事，歧义关闭。
  **这条证据只覆盖 b3dm 侧**：fixture 生成器只吃 b3dm，i3dm 的分区（偏移 28 的
  `gltfFormat`）本轮**没有**重做已知阳性验证；v19 的 126 个 i3dm 是靠"命中区表里
  根本没有 i3dm 的裸命中" + `i3dm_pose_check.js` 独立解析通过来兜的，不等于同一把尺子验过。

**仍未闭合**：v19 的**视觉验收一份都没做**，"官方 validator + 自研电池全绿"≠"发布验收通过"。
判据本身还在重做（未闭合第 7 条）。

KTX2 那批 warning 的成因这轮拿到了**validator 自己的话**，不再靠推断：
叶层 info 里有 **691 条**（v17，= 带图的瓦片数）/ **81 条**（`longxiang_a42`）
`Cannot validate an extension as it is not supported by the validator: 'KHR_texture_basisu'`，
且内嵌 `gltf-validator@2.0.0-dev.3.10` 全文
`grep -c KHR_texture_basisu` = **0**、唯一的 `KTX` 字样就是 `image/ktx2` 这个字符串本身
（即它既不认识扩展、也没有 KTX2 魔数）。产物侧声明是齐的：v17 的 691 个带图瓦片
每个都写了 `extensionsUsed` / `extensionsRequired` = `KHR_texture_basisu` +
`KHR_texture_transform`，1569 张图全为 `image/ktx2`（内嵌 bufferView，无外部 uri）。
所以这两类 warning 是**校验器口径限制**，判据可复核。

warning 全部同源、且不成立为缺陷：validator 自带的 glTF schema 早于
`KHR_texture_basisu`，对 `mimeType: image/ktx2` 报
`Invalid value "image/ktx2"` / `Image format not recognized`。
**计数口径**（2026-10-20 实测，别再按"张数=warning 数"读）：报告头的
`numErrors/numWarnings/numInfos` **与叶层条数没有固定换算**——
`longxiang_a42` 头 `warn=81`，而叶层 WARNING 500 条（250 张 `image/ktx2` × 两类消息），
只覆盖 81 个瓦片；头 `info=40`，而叶层 INFO 405 条、覆盖 **121** 个瓦片。
`lugouqiao_v17` 头 `warn=682`，叶层 WARNING 3138 = 1569 × 2、覆盖 **691** 个瓦片
（头比"带警告的瓦片"少 9，**差因本轮未归因**）。
**因此验收只读两样**：叶层按 `severity` 分类的条数 + 按瓦片归组的集合；
头三个数只当进度提示，不当判据。上一版这里写的"81 条 = 85 张 KTX2 去重后…"是错的，已按实测替换。
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
| **longxiang_final512**（A-48/A-49 后，`-ptw 512`） | 127 | **32**（`t79`，6 张 >24） | 34 675 | 34 675 | 737 115 |
| longxiang_final2048（同二进制 `-ptw 2048`） | 127 | **31**（`t79`，6 张 >24） | 34 675 | 34 675 | 737 115 |
| longxiang_l0b（final + A-47 落盘仪表） | 127 | **32**（`t79`，6 张 >24） | 34 675 | 34 675 | 737 115 |
| cmpt_probe（`mixed.cmpt`） | 1 | 1 | 12 | 3 588 | 3 600 |

三点读法：
1. **三角形两条硬限（去重/实例展开）核对全部通过**，而且是**读回真实产物**得到的，不是工具自报；
   b3dm 的"去重"列与 `conversion-report.json` 的 `maxTrianglesInTile` 逐个相等
   （t01 3 042 / t03 34 296 / t04 14 400 / longxiang 34 675 …），
   说明写出器与统计口径没有分叉。
   **但 primitive ≤24 那一条本轮被打破了**：同一份产物 `longxiang_final512` 判 **rc=1**，
   6 个内节点代理 25~32 个 primitive（`t0`=27/`t1`=26/`t21`=28/`t29`=25/`t79`=32/`t100`=28），
   三角形两列与 covfix 逐值相等 ⇒ **破的是结构预算，不是负载**。旧写法"硬限核对全部通过"
   只对 12 资产矩阵与 covfix 成立，对 2026-10-21 的纹理修复不成立 —— 定级见 AUDIT **A-51**。
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

## 8c. 代理纹理折叠（A-51 方案 A）产物级对照（2026-10-21 下午）

资产 `D:/BaiduNetdiskDownload/longxiang.fbx`（65 764 688 B，123 张贴图），
参数一律 `-sp … -dc 24 -ptw 512`。**三组对照都是同一份源码、只差折叠开关或守卫开关**：

| 目录 | 二进制 | 折叠 | 跨瓦片守卫 | 代理 prims max/mean | `prims>24` | 代理无纹理 | 叶 prims max/mean | 内容字节合计 | 墙钟 / CPU |
|---|---|---|---|---|---|---|---|---|---|
| `longxiang_abnofold/.wb-staging` | 15:53 前 | 关 | 无 | **32 / 12.7** | **6** | 2 | 11 / 3.6 | 87 793 600 | — / — |
| `longxiang_fold512` | 14:55 | 开 | 无 | **24 / 9.8** | **0** | 2 | 11 / 3.6 | 87 565 848 | 120 s / 364 s |
| `longxiang_guard512` | 15:53 | 开 | **有** | **24 / 9.8** | **0** | 2 | 11 / 3.6 | 87 563 536 | 338 s / 896 s |
| `longxiang_nog2` | 16:54 | 开 | **摘掉**（只作计时对照） | 未测 | 未测 | 未测 | 未测 | 未测 | **329 s / 869 s** |

> **最后一行为什么存在**：先前本节把 120→338 s 记成守卫的代价，**那个归因已经撤回**。
> 16:53 备份源码后用 node 脚本把 `FoldScopeGuard` 整块摘掉、重编出 16:54 的二进制，17:00 跑出的
> 墙钟/CPU 与 16:00、16:07 两次带守卫运行同档；且**贴图写出上游**的 `micro_evaluate`
> 也一起翻倍（130.9 → 260.1 s，守卫根本不读 UV），而五次运行的工作量计数器逐项相同
> （`micro_evaluate_samples` 48 963 414、`micro_texture_export_files` 121、`tileCount` 129）。
> 当天同一资产同计数器下的墙钟跨 **119.9 – 436.4 s**（09:14=391.8、10:29=436.4、10:38=122.2、
> 14:55=119.9、16:00=338.4、17:00=329.1）⇒ **表里这两列不是判据**，守卫的真实代价仍是未测，
> 要量得在同一时间窗内交替跑两个二进制。

- **折叠的收益**：代理图元 32→24、`>24` 违例 6→0、字节 −230 064 B（−0.26 %），
  **纹理一条没少**（代理无纹理 2→2、叶瓦片逐值不变）——满足"顶层无贴图不可接受"。
- **守卫为什么保留**：它是**瓦片隔离**的正确性修复，不是性能项。不加守卫时，后面瓦片的贴图
  决策会读到前一个瓦片折叠时清掉的变换键。守卫的计时代价未测（见上），不作为保留/移除的依据。

**"折叠有没有改采样"的判据（`E:/tmp/diag_all.js`，本轮新写）**：把每个代理瓦片、
每张图集页（按 `images[].name` 内容哈希配对）的**最终采样坐标** `offset + scale·uv`
量化到 `1e-5` 做多重集差。`guard512` vs `abnofold`：**450 组 / 505 880 顶点，有差的组 = 0**，
最大计数偏差 0；4 组因页换名无法配对（页尺寸两侧逐值相同）。
> **换页不是折叠造成的**：同一二进制同一参数连跑两遍也对不上 ——
> `fold512` vs `fold512_b` 差 **5 组页 + 3 个没走折叠的叶瓦片**（`t40`/`t78`/`t119`），
> `guard512` vs `guard512_b` 同样差 5 组 + 3 叶。机制是图集装配按
> `unordered_map<osg::Geometry*, …>` 的指针哈希序遍历（`GltfOptimizer.cpp:2004`、`:2026`），
> 记 **A-52**。⇒ **产物逐字节对照这把尺子对本条无效**，只能用量化坐标多重集 + 逐像素截图差。
> 先前那把按排序索引对齐的尺子（`/tmp/fold_all.js`）报的"20 组超容差"是尺子自己的错。

**OGC validator**：`longxiang_guard512` 头 `0 ERROR / 121 WARN / 0 INFO`，
递归 `issues`+`causes` = 1514 WARNING / 837 INFO / **0 ERROR**、ERROR 文件数 0，
与折叠前 `longxiang_final512` 的头三列逐项相同（`validation_lx_guard512/`）。
> **口径更正（2026-10-22）**：1514 里含 **242 个带 `causes` 的结构节点**（121 告警文件 ×2 层），
> 真叶 = **1272 WARNING / 837 INFO / 0 ERROR**。尺子 `E:/tmp/val_leaf.js` 只数无 `causes` 的节点，
> 并用 `lugouqiao_v17` 的已知答案复算对齐（真叶 ERROR 195 / 告警文件 691 / 错误文件 26）。
> `lx_p_a`（A-52 清零后的当前二进制）真叶读数同为 **1272 / 837 / 0** ⇒ 清零未改变校验结果。

**预算闸真的在开火**：默认 `-dc 20` 跑折叠后的 longxiang → **rc=3**，
`selfValidate` 拒 `tiles/t0.b3dm`=24、`tiles/t1.b3dm`=22（>20）。
这条与手册 §3 的 24 之间的差值**未由我裁定**，见 A-51 与 STATUS 的 `-dc` 注记。

**视觉验收（部分完成，别当成全量）**：`scripts/visual_capture.sh` + 无头 Edge/Cesium，
`far-to-near` 4 帧采样，对照 = 同码不折叠的暂存产物；逐像素差（`sharp` raw，1280×720）：

| 机位 | coverage 像素（对照 / 折叠） | 平均绝对差 | 最大差 | >8/255 通道占比 |
|---|---|---|---|---|
| `dist_30.00R`（纯代理 LOD） | 0 / 0 | **0.000** | **0** | 0.000 % |
| `dist_11.42R` | 3 857 / 3 857 | 0.007 | 5 | 0.000 % |
| `dist_3.15R` | 57 430 / 57 430 | 0.096 | 5 | 0.000 % |

未见错位采样该有的大块错纹理；几何占比像素两侧逐值相同。
**三个缺口**：①这三帧 `chromaFraction ≈ 0.0002`（画面近乎灰度），纹理判据灵敏度低；
②折叠侧第 4 帧 `dist_1.20R`（近景）因超时未截到 ⇒ **折叠最相关的近景 LOD 边界没有像素级证据**；
③发布资产 `lugouqiao` 的视觉验收仍未做。

> ### 2026-10-22 复测：近景帧补齐（缺口 ② 闭合），并发现一把尺子缺陷
>
> **匹配对照换了**：上一段表的不折叠侧是**暂存产物**（`longxiang_abnofold`，走的是另一套纹理
> 处置），且与折叠侧不同二进制档。本轮改用**同二进制、同 `-dc 100`、只差折叠入口**的一对：
> `lx_fold_m100` vs `lx_nofold_m100`（`maxMaterialsPerTile` 两侧都置 100，只为让不折叠侧过闸，
> 折叠与否是**唯一变量**）。四次截图落在 **09:06→09:39 同一时间窗**内。
>
> | 机位 | coverage 像素（折叠 / 不折叠） | 平均绝对差 | 最大差 | >8/255 通道占比 | >16/255 |
> |---|---|---|---|---|---|
> | `dist_30.00R` | **0** / 535 | 0.0204 | 109 | 0.0425 % | 0.0381 % |
> | `dist_11.42R` | 3 857 / 3 857 | 0.0016 | **7** | 0.0000 % | 0 |
> | `dist_3.15R` | 57 430 / 57 430 | 0.0231 | **9** | 0.0002 % | 0 |
> | `dist_1.20R`（近景） | 285 856 / 285 856 | 0.0564 | **10** | 0.0033 % | **0** |
>
> 近景两侧 coverage / `projPx`（519.6）/ 可见瓦片清单逐值相同，差异全部落在 ≤10/255 的
> 纹理边缘量化噪声上（`>16/255` 通道 **0 条**）——错位采样该有的成块错纹理没有出现。
> ⇒ **缺口 ② 闭合：折叠在近景 LOD 边界上视觉中性。**
>
> `dist_30.00R` 那一行**不是产物差**：折叠侧那一帧是没加载出来的洞（`settleIters=12`、coverage=0），
> 同窗内发布产物 `lx_p_a` 与不折叠侧都是 535 px ⇒ 该帧判据作废（与上一段表里"0/0 差 0"同理，
> 两个洞相减得到的 0 不构成证据）。
>
> **发布产物 vs 上一轮已验收产物（同窗重截）**：`lx_p_a`（当前二进制、全默认：`-dc 24` 与 `-ptw 512` 都是默认值）对
> `longxiang_guard512` 于 09:39 重截的 `lx_guard512_recheck`，近景 `dist_1.20R`
> 平均 0.0230、**最大 5**、`>8/255` 通道 **0 条** ⇒ A-52 画布清零与其后的改动**未带来视觉变化**。
>
> **尺子缺陷（本轮新踩到，影响所有历史截图读数）**：同一份 `longxiang_guard512` 产物，
> 10-21 16:35 截的 `silLumAvg` = 141.8 / 138.0 / 131.8，10-22 09:39 重截 = 91.3 / 80.2；
> 而两侧 coverage（3 857 / 57 430 / 285 856）、`projPx`（54.6 / 197.8 / 519.6）、
> 3.15R 机位可见瓦片清单（各 51 块，`only in` 两侧均空）**逐项相同**
> ⇒ 差在**渲染曝光**，不在产物。⇒ **逐像素判据只在同一时间窗内有效**，跨窗的绝对亮度
> （`silLumAvg`、`spread`）互不可比。上一段表里"最大差 5"仍然成立（两侧同窗），
> 但它与本轮表不属于同一亮度档，不能拿来做趋势。
>
> **缺口更新**：① 仍然成立（本轮四帧 `chromaFraction` 依旧 0.0002～0.0003，画面近灰度，
> 纹理判据灵敏度低）；② **已闭合**；③ `lugouqiao` 的视觉验收由用户裁定**停做**
> （A-54 走"明确失败"，见 AUDIT 的 A-54 与本节 §8c）。

**回归**：`test_production` **277/277**（`foldUv` 26 + `budgetGate` 9 + A-52 的 `atlasCanvas` 14；
本段先前记的 263 是 `atlasCanvas` 落地前的读数），
`longxiang_guard512` 的报告不变量与折叠前逐项相同（`tileCount` 129、`maxContentBytes`
2 535 648、`errorMaxMeters` 94.515 468 985 376 81、`nodesCertified` 2 / `nodesBoundLoose` 60）。

> **发布产物 `lx_p_a` 的七项自研电池（2026-10-22 逐条取 rc，全部 rc=0）**
>
> | 尺子 | 读数 |
> |---|---|
> | `scripts/glb_prims.js` | 127 个内容文件（121 b3dm + 6 i3dm）违例 + 解析失败 **0**；`max prims/tile = 24`（`t0.b3dm`，正好压在上界）、实例展开三角形最大 34 675（`t60.b3dm`）在 12 万硬限内 |
> | `scripts/i3dm_pose_check.js` | **rc=0**：6 个 i3dm / 10 条实例位姿在**相对帧** 100 % 对上、最近邻残差最大 **7.11e-6 m**、绝对帧比对 **0 命中**（正确签名）、POSITION 全 0 瓦片 0、GLB 节点带 TRS 0、原型 `\|v\|` 最大 126.55 m ⇒ §9 那条"任何带 `.i3dm` 的既有产物一律不可发布"对**当前二进制**的 longxiang 已解除 |
> | `scripts/check_manifest_assertions.js` | `aggProxies 10 / aggBuilt 10 / aggSkipped 0`、`occurrences 571`、断言 **PASS** |
> | `scripts/accessor_extents_scan.js` | 1 662 个 VEC3 访问器，声明≠实算 0、非有限分量 0、`>1e6` 瓦片 b3dm/i3dm 均 0（max\|c\| 144.8 / 126.6 m） |
> | `scripts/zero_normal_triage.js` | **零长度 NORMAL 顶点 = 0**（A-43 在这档资产上产物级为 0，不是"有但没判"） |
> | `scripts/path_leak_structural.js` | 127 瓦片 + 3 旁证 **CLEAN**，裸命中进 JSON 区 0、padding 残留 0、尺子失效 0 |
> | `scripts/tileset_bounds.js` | 父不覆盖子 0、NaN box 0、负半轴 0、非法 geometricError 0、子级误差大于父级 0 |
>
> 官方 validator 的读数在上面同一段落里（真叶 1272 WARN / 837 INFO / **0 ERROR**）
> ⇒ **longxiang 档的验收证据就此齐了**：转换 rc=0 + 七项电池 + validator + 报告不变量 + 4 帧逐像素。

## 9. 验证结论

- **实例位姿（A-41，本轮新增口径）**：代码分两段修好（flatten 掩码 exe 22:40 /
  相对帧 exe 23:53），§1c 的 `matrix_a42fix` t05 用 `scripts/i3dm_pose_check.js` 逐条对上
  `inverse(原型世界)·实例世界` 的 `(x,−z,y)` 置换（**299/299、残差 ≤9.47e-6 m、GLB 无节点 TRS**）；
  **按绝对平移比对必须 0/299**，全命中绝对平移恰恰是"相对帧没生效"的签名——本轮一开始就把
  这条判据写反过，`a41fix_t05`、`matrix_a41fix2` 两批"299/299 命中绝对平移"的 PASS 已作废。
  同一工具对 `matrix_a34/t05`、`lugouqiao_v15` 与上述两批中间产物都判 exit 1。
  **真实 FBX 现已在同一判据上验过**：`lugouqiao_v17`（23:53 exe + 00:16 插件；`v16` 用的是
  "相对帧为空操作"的 22:40 二进制，已主动终止）实测 **126 个 i3dm / 1787 条实例位姿
  相对帧 100 % 对上、残差最大 1.85e-5 m、POSITION 全 0 的瓦片 0 个、绝对帧比对 0 命中、
  原型 `|v|` 最大 383.48 m，rc=0**（`wuhu_v1` 在同一工具上判 rc=1：5/5 个 i3dm POSITION 全 0、
  19 条 0 命中、原型 52 494 m，属 A-41 之前产物）。在此之前**任何带 `.i3dm` 的既有产物
  一律不可发布**（v15、longxiang*、matrix_a34 等）这条判据不变。
- 结构合法性：12 资产 + 真实 FBX + cmpt 探针在**官方** OGC validator 下
  上一批产物 `numErrors=0`（§7），但同日复跑 `matrix_a34` 的 t02 报 3 个
  zero-length-normal ERROR（§1b/§7）——**"0 error"不是不变量**，缺陷按 A-38 记，
  内发布闸 `selfValidate` 对那批产物同样全 0 error（当时还没有 2b）；
  **2026-10-20 这条判据落到 v17 上了**：`matrix_a42fix` 12/12 与 `longxiang_a42` 全 0 error，
  而 `lugouqiao_v17` **26 个内容文件 195 条 zero-length-normal ERROR、rc=1**（§7）——
  这一条不满足，发布验收就不能判过；
- 预算控制：超硬限明确失败、零残留（S2 实测）；真实 FBX 硬限达标两份——
  `longxiang_a42` 127 个内容瓦片最大 2.17 MiB，`lugouqiao_v17` 1057 个内容文件
  最大 3 237 360 B（`maxTrianglesInTile=119 840` 在 12 万硬限内，A-33 展开口径同值）；
  矩阵 12/12 违例 0；实例瓦片的三角形预算见 §6.2 的 A-33；
- 身份映射：manifest 跨瓦片回查单测通过；真实 FBX 最大一份是 `lugouqiao_v17`
  的 **2729 个源 feature / 4250 条出现**（`longxiang_a42` 是 145 / 571，两份都由
  `check_manifest_assertions.js` 判 rc=0）；
  **注意** `longxiang_covfix` 是 A-35 修复前的产物，它的
  `report.manifestOccurrences=623` 与 `manifest.occurrenceCount=571` 那对
  不一致的数字正是 A-35 的现场（差的 52 条是聚合代理条目，本就不该算源出现次数），
  不是可接受的现状；新二进制复跑后两个数必须由 `sourceOccurrenceCount` 同出一个定义
  （`node scripts/check_manifest_assertions.js` 第 1 条断言）；
  `featuresMissingFromLeaves=0` / `proxyDroppedFeatureCount=0`；
  `aggregateProxies` 补全后可由源 feature 反查代理 ID 与其代表的源集合；
- 视觉链路：CesiumJS 加载渲染真实出图，自动化 0 错误；
- **误差口径（2026-10-20 下午重新基线）**：`MeshBVH` 叶子索引 + 覆盖半径两处修复后，
  12 资产矩阵 `mx_bvh` 的 `errorMax` 全线下降（t08 1.607e7→5 993.6 m、t11 5 643→6.39 m）、
  **0 个瓦片自身上界变大**、`upper<lower` 违例 0、内容文件改动 0，
  validator 12/12 `errors=0`（§5b/§7）。代价是一条**待用户裁定**的判据问题：
  `nodesCertified` 在这 12 个小资产上全部变成 0，`CERTIFIED`（1 mm 绝对 / 上界 2 %）
  在诚实距离 + 40 万样本预算下不可达；**但"全部归零"不是普适结论**，
  `lugouqiao_v19` 仍有 63 个 CERTIFIED（450→63）。本轮**没有**为了凑合格证去动
  `toleranceMeters`/`certifyRatio`（那属于放宽验收），也没有加大采样预算。
- **A-51 方案 A（代理折叠）已于 2026-10-21 下午做完产物级验收**（§8c）：代理 prims
  32/12.7→24/9.8、`>24` 违例 6→0、纹理保住、validator 叶层 0 ERROR 与折叠前逐项相同、
  450 组 / 505 880 顶点的最终采样坐标量化多重集 **0 组有差**。
  同轮发现并记入 **A-52**：图集装配顺序随指针哈希漂移，**同二进制同参数两遍跑产物不逐位相同**
  （实测 5 组换页 + 3 个叶瓦片字节差），所以"产物逐字节对照"从今往后只能当阴性对照看。
  折叠的视觉验收**近景帧已于 2026-10-22 补齐**（同二进制匹配的 m100 一对，`dist_1.20R` 最大差 10、
  `>16/255` 通道 0 条，见 §8c 末尾）；发布资产 `lugouqiao` 的视觉验收由用户裁定停做（A-54 走明确失败）。
- 未决：真实 GPU 人眼验收（BLOCKED-VISUAL）、Python 采集器（BLOCKED B-5）、
  **A-43 写出侧处置的产物级复验已于 2026-10-20 下午闭合**（不再是待办）：
  `lugouqiao_v19` 经同一 validator 判 **0 error / `rc=0`**，同轮对 `lugouqiao_v17` 复跑
  仍是 **195 条 zero-length-normal ERROR / `rc=1`** 作阳性对照，明细见 §7 末尾。
  **`lugouqiao_v19` 的视觉验收**（与 v17 一样还没做，validator/电池全绿不等于可发布）、
  **`lugouqiao_v17` 不必再做视觉验收**（它已被 v19 取代，且本身没过发布闸），
  要验的是 **v19** 这一份，而它的判据本身还在重做（未闭合第 7 条）、
  **芜湖水厂用当前 HEAD 复跑**（`wuhu_v1` 是 A-41/A-42 之前产物，已判废）、
  **433 MB 的 `jianzhu+tietu.FBX` 从未出过盘**（5 个目录只有纹理），
  longxiang 内存回退（B-7）。
  ~~写盘后 glTF primitive 数回读（B-9）~~ 已解除：`glb_prims.js` 在 v17 的 1057 个
  内容文件上违例 0（读数见 §6.2 的 v17 电池表）。

---

## 2026-10-28（第二轮）：真实 FBX 的产物级复验（A-60 ~ A-64 之后）

### 1. 产物同一性（这是本轮最关键的一条）

`longxiang.fbx` 用两个二进制各跑两遍（A = 干净 HEAD，B = 候选，A→B→B→A）：

| 项 | 读数 |
|---|---|
| 每份产物的文件数 | 246（`tiles/` 129 + `textures/` 117 + `tileset.json` + `feature-manifest.json`，不含 `conversion-report.json`） |
| 四份清单的 md5 | **全部 `9a4326be42c6864251a41e3690235e9c`**（逐文件 sha256 全等，跨配置、跨轮次） |
| `report_tile_diff.js A1 B1` | **PASS**：瓦片对齐 129/129、缺失 id 0；负载/结构字段差异 **0**；误差字段差异 **0**；`upper<lower` 违例 **0**；上界"变小 0 / 变大 0 / 不变 129" |
| 唯一差异 | `timing` 字段 127 条（耗时自然不同） |

⇒ SAH 换树形、坐标记账、查询计数三处改动**没有**改变瓦片结构、绘制量、几何误差或字节内容。
Naruto 档同样（A/B 各 1 次，4 个文件逐位相同，`outputBytes=190 915`）。

### 2. 官方 validator（`scripts/run_validator.sh`）

- 对象：`F:\wb_3dtiles_out\_followup_20261028\ab\lx_B1`（候选 B），报告落在
  `…\_followup_20261028\validation\lx_B1.validator.json`。
- 表头：`numErrors=0 / numWarnings=121 / numInfos=0`。
- 叶层（**按"冒号后可有空格"的尺子**数，避免 A-57 那轮的死尺子）：
  `"severity": *"ERROR"` = **0 条**；`"severity": *"WARNING"` = **1 514 条**。
- 只验一份的理由：A1/B1/B2/A2 的产物**逐位相同**（上表），validator 的输入相同 ⇒ 输出必然相同；
  这不等于"四个数据集都验过"，而是"验过的那份与另外三份字节全等"。
- **视觉验收没做**：没开 CesiumJS、没有 GPU 渲染、没有截图 ⇒ **BLOCKED-VISUAL**。
  validator 0 error **不等于**视觉通过（沿用前几轮的口径）。

### 3. 坐标有效性（A-60）的真实资产读数

7 个真实 FBX，`modelstats` 的 S1（局部/世界）/S3（SourceModel 烘焙后）四组统计：

| 项 | 读数 |
|---|---|
| `nonFiniteVertices`（世界空间） | 7/7 全 **0** |
| `floatOverflowVertices`（double 有限、float 溢出） | 7/7 全 **0** |
| 索引越界（`indexOutOfRangeTris` / `indexViolations`） | 7/7 全 **0** |
| 退化面（零面积） | jianzhu **28 356**、龙翔桥站厅 909、芜湖水厂 344、longxiang 15、卢沟桥 16、Naruto 0 |
| 大坐标（合法） | 芜湖水厂 `maxAbs=75 465.9`、卢沟桥 4 337.4 —— **保留**，未按阈值丢弃 |

结论按"本轮所测"写：**未检测到非有限/溢出顶点**；这不能外推成"任何输入都不会有"。
出现时的行为由单测 `coordValidity` 钉住：整条 drawable 丢弃 + 5 个 counter + 汇总/明细 warning，
且 warning 明写"nothing was clamped, moved or rebuilt"。

### 4. UTF-8 路径（A-59 遗留 + A-63）

| 场景 | 结果 |
|---|---|
| 中文输入 + 中文输出（`模型_中文.fbx` → `输出_中文目录\结果_甲`） | **成功**：`inputBytes=29 205 344`（与 `stat` 相等）、`tileCount=1`、`errors=[]`、`tileset.json`/`feature-manifest.json`/报告都落在中文目录 |
| 报告里的路径字段 | `inputFile` 原样保留 UTF-8 中文路径，未乱码 |
| **失败清理**（`discardStaging()` + 空纹理目录 `remove()`） | 单测 `cjkCleanup`：在 `%TEMP%\测试_失败清理_中文目录` 下建"暂存/瓦片/t0.b3dm"与空"纹理"目录，跑一次无几何的（必失败）管线后按 CLI 顺序清理，**6 条断言全过**（暂存目录与其内部文件都消失、空纹理目录被删） |
| CLI 端到端"强行失败" | **PENDING**：`-mb 4096` 让分区器把 2 472 面的模型切成上百瓦片（暂存区已落 67 个 b3dm），跑到一半被本轮命令超时中止；**没有**拿到"rc=3 + 零半成品"的端到端读数，不写成通过 |
| Debug 构建 | **BLOCKED-DEBUG**：A-63 删掉了 `#ifndef NDEBUG` 分叉（代码级已一致），但本机没有 Debug 运行库/插件，未编 Debug 二进制 |

### 5. 未做 / 不可做

- **视觉**：BLOCKED-VISUAL。
- **A-54 档（卢沟桥 335 MB）**：判据未动 ⇒ 仍不可发布；本轮**没有**重跑完整转换，
  只有 `modelstats` 只读数据（5 710 488 三角形 / 2 729 feature / 1 787 实例 / 2 747 MB 纹理驻留）。
- **jianzhu+tietu（12.8 M 三角形）、龙翔桥站厅（2.2 GB 图像）**：只做了只读扫描，端到端 **PENDING**。
- **A-53 堆损坏**：见 STATUS，仍 OPEN。

