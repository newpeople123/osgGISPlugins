# STATUS.md — 分阶段执行状态

| 阶段 | 内容 | 状态 | commit |
|---|---|---|---|
| Phase 0 | 审计与基线（BASELINE.md / AUDIT.md / 构建脚本 / 测试资产 / 基线运行） | 完成 | a53d419 |
| Phase 1 | 源几何 IR 与 Feature Registry、BatchId 修复、manifest | 完成 | d18175e |
| Phase 2 | 自适应空间二叉 BVH（对象/图元/三角形级） | 完成 | 2c3d597 |
| Phase 3 | 预算驱动代理生成 + 聚合 fallback | 完成（A-26/A-27 重写 + A-30 geo/tex 分账：feature 抽稀聚合、字节反推配额、不锁边整体挤压） | 6ab22c4, 1fd1fbe, ee6499e |
| Phase 4 | Seam Registry 与 feature 保留 | 完成（策略：跨瓦片对象不裁剪，归属单 tile） | d18175e |
| Phase 5 | GeometryErrorEvaluator（双向 Hausdorff） | 完成 | 2c3d597 |
| Phase 6 | RefinementPolicy 与 tile tree writer | 完成 | dcda2a9 |
| Phase 7 | 纹理 KTX2 / b3dm / i3dm | 完成（S1/S3 场景 + 真实 FBX 85 KTX2/6 实例瓦片实测，2026-10-19 复测）；cmpt 未触发（B-8） | 0d8bf02 |
| Phase 8 | Bounds、validator、端到端测试、原子化输出 | 完成（A-28/A-29 覆盖不变量 + 首个端到端用例；A-31 内节点代理失败降级、A-32 树深硬限入发布闸，单测 82/82 实测） | 245b717 |
| Phase 9 | 性能优化与最终视觉验收 | 页面/截图链路完成；真机 GPU 验收 BLOCKED-VISUAL；真实 FBX 完成 | bfa43a0 |

交付文档：BASELINE.md / AUDIT.md / DESIGN.md / STATUS.md /
VALIDATION_REPORT.md / PERFORMANCE_REPORT.md（均在 `docs/model23dtiles-production/`），
视觉验收：`docs/visual-acceptance/`（index.html + README + runs/）。

## 可用命令

```bash
bash scripts/wb_build.sh model23dtiles     # 构建 Release 工具
bash scripts/wb_build.sh test_production   # 构建单元测试（当前 118 断言）
./Release/test_production.exe              # 跑单元测试
./Release/test_production.exe pipelineCoverage   # 只跑端到端覆盖不变量用例
./Release/test_production.exe proxySkip    # 只跑远景代理降级/失败关闭用例
bash scripts/run_matrix.sh [assetsDir] [outDir]  # 12 资产回归矩阵（bash+node，本机无 python）
node scripts/collect_matrix.js <outDir>    # 按手册 §3 硬限汇总矩阵结果
node scripts/serve_static.js 8080          # 静态服务器（仓库根；带 POST /va-report 落盘截图）
node scripts/serve_static.js 8081 /f/wb_3dtiles_out   # 只读服务 F 盘产物（CORS *）
bash scripts/visual_capture.sh <session> <tileset-url> [paths] [maxFrames] [port]
                                           # 无头批量截图 + 每帧量化判据（替代 visual_capture.py）
bash scripts/visual_sweep.sh [maxFrames] [name...]
                                           # 12 资产矩阵 + 真实 FBX + cmpt 探针一次跑完
node scripts/va_summary.js [sessionFilter] # 汇总 runs/*/summary.json（帧数/空洞/判不了/灰模）
node scripts/va_paths.js [sessionFilter]  # 逐相机路径明细（空洞/判不了/cameraFinite=false）
node scripts/check_manifest_assertions.js <数据集|容器目录> [...]
                                           # report ↔ manifest ↔ tileset 三文件一致性断言
node scripts/check_page_js.js docs/visual-acceptance/index.html
                                           # 改页面后先做内联 script 语法校验
bash scripts/run_validator.sh <tilesetsDir> [reportDir]
                                           # 官方 OGC 3D Tiles validator（B-3）
node scripts/glb_prims.js <datasetDir> [...]
                                           # 读回写盘后的 glTF primitive / 实例展开三角形（B-9）
node scripts/make_cmpt.js <out.cmpt> <tile> [...]  # 打包 b3dm/i3dm 混合容器（B-8 探针）
python scripts/run_matrix.py --out <dir>   # 12 资产回归矩阵（受 B-5 阻塞）
python scripts/run_extra_scenarios.py      # KTX2/超限/i3dm/b3dm 四场景（受 B-5 阻塞）
python scripts/tileset_tool.py validate <tileset.json> --budget 8388608
python scripts/visual_capture.py --tileset <tileset.json> --out <dir>  # 视觉截图（受 B-5 阻塞）
```

> 本机 `python`/`python3` 已退化为 Windows Store 存根（`python -c` 直接退出 49、
> 零输出），上述 Python 脚本本轮无法执行，见 BLOCKED B-5。矩阵改用等价的
> **仓库内** bash+node 采集（`scripts/run_matrix.sh` + `scripts/collect_matrix.js`：
> 逐资产跑 `model23dtiles.exe -sp`，读工具自己写出的 `conversion-report.json`，
> 再按手册 §3 硬限与覆盖计数汇总），结果表见 VALIDATION_REPORT.md §1/§5。

## commit 清单

| commit | 内容 |
|---|---|
| a53d419 | Phase 0：可复现基线与代码审计 |
| d18175e | Phase 1：源几何 IR + 空间划分 + HLOD 代理 + 误差管线 |
| 2c3d597 | Phase 2/5：分区器语义修正 + 单元测试 + TBB 关闭构建 |
| dcda2a9 | 原子化输出与失败清理（14.17）+ `-mb` 开关 |
| 0d8bf02 | 测试矩阵补齐 + 视觉验收页面 + BatchIdVisitor 重跑修复 |
| bfa43a0 | 真实 FBX 端到端三个缺陷修复（字节估算 / sloppy 兜底 / 双预算判据） |
| 6ab22c4 | A-26/A-27：重写预算驱动简化器与聚合代理（Morton 抽稀、缓冲越界修复） |
| 245b717 | A-28/A-29：源身份覆盖不变量 + 聚合 batch 身份不再冒充源 feature |
| 1fd1fbe | 聚合代理按字节硬限反推面数配额 + 不锁边整体挤压（修 t03 回归） |
| ee6499e | A-30：代理字节预算拆 geo/tex 两本账，纹理不再卡远景代理 |
| b267ca1 | A-31：内节点代理失败降级为"仅子树覆盖" + A-32 树深硬限入发布闸 |

## 真实 FBX 端到端（B-1 已解除）

### longxiang.fbx（66 MB / 123 纹理）— 2026-10-19 HEAD `ee6499e` 复测

`F:\wb_3dtiles_out\longxiang_covfix\`（日志 `longxiang_covfix.log`）。
**0 ERROR、已发布**：129 tiles（深度 12）、127 内容瓦片最大 2 226 920 B、
最大 34 675 三角形、145 源 feature / 623 occurrences、
`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0`、
误差 max 168.37 m / p95 100.51 m / mean 15.10 m、certified 58 / boundLoose 41、
30 个远景代理按 A-30 契约建成无纹理版、93.4 s、峰值 RSS 2 401 MiB。
旧基线（bfa43a0，2026-10-16）：69 tiles / 最大 7.91 MiB / 629.9 s / 650 MiB。

> 本轮新增修复：A-30（纹理字节被当硬预算 → 聚合 fallback 永不收敛，
> node 20 估算 32.5 MB ≈ 纯几何 6 倍）。历史三项见 bfa43a0：
> 字节估算低估 4 倍、meshopt 因误差上限过宽不削减、代理只判三角形预算。

### `20240529卢沟桥分洪枢纽.fbx`（335 MB）— A-33 修复后复跑中

`lugouqiao_v11`（37d832c 时代二进制）真实失败：`selfValidate` 抓到 7 个瓦片
12.2 万～50.8 万三角形超 120 000 硬限 → 按 14.17 拒绝发布（日志
`F:\wb_3dtiles_out\lugouqiao_v11.log`）。当时把原因写成"内节点远景代理放不下"
是**错的**：A-31 的内节点降级通路（`ProductionPipeline.cpp:770-785`）本来就会
把超限远景代理删掉退化成"仅子树覆盖"，`lugouqiao_v14`（含 A-31 + geo/tex 分账）
复跑仍然失败，暴露出真正的漏网路径——

**A-33：i3dm 实例瓦片不做三角形预算。** 实例瓦片的块大小只按
`maxInstancesPerTile`（默认 512 个实例）切，`writeContent` 之后的门槛只写了
`wo.bytes <= maxTileContentBytes`（旧 `ProductionPipeline.cpp:967`），
`_tiles[].triangles` 于是可以在超限的情况下被登记；glTF 侧 accessor 是共享的，
所以**字节不超**，而运行时按实例数展开的真实绘制量 =
实例数 × 原型三角形数远超 12 万。i3dm 瓦片 id 排在主树之后（实测
t05_instances：4 个瓦片里 i3dm 是 `i2.i3dm`），正好对上 v14 日志里
954/955/1014/1038/1039/1042/1043 这一段——而远景代理的 WARNING
只到 tile 928，说明这批全是实例瓦片，且没有任何一条构建期告警。

修复（`buildInstanceTree`）：把原型解析/物化提到实例区间循环之外，
按 `ProxyMeshBuilder::countTriangles(protoGeom)` 反推块大小
`chunk = min(maxInstancesPerTile, maxTrianglesPerTile / protoTris)`；
原型单体就超限时显式失败（不静默丢实例）；写盘后的门槛改成与 b3dm 一致的
`overBytes || overTris`，超限时删除暂存文件并报出实测字节/三角形/实例数，
同时补上此前漏记的 `_report.maxTrianglesInTile`。

`lugouqiao_v15` 用修复后的二进制复跑（输出仍写 `lugouqiao_v14/` 目录以复用
内容寻址的 KTX2 纹理缓存，`exportImage` 命中即跳过重编码）。

> 失败目录里残留 `textures/` 不是清理漏洞：缓存路径由
> `tools/model23dtiles/main.cpp:704-711` 只在**空目录**时删除，因为可能是
> 用户共享的缓存；这些 `.ktx2` 按图像哈希命名，重跑直接命中。


## BLOCKED

- B-1 ~~真实 FBX 样本~~ 已解除（`D:\BaiduNetdiskDownload\*.fbx`）
- B-2 `osgdb_osgt.dll` 未部署（已改用 `.osg`）
- B-3 ~~OGC 3D Tiles Validator 需要 npm~~ **已解除**（2026-10-19 实测）：
  `bash scripts/run_validator.sh F:/wb_3dtiles_out/matrix_texless`
  用官方 CesiumGS `3d-tiles-validator@0.6.1`
  逐个校验 12 资产矩阵 + longxiang + cmpt 探针，**13/13 `numErrors=0`**，
  报告落 `docs/model23dtiles-production/validation/`。
  安装踩坑：validator 依赖的 `sharp@0.32` 走 prebuild-install 从 github.com
  拉 libvips，本机 github 不可达 → 用 `--ignore-scripts` 装 validator，
  再从 registry 装 `sharp@0.33.5` 覆盖三处嵌套 `node_modules/*/node_modules/sharp`。
  唯一残留是 validator 自带 glTF schema 早于 `KHR_texture_basisu`，
  对 `image/ktx2` 报 `Invalid value 'image/ktx2'`（longxiang 81 条 W、
  t12 1 条 W）——validator 版本限制，不是产物缺陷，CesiumJS 1.119 实测能解码。
- B-4 ~~CesiumJS 视觉验收页面~~ 已解除：截图链路改为 **node + headless Edge**
  （`scripts/visual_capture.sh` + `scripts/serve_static.js` 的 `POST /va-report`），
  本机无需 python/playwright 即可批量出图与出量化判据，见"视觉验收"一节。
  仍留 **BLOCKED-VISUAL**：主观项（`SEAM_NORMAL` / `LOD_POP` 观感、材质审阅）
  需要真机 GPU + 显示器复核；本机只有 SwiftShader 软件渲染。
- B-5 **Python 解释器不可用**（Windows Store 存根，`rc=49`）。受影响：
  `run_matrix.py` / `run_extra_scenarios.py`（S1–S4 无法重跑，现有 4/4 结果是
  2026-10-16 的历史产物）/ `tileset_tool.py`。
  已用**等价的仓库内 node/bash 链路**顶替并实测：矩阵用 `scripts/run_matrix.sh` +
  `scripts/collect_matrix.js`，截图用 `scripts/visual_capture.sh`。
  恢复命令：`winget install Python.Python.3.12` 后重跑
  `python scripts/run_extra_scenarios.py` 与 `python scripts/visual_capture.py --tileset <t.json> --out <dir>`
- B-6 **t08/t11 远景部件瞬现无法人眼判定**：`errMax` 达根包围盒对角线的
  99 %～100 %（t11 稀疏离群、t08 大坐标）。结构、预算、覆盖全部合法，
  但"远景是否看得见离群部件"只能靠真机 GPU 视觉验收（受 B-4/B-5 限制）
- B-7 **longxiang 峰值 RSS 650 MiB → 2 401 MiB 回退**：耗时同时
  629.9 s → 93.4 s，尚未定位到单一分配点；本机 127 GB 内存下无实际风险，
  但生产环境需按此值做并发预算
- B-8 **cmpt（b3dm+i3dm 混合容器）路径未触发**——本轮**部分闭合**：
  写出侧确实没有 cmpt 通路（`grep -rn cmpt src/` 为空，实例瓦片走
  "同父节点下并列 b3dm / i3dm 兄弟子树"，官方 validator 判合法）。
  为了不让 §17 的"cmpt 能加载"变成空白，新增 `scripts/make_cmpt.js`
  打包探针数据集 `F:\wb_3dtiles_out\cmpt_probe\tiles\mixed.cmpt`
  （b3dm + i3dm 同容器），官方 validator 实测 **E=0 W=0 I=0**。
  坑：内层瓦片是**独立**的 b3dm/i3dm（magic 在 +0、自己的 byteLength 在 +8
  直接当步长），**没有**额外的 4 字节 tileLength 前缀；加上就会被
  `CmptValidator.js` 判 "Invalid inner tile magic / must be aligned to 8 bytes"。
  仍未做：把 cmpt 接进生产写出器（需要 `writeContent` 支持容器聚合，
  当前样本形态不需要），复现命令
  `node scripts/make_cmpt.js out.cmpt a.b3dm b.i3dm && bash scripts/run_validator.sh <含 out.cmpt 的数据集目录>`
- B-9 ~~写盘后 glTF primitive 数无回读核对~~ **已解除**（2026-10-19）：
  新增 `scripts/glb_prims.js`，从 `tileset.json` 逐条 `content.uri` 剥
  b3dm/i3dm/cmpt 外壳、解析 GLB JSON chunk，统计 `mesh.primitives` 与
  accessor 面数，i3dm 另乘 feature table 的 `INSTANCES_LENGTH` 得展开绘制量。
  实测 341 个内容文件 **0 违例**（prims ≤ 24、展开三角形 ≤ 120 000），
  最大 prims/瓦片 = 11（longxiang `t78.b3dm`）。表与口径见
  VALIDATION_REPORT §8。踩坑：b3dm 头 28 B 但 **i3dm 头 32 B**（多一个
  `glTFFormat`），且 GLB 起点还要加 feature/batch 四张表的长度，
  按固定 28 B 偏移解析 `mixed.cmpt` 会偏 4 B 而报 "not a GLB"。
