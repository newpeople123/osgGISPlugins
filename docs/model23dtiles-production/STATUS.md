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
bash scripts/wb_build.sh test_production   # 构建单元测试（当前 130 断言）
./Release/test_production.exe              # 跑单元测试
./Release/test_production.exe bounds       # 只跑包围体发布闸用例（A-36/A-37）
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
node scripts/tileset_bounds.js <数据集|容器目录> [...]
                                           # 回扫 tileset.json：NaN/负半轴/非法 geometricError/父盒覆盖子盒
node scripts/b3dm_extents.js <文件.b3dm|文件.i3dm> [...]
                                           # 读 GLB accessor min/max，核对 tileset 盒与真实写出的几何
node scripts/b3dm_attr_probe.js <数据集目录|文件.b3dm> [--abs N] [--all]
                                           # A-38 取证：逐 primitive 关联 POSITION/NORMAL/索引，
                                           # 报荒谬坐标与零长法线的 @count-k 相对下标与是否被索引引用
node scripts/i3dm_pose_check.js <数据集目录|文件.i3dm> [--top 12]
                                           # A-41 验收：i3dm 特征表 POSITION 是否落盘、逐条对得上
                                           # inverse(原型世界)·实例世界 的 (x,-z,y) 置换（**不是**
                                           # 绝对平移：绝对帧全命中恰恰是双变换）、GLB 节点有无 TRS
node scripts/path_leak_scan.js <数据集目录>
                                           # A-42 验收：分发瓦片与旁证 json 里不得出现本地绝对路径；
                                           # 只认 JSON 字符串字面量形态，避免在 float 数据里误报
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

### `20240529卢沟桥分洪枢纽.fbx`（335 MB）— v15 被 A-41 判废；v16 中途主动终止（用的是"相对帧为空操作"的 22:40 二进制）；v17 用 23:53 exe + 00:16 插件复跑中

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

`lugouqiao_v15` 用修复后的二进制复跑，实测命令行
`Release/model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/20240529卢沟桥分洪枢纽.fbx
-o F:/wb_3dtiles_out/lugouqiao_v15`（15:47 起）。输出目录是**新建的 `lugouqiao_v15/`**，
不是 v14：471 个 `.ktx2` 在 15:47–15:48 一分钟内就位，文件名与 `lugouqiao_v14/textures/`
里的 471 个**完全同名**（图像哈希命名，零差集）——按内容寻址的缓存命中与"等价重编码
足够快"这两种解释从产物上分不开，所以这里只记录可观测事实；此后 3 h 只在瓦片构建里
惰性新增 34 个（16:43 起散布到 19:08）。`.wb-staging/` 建于 16:00，即纹理阶段约 13 分钟
结束、真正开始写盘。

> 失败目录里残留 `textures/` 不是清理漏洞：缓存路径由
> `tools/model23dtiles/main.cpp:704-711` 只在**空目录**时删除，因为可能是
> 用户共享的缓存；这些 `.ktx2` 按图像哈希命名，重跑直接命中。

**跑到一半的实测（19:08，尚未结束）**：`lugouqiao_v15/.wb-staging/tiles/` 已落
551 个 `.b3dm`（此刻约 7 个/分钟），全部是主树瓦片、还没有 i3dm；逐 accessor
扫描 `maxTriangles = 34 847`（t50，硬限 120 000）、`maxBytes = 3 084 040`（t386）、
`|x|>1e6` 的荒谬顶点 **0 命中**（A-38 探针在真实 FBX 上没有发现）。进程累计
CPU 32 699 s / 墙钟 12 060 s ≈ 2.7 核，说明并行窗口在正常工作而不是卡死。
i3dm 瓦片在 A-33 之后才带三角形预算，所以实例段的验收必须等跑完。


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

## 未闭合（写在这里，不靠记忆）

1. **带 2b 闸的二进制已在 12 资产矩阵上实测，真实 FBX 仍未跑**。闸门**第一版是错的**：
   它拿 `_model.bounds` 当参考系，而 `SourceModel.cpp:578-584` 在识别重复实例后把
   `bounds` 重算成"剔除实例几何"的范围 → 任何实例数据集里**正常**的 i3dm 瓦片都会被判越界
   （`matrix_a38gate/t05`：`i2.i3dm` x=[308.96,310.96] vs 源 [35.87,37.87]±1.73 → rc=3 拒发；
   同一输入在 matrix_a34 / covfix / final 三次都正常出盘 4 个瓦片）。参考系已改成"全部
   `_model.objects` aabb 的并集"并重跑 12 资产（`runs/matrix_a41fix`，exe 22:14）：
   **11 个 rc=0**，其中 t08 大坐标 `errMax=1.06e7` 没有被误伤（① 的答案：正常数据集不误伤，
   但**只在对实例内容做了正确处理后**成立）；t05 仍被拒，这次是**真阳性**，拦到的正是
   下面第 5 条的 A-41（`tile 2 ... axis 1 ([2970.56,2972.56] vs source [-0.898,20.986] ±213.14)`）。
   A-41 修好后再跑全矩阵：`runs/matrix_a41fix2`（exe 22:40）那轮 t05 虽 12/12 rc=0，
   但位姿落的是**绝对平移**（相对帧那半当时是空操作），权威轮是
   `F:/wb_3dtiles_out/matrix_a42fix`（exe 00:12 + `osgdb_gltf.dll` 00:16）：
   **12/12 rc=0、`limitViolations=0`、`path_leak_scan` 0/213 命中**，t05 首次以
   "实例瓦片 + 2b 闸门 + **相对帧位姿验收 299/299**"同时通过（`instanceTileCount=1`、
   `instanceCount=299`、`src/occ 300/2`、`miss/drop 0/0`）。
   **闸门至今没有在 A-38 本人的污染样本上实测过**（127 次那 1 次无法复现，没有新鲜坏样本），
   所以 VALIDATION_REPORT §8b 关于 2b 对 A-38 的拦截仍然只是数值推演。
   真实 FBX 侧：65 MB 的 `longxiang.fbx` 已用修正后的二进制跑通（`F:/wb_3dtiles_out/longxiang_a42`，
   129 瓦片 rc=0，位姿 10/10 命中相对帧、路径泄漏 0/127）；335 MB 的
   `20240529卢沟桥分洪枢纽.fbx`（约 6 h）正在以 `lugouqiao_v17` 重跑，
   在此之前"真实 FBX 用带闸 + 修好参考系 + 修好位姿的二进制跑通"仍未闭合。
2. **A-40 产物不逐位一致**（t02 累计 127 份产物：121 份逐字节相同，6 份是 5 个
   互不相同的整棵瓦片树变体——`nodesBoundLoose` 在 0/1/3、`nodesCertified` 在 1/2/3
   跳，峰值 RSS 34.4→47.3 MiB，其中 matrix_a34 那一份就是 A-38 的污染产物）。
   本轮追加 3 配置 × 30 = 90 次，把两条旧解释都打掉：`WB_JOBS=4` 跑出的变体哈希与
   旧"`WB_PROXY_DEBUG` 第 3 跑"的**完全相同**（`fe354714…`），所以变体既不绑调试开关
   也不绑并行度；"`WB_JOBS=1` 全同 → 并行度不是变量"（n=6）已撤回。分类结论
   （loose/certified）本身随变体跳，说明依赖的是遍历/完成次序而非几何。污染那次的
   t1/t2/t3 与 `fe354714…` 变体**逐字节相同**，而基线的 t5 与变体的 t3 是同一个文件
   （sha256 `3e0d68a79eacfa45…`）——变的是**几何→瓦片 id 的分配次序**，不是数值。
   **本轮拿到第二个廉价样本**：`t09_cross_tile_object` 用 22:40 二进制单跑 8 次 →
   `t0.b3dm` 3 个变体（6×`95b36adf74` / 1×`792a9422a0` / 1×`8e8f5da801`），矩阵内那次是
   第 4 个（`maxBytes` 41152、`errMax` 331.62 而非 386.77）；**误差指标本身**也会随变体动，
   且 6/8 次与上一版二进制逐字节相同 → 变体不绑二进制版本，A-41 的改动被排除为原因。
   单轮 0.75 s，后续定位 A-40 请用它而不是 t02。
   复现与逐配置哈希对比见 AUDIT A-40 / `evidence/a38-forensics.txt` §5/§7/§8/§9。
3. **A-38 的写入方仍未定位**。已测到形态（个别内部槽装着旧数据/零值，三个瓦片是
   同一条尾巴的拷贝）。本轮读码把整类"按 size 构造后漏填"的写法排除：
   `BatchIdVisitor`（只用 `assign()`）、`Osg2Gltf.cpp:194`（`std::fill` 全覆盖）、
   `mergeGeometries/appendElements/cloneEmpty`（全 `reserve`+`push_back/insert`）、
   `Simplifier` 的索引 `std::vector::resize`（值初始化为 0）。**也推翻**了上一版留下的
   "裸指针缓存地址回收→悬垂 accessor"猜测：`_buffers/_bufferViews/_accessors` 的键是
   `ref_ptr`（`Osg2Gltf.h:12,20-22`），一次写出内数组都被钉住，地址不会复用。
   现存两条候选（都**未验证**与本例相关，见 AUDIT A-38 隐患①②）：
   ① `getOrCreateBuffer` 只首触快照字节 + `Osg2Gltf.cpp:158` 就地 `normalize()` 改
   共享数组，可能让"元数据"与"字节快照"分家；② `getOrCreateAccessor` 按指针去重，
   同一数组被两个 `DrawArrays` 图元共享时第二个继承第一个的 `byteOffset/count`。
   污染字节全库扫（仓库 + 输出盘 58 133 个文件）只命中那 3 个瓦片，**新增 90 次复跑
   0 次重现**（累计 127 次仅 1 次），没有新鲜污染产物可差分。
   `mergeGeometries` 缺的 `nrm->size()==pos->size()` 守卫（`ProxyMeshBuilder.cpp:361`）
   仍是独立隐患，但本例 `aggregateProxyCount=0`，聚合路径没走，不是这里的解释。
4. **`lugouqiao_v15` 已跑完，验收未做**。15:47 起跑、**21:52:17 结束**（21 621 s ≈ 6 h），
   产出 **1059 个瓦片**（1057 个 tile 文件：931 `.b3dm` + 126 `.i3dm`），
   `errors: []`、`featuresMissingFromLeaves=0`、`proxySkippedCount=0`、
   `proxyDroppedFeatureCount=0`、`maxTrianglesInTile=119840`（在 12 万硬上限内）、
   `maxContentBytes=3238136`、`maxDepth=18`、`aggregateProxyCount=122`、
   `nodesBoundLoose=332`、`nodesCertified=450`、`peakRssBytes=9.84 GB`、454 条 warning。
   中盘与终盘独立扫描（自定义脚本，与 `b3dm_attr_probe.js` 同解析、100 % 覆盖，
   1057 文件 / 2202 primitive）：**A-38 污染字节 0 命中、非有限坐标 0、量级离散 0、
   声明 accessor min/max 与实数据逐 primitive 全等**——即这份产物**没有** A-38 签名；
   但有 **195 条被索引引用的零长度 NORMAL**（分布在 19 个文件，常以连续 3 顶点成组，
   如 `mesh0 v24/v25/v26`），按三角形面积分档：退化三角形 140～159 条、
   真实面积 36～55 条（阈值随尺度在 1e-8～1e-9 之间摆动，判别力弱，见
   `scripts/zero_normal_triage.js`）。官方 validator 会**逐条**报 `is not of unit length: 0`。
   **仍未做**：官方 validator 逐资产跑一遍、A-33 口径的 i3dm 展开三角形核对、
   `aggregateProxies` 复核，再写进 §6.2/PERFORMANCE。
   **已完成并有结论的一项**：A-41 对这份产物的影响评估 = **全废**。`scripts/i3dm_pose_check.js`
   实测 126/126 个 i3dm 瓦片 POSITION 全 0、1787 条实例位姿 0 命中、原型 `|v|` 被累乘到
   1.22e8 m；这份产物作为"可发布数据集"已经不成立，下面两项待办只对**修复后的 v16** 有意义。
5. **A-41（本轮新发现，阻断级）已修，但分两段生效**（flatten 掩码 exe 22:40 / 相对帧 exe 23:53）：
   i3dm 的**每实例位姿全部丢失**，原型几何被同组
   N 个实例矩阵**就地累乘**。`ProductionPipeline.cpp:963-972` 让 N 个 `MatrixTransform`
   共享同一份原型 drawable；`GltfOptimizer.cpp:549-554` 的 `FlattenTransformsVisitor`
   用 `TransformAttributeFunctor` **就地**改这份共享数组，随后 `GltfOptimizer.cpp:563-565`
   把每个 MatrixTransform 复位成单位阵；`ReaderWriterGLTF.cpp:302-345` 只能分解出单位阵。
   实测：v15 **126/126** 个 i3dm 瓦片特征表 POSITION 全 0（1787 个实例位置一个都没落盘）、
   **80/126** 是 IL≥2 的共享数组累乘、6 个瓦片烘焙坐标 >1e4，最坏 `i1018.i3dm` |v|=1.22e8 m
   （而 manifest 里全部 1787 个实例变换的 16 个元素最大只有 378）；root 盒被抬到 1.33e8 m、
   root `geometricError` 抬到 7.65e7 m（注意与报告字段 `tilesetGeometricError=8857.99`
   是两个不同的量，别再混用）。机制已做**定量证伪检验**：t05 的 299 个矩阵纯平移，
   平移之和 (273.09, 2956.73, −1788.77) 与出盘偏移 (273.09, 1788.78, 2956.72) 逐项相等
   （仅 Y-up `(x,−z,y)` 轴序差）。
   **修法（两段）**两处：实例瓦片把 `FLATTEN_TRANSFORMS` / `MERGE_TRANSFORMS` /
   `FLATTEN_STATIC_TRANSFORMS(_DUPLICATING_SHARED_SUBGRAPHS)` 四个位从优化选项里屏蔽掉
   （与遗留 `I3DMTile.cpp:5-12` 同一处置，exe 22:40 生效），并把矩阵改成**相对原型**的
   `invProto * insts[k]->matrix`（原型世界矩阵已烘进几何，`SourceModel.cpp:356`），
   原型世界矩阵不可逆时显式失败而不是静默错位。**这第二处 22:40 版是空操作**：写的是
   `pw.invert(invProto)`，而本仓 OSG 的 `bool Matrixd::invert(const Matrixd& rhs)` 语义是
   **`this = inverse(rhs)`**，方向写反后 `invProto` 停在单位阵上，位姿静默变成绝对世界平移
   且不报错；改成 `invProto.invert(pw)` 才真正生效（exe 23:53）。全仓该调用点只有这一例
   （`ObbVisitor.cpp:290` 是注释、`main.cpp:321` 用静态 `Matrixd::inverse()`，方向正确）。
   **验收（新增 `scripts/i3dm_pose_check.js`，口径已纠正一次）**：判据是"逐条对上
   `inverse(原型世界)·实例世界` 的 Y-up 置换"，**不是**对上绝对平移。权威轮
   `F:/wb_3dtiles_out/matrix_a42fix` 的 t05：**299/299 = 100 %** 命中相对帧、最近邻残差最大
   **9.47e-6 m**（float32 在 \|v\|≈150 m 的舍入量级），按绝对平移比对 **0/299**（命中数必须为 0，
   命中即双变换），POSITION 非零 299/299、GLB 节点带 TRS = **0**、原型 `|v|` 从污染态 2972.56 m
   回到 **89.56 m**；`test_production` **150/150**；12 资产矩阵 12/12 rc=0、`limitViolations=0`。
   同一脚本对**修复前**产物必须判 FAIL——已在 `matrix_a34/t05`（0/299 命中、POSITION 全 0、
   原型 `|v|` 最大 2972.56 m）和 `lugouqiao_v15`（126/126 POSITION 全 0、1787 条 0 命中、
   原型 `|v|` 最大 **1.22e8 m**）上验证过，探测器不是空跑。
   **我自己写错过一次并已纠正**：22:41 那轮把"299 条 POSITION 逐项等于**绝对**平移"当成 PASS
   证据（`F:/wb_3dtiles_out/a41fix_t05`、`runs/matrix_a41fix2`），那次确实是全 0 位姿被修好了，
   但落的是错的帧——绝对帧 100 % 命中恰恰是相对帧 0 % 生效的签名。这两批产物已作废。
   **待办**：真实 FBX 用**修正后的 23:53 exe + 00:16 插件**重跑为 `lugouqiao_v17`（约 6 h）；
   `lugouqiao_v16`（22:53 起，用的是"第 2 点为空操作"的 22:40 二进制）已**主动终止**，
   终止时只有 `.wb-staging/` 里 8 个半成品 b3dm，没有 `tiles/`、没有 `tileset.json`。
   **已发布/已归档的 i3dm 资产一律作废**（`lugouqiao_v15`、`longxiang*`、`matrix_a34` 等所有带
   `.i3dm` 的产物），VALIDATION_REPORT S3 的 PASS 口径已按此改写。
6. **`cmake --build build --config Release` 在 HEAD 不是绿的**：`test_3dtiles` 目标有
   **98 个既有** `std::filesystem` 编译错误（C2039/C3083/C2660，与本轮改动无关，
   在 `F:/wb_3dtiles_out/build_a41fix.log` 里改动之前就已存在）。生产构建与单测请**指定目标**：
   `cmake --build build --config Release --target model23dtiles test_production -j 8`（exit 0）。
   `test_3dtiles` 的修复本身是一条独立的未闭合项。
   **同一类坑（本轮踩到，必须记）**：`osgdb_gltf.dll` 是 `add_library(... MODULE)`
   （`src/osgdb_gltf/CMakeLists.txt:41`），**不在 `model23dtiles` 的依赖图里**，按目标构建
   不会重建它。A-42 改 `Osg2Gltf.cpp` 后只构建那两个目标，`Release/osgdb_gltf.dll` 仍停在
   22:14，重跑 t12 观察到"泄漏没变"，差点把修好判成没修好。**改到插件源码后必须
   `cmake --build build --config Release`（全量）或显式加 `--target osgdb_gltf`，
   并核对 DLL 时间戳再复验**。同理的四个插件目标：`osgdb_gltf` / `osgdb_fbx` /
   `osgdb_ktx` / `osgdb_webp`（`src/osgdb_*/CMakeLists.txt`，均为 `MODULE`）。
7. **视觉验收文档与产物不符，本轮已就地更正（§4/§1c 之外新发现的第二条）**：
   VALIDATION_REPORT §4 与 `docs/visual-acceptance/README.md` 都写过
   "修复前 `ax_t05_instances`/`ax_t08_big_coords` 各 12 帧 `cameraFinite=false` 12、空洞 12；
   修复后同样 12 帧空洞 0（中位剪影 65 674–819 868 px）"。用 `va_summary.js ax_` 复核：
   `ax_t05_instances` = 12 帧 / 空洞 **6** / tooSmall **6** / camBad **0**，
   `ax_t08_big_coords` = 12 帧 / 空洞 12 / camBad **0**，且 12 帧剪影全 0——
   现存 `ax_*` **全是修复前批次**，"修复后 0 空洞"与 65 674–819 868 px 实际出自
   `fa_t01_plane_curved`（只有 t01）。两个后果：① t05/t08 的修复后六轴验收**从未做过**；
   ② `cameraFinite` 判据字段在 `camera.lon|height = null` 时仍报 camBad=0，是**漏报**，
   必须先修采集页再谈重跑。加上 A-41 之后所有 i3dm 截图都是错版，视觉验收整批重做。
8. **A-42（本轮新发现，中）已修**：发布产物里泄漏**本地绝对路径**，且瓦片字节数随输出目录
   长短变化。两个口子都是"路径当名字"：`Osg2Gltf.cpp:763/766` 把纹理的完整输出路径写进 GLB
   `bufferView.name`/`image.name`（随 `.b3dm/.i3dm` 分发出去——lugouqiao_v15 实测
   **691/1057** 个瓦片带 `F:/wb_3dtiles_out/lugouqiao_v15\textures\…`）；FBX 导入器用**源文件
   绝对路径**命名根节点，顺着 `SourceModel.cpp:201/211` 的 `nodePath` 进
   `feature-manifest.json`（同数据集 **2729** 处 `D:/BaiduNetdiskDownload/…fbx/…`）。
   第二条伤害更隐蔽：name 进 GLB JSON chunk → 同一输入、同一代码，t12 的 `t0.b3dm` 在不同
   输出目录下出现过 **510432/510440/510344/510304/510296** 五个尺寸，**以前这类差异被当成
   A-40 的噪声**（A-40 的样本里因此掺了假样本）。修法：两处都只留文件名。
   **验收**：新工具 `scripts/path_leak_scan.js` 只认 JSON 字符串字面量形态的盘符
   （第一版用裸 `[A-Za-z]:[/\\]` 扫二进制，把 t03 的 7 个瓦片**误报**成泄漏，已收紧——
   那种命中不能当证据）；修复后 `matrix_a42fix` **0/213 瓦片、0/36 旁证 json**，
   而修复前的 `lugouqiao_v15`/`matrix_a41fix2` 仍报 691/1 泄漏，探测器既不空跑也不恒阳；
   **路径无关性**用两个长度差 40 字符的目录验证，`t0.b3dm` sha256 前缀
   `115eef9af8c80ef63e7c2cf8` 相同；`test_production` 新增用例 `noLocalPaths`
   （含 3 条探测器自检），共 **150/150**。
   **边界**：`noLocalPaths` 的 fixture 不带纹理，只钉住 `nodePath` 那一路；纹理 `name`
   那一路靠矩阵里的 t12 + `path_leak_scan.js`，**没有单测保护**。


