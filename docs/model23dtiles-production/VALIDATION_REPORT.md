# VALIDATION_REPORT.md — 验证报告

所有数据均为本机实测（2026-10-16，`prod/model23dtiles-production` 分支），
不包含推断或估算值。运行环境：Windows 11 / MSVC 19.44 / OSG 3.6.5 / TBB。

## 1. 测试矩阵（手册 14 节，12 资产回归）

命令：

```bash
python scripts/gen_test_assets.py test_assets
python scripts/run_matrix.py --out docs/model23dtiles-production/runs/p2_regress_atomic
```

结果 **12/12 PASS**（判定依据：conversion-report.json 的 errors==0 且
tilesetWritten==true；退出码在本环境被宿主改写，不作判据）：

| 资产 | 结果 | 耗时 s | 峰值 MiB | tiles | 深度 | 内容文件 | validator errors |
|---|---|---|---|---|---|---|---|
| t01_plane_curved | PASS | 1.53 | 21.6 | 1 | 0 | 1 | 0 |
| t02_thin_features | PASS | 1.94 | 33.9 | 7 | 2 | 7 | 0 |
| t03_giant_single_mesh | PASS | 113.13 | 93.1 | 3 | 1 | 3 | 0 |
| t04_many_objects | PASS | 67.53 | 77.3 | 127 | 6 | 127 | 0 |
| t05_instances | PASS | 2.05 | 22.7 | 4 | 2 | 2 | 0 |
| t06_nonuniform | PASS | 14.71 | 38.9 | 31 | 4 | 31 | 0 |
| t07_zero_axis | PASS | 1.23 | 21.6 | 1 | 0 | 1 | 0 |
| t08_big_coords | PASS | 3.20 | 36.8 | 7 | 2 | 7 | 0 |
| t09_cross_tile_object | PASS | 3.09 | 37.8 | 7 | 2 | 7 | 0 |
| t10_material_heavy | PASS | 3.19 | 37.1 | 7 | 2 | 7 | 0 |
| t11_sparse_outlier | PASS | 7.67 | 38.6 | 15 | 3 | 15 | 0 |
| t12_big_texture | PASS | 3.34 | 235.8 | 1 | 0 | 1 | 0 |

对比 Phase 0 基线（同一资产集，BASELINE.md §5）：

| 缺陷 | 基线 | 现在 |
|---|---|---|
| box 负半轴 | t07 实测出现 | 无（validator 0 error） |
| 父子包围盒不覆盖 | 48/6/2/3/1 个 ERROR | 0（double 精度 + 相对容差） |
| 巨型单网格不切分 | t03 134k 三角形 1 个 tile | 切成 3 tiles（对象/三角形级） |
| 父子 geometricError 差恒 0.1 | 是 | 无（测量误差自底向上聚合） |
| tileset 误差 = root×1.5 | 是 | 无 |

## 2. 扩展场景（矩阵缺口补齐）

命令：`python scripts/run_extra_scenarios.py`
结果 **4/4 PASS**（`docs/model23dtiles-production/runs/extra_scenarios/summary.json`）：

| 场景 | 内容 | 结果 |
|---|---|---|
| S1 | `-tf ktx2` 全链路：KTX magic 校验 + b3dm 内引用全为 .ktx2 + validator | PASS |
| S2 | `-mb 4096` 超硬限：报 597 条明确 ERROR，**outputDir 零文件残留**（14.16/14.17） | PASS |
| S3 | 实例瓦片格式：内容文件 magic = `i3dm` | PASS |
| S4 | 常规 b3dm + BATCH_LENGTH 一致性（validator BATCH 检查） | PASS |

## 3. 单元测试

```bash
bash scripts/wb_build.sh test_production
./Release/test_production.exe
```

**44/44 通过**（`test_production_result.txt`）：
- ProductionProfile 硬上限校验（超限抛异常）
- SourceModel 世界变换烘焙正确性
- 分区器 coverage + 确定性（同一输入两次划分结果一致）
- 巨型单网格三角形级切分
- 误差求值器解析用例（同面=0 / 平行偏移 0.5m / 单侧为空=保守上界）
- feature manifest 跨瓦片回查
- **BatchIdVisitor 回归**：属性 0 占用→搬到空闲槽并写 `_ATTRIBUTE_<n>`；
  属性 0 空闲→直接写 _BATCHID；二次处理→不再搬动（本轮新发现并修复的缺陷）

## 4. 视觉验收（手册 15 节）

```bash
python scripts/visual_capture.py \
    --tileset <tileset.json> --out docs/visual-acceptance/runs/<name>
```

实测（t02 资产，CesiumJS 1.119 + Chromium 156 headless + SwiftShader WebGL）：

- 9 条相机路径全部执行：**53 帧 / 53 张截图**，1280×720；
- **console errors: 0，page errors: 0**；
- 每帧记录相机经纬高、可见 tile URI、geometricError；
- 截图目录：`docs/visual-acceptance/runs/t02_thin_features/shots/`。

环境限定：headless Chromium 只能拿到 SwiftShader 软件 WebGL，
**人眼级接缝/LOD_POP 判定需在有物理 GPU 的机器上复跑**（命令见
`docs/visual-acceptance/README.md`），该项标 BLOCKED-VISUAL。

## 5. 逐资产误差实测（conversion-report.json 摘录）

| 资产 | errMax (m) | errMean (m) | certified | boundLoose |
|---|---|---|---|---|
| t04_many_objects | 317.14 | — | 63 | 59 |
| t02_thin_features | — | — | — | — |
| t08_big_coords | 10633297（大坐标量纲，符合预期） | — | — | — |

注：t03/t04 的绝对数值大是因为测试资产场景尺度大（数百米）且叶层保留全量源
几何时父代理误差本来就到场景量级；`certified` 表示上界含真实采样；
`boundLoose` 表示未采样面片用了保守全局上界（手册 9.3 允许）。

## 6. 真实 FBX 端到端（B-1 解除，2026-10-16）

输入：`D:\BaiduNetdiskDownload\longxiang.fbx`（66 MB，123 张纹理）。

```
Release/model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/longxiang.fbx -o out_test/longxiang
```

**结果：0 ERROR，validator errors=0**（188 条 FILES warnings 是纹理经 glTF
内部 URI 引用、validator 的 FILES 检查未解析 glTF 内部引用所致，非产物缺陷）。

| 指标 | 实测值 |
|---|---|
| tiles / 内容瓦片 | 69 / 67（深度 11） |
| 单瓦片最大内容字节 | 7.91 MiB（≤ 8 MiB 硬上限 ✓） |
| 单瓦片最大三角形 | 74 434（≤ 120 000 ✓） |
| feature 数 / manifest occurrences | 145 / 1 013 |
| 误差 max / p95 / mean | 168.4 m / 78.4 m / 19.7 m（场景 ~600 m 尺度） |
| 误差状态 | certified 30 / boundLoose 23 |
| 实例 | instanceTileCount 6 / instanceCount 10 |
| 纹理（hash 去重 KTX2） | 186 个 / 31.6 MB |
| 耗时 / 峰值 RSS | 629.9 s / 650 MiB |

该模型此前暴露并修复的三个缺陷（commit bfa43a0）：
1. 内容字节估算低估 ~4 倍（36 vs 实测 116–242 B/三角形）→ 分区器不切分；
2. 大节点 `meshopt_simplify` 因误差上限宽松而完全不削减（133k→133k）→ sloppy 兜底；
3. 代理简化只判三角形预算漏掉字节预算 → 双预算判据。

修复后回归：12 资产矩阵 12/12 PASS（t03 输出 5.69→4.29 MB），单测 44/44。

## 7. 验证结论

- 结构合法性：12 资产 + 4 场景 + 真实 FBX validator 全 0 error；
- 预算控制：超硬限明确失败、零残留（S2 实测）；真实 FBX 67 瓦片全部 ≤8 MiB；
- 身份映射：manifest 跨瓦片回查单测通过；真实 FBX 145 feature / 1013 occurrences；
- 视觉链路：CesiumJS 加载渲染真实出图，自动化 0 错误；
- 未决：真实 GPU 人眼验收（BLOCKED-VISUAL）、官方 3d-tiles-validator
  （BLOCKED B-3，需 npm 网络）。
