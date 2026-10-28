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
bash scripts/wb_build.sh test_production   # 构建单元测试（当前 277 断言，2026-10-21 晚复测）
                                           # ⚠ 不带参数的 wb_build.sh 只构建 model23dtiles（脚本第 41 行
                                           # TARGET="${1:-model23dtiles}"），test_production.exe 不会被重链：
                                           # 改了 src/utils/*.cpp 却只跑无参数构建，测的还是旧二进制
./Release/test_production.exe              # 跑单元测试
./Release/test_production.exe bounds       # 只跑包围体发布闸用例（A-36/A-37）
./Release/test_production.exe pipelineCoverage   # 只跑端到端覆盖不变量用例
./Release/test_production.exe proxySkip    # 只跑远景代理降级/失败关闭用例
./Release/test_production.exe texResize    # 只跑"共享 osg::Image 不许就地改"用例（A-49）
./Release/test_production.exe proxyTexture # 同时命中 proxyTexture / proxyTextureLadder（A-48）
./Release/test_production.exe bvhOrder     # 只跑 BVH 遍历顺序逐位相同差分（PERFORMANCE L1）
./Release/test_production.exe foldUv       # 只跑图集子矩形折叠/还原等价用例（A-51 方案 A）
./Release/test_production.exe budgetGate   # 只跑 glTF primitive/材质预算闸的正反对照（A-51）
./Release/test_production.exe atlasCanvas  # 只跑"图集画布缝隙必须为 0"用例（A-52）
                                           # 负对照（把 TexturePacker.cpp 的 memset 去掉重编）
                                           # 实测 3/3 次 FAIL：dirty=600/3600 → 这条用例不空过
./Release/model23dtiles.exe -sp -i <模型.fbx> -o <输出目录> [-ptw 512]
                                           # -ptw：内节点（远景 LOD）代理的纹理边长上限，默认 512；
                                           #   叶瓦片仍走 -tw/-th（默认 2048）。低于下限 64 由
                                           #   profile.validate() 直接报错，不静默钳制
                                           # -dc（= maxPrimitivesPerTile）默认 **24**：与 profile、
                                           #   手册 §3 的 24 对齐（tools/model23dtiles/main.cpp:500，
                                           #   用户 2026-10-21 裁定）。闸门仍在：显式 -dc 23 → rc=3
                                           #   且产物不提交。副作用：不带 -sp 的 HLOD 路径也继承该
                                           #   上界（main.cpp:761 → TreeBuilder.cpp:580，20→24）
bash scripts/run_matrix.sh [assetsDir] [outDir]  # 12 资产回归矩阵（bash+node，本机无 python）
node scripts/collect_matrix.js <outDir>    # 按手册 §3 硬限汇总矩阵结果
node scripts/serve_static.js 8080          # 静态服务器（仓库根；带 POST /va-report 落盘截图）
node scripts/serve_static.js 8081 /f/wb_3dtiles_out   # 只读服务 F 盘产物（CORS *）
bash scripts/visual_capture.sh <session> <tileset-url> [paths] [maxFrames] [port]
                                           # 无头批量截图 + 每帧量化判据（替代 visual_capture.py）
bash scripts/visual_sweep.sh [maxFrames] [name...]
                                           # 12 资产矩阵 + 真实 FBX + cmpt 探针一次跑完
node scripts/va_summary.js [sessionFilter] # 汇总 runs/*/summary.json（帧数/空洞/判不了/灰模）
node scripts/va_paths.js [sessionFilter]  # 逐相机路径明细（空洞/判不了/camBad/camNaN/noFlag）
node scripts/check_manifest_assertions.js <数据集|容器目录> [...]
                                           # report ↔ manifest ↔ tileset 三文件一致性断言
node scripts/tileset_bounds.js <数据集|容器目录> [...]
                                           # 回扫 tileset.json：NaN/负半轴/非法 geometricError/
                                           # 父盒覆盖子盒/子级误差大于父级（ge 单调性）
node scripts/b3dm_extents.js <文件.b3dm|文件.i3dm> [...]
                                           # 读 GLB accessor min/max，核对 tileset 盒与真实写出的几何
node scripts/b3dm_attr_probe.js <数据集目录|文件.b3dm> [--abs N] [--all]
                                           # A-38 取证：逐 primitive 关联 POSITION/NORMAL/索引，
                                           # 报荒谬坐标与零长法线的 @count-k 相对下标与是否被索引引用
node scripts/fbx_source_normals_probe.js <模型.fbx> [--detail N]
                                           # A-43 源侧定性：**裸读源 FBX**（按属性名找 Normals、
                                           # encoding==1 时先 inflate）数零长度法线，再按同 mesh 的
                                           # PolygonVertexIndex 真实负下标边界归面。回答的是
                                           # 上面几条都回答不了的那个岔口："源里就有，还是我们写出
                                           # 时变 0 的"——不需要动构建。三重自检（解压后长度精确
                                           # 等于 arrayLength×elemSize / 非零向量里的"非单位"数 /
                                           # 角点数==PVI 元素数）任一不过就别信统计数；含内容摘要
                                           # 去重（instance 拷贝会把 7 个 mesh 重复计成 36 个数组）
node scripts/accessor_extents_scan.js <数据集|容器目录|tiles 目录> [...] [--abs 1e6]
                                           # 全量逐 accessor 三条判据：声明 min/max vs 实算、
                                           # POSITION 非有限分量、荒谬顶点 |coord|>阈值；
                                           # b3dm 与 i3dm 分开计数（A-38 只可能在 b3dm，
                                           # A-41 的烘焙错位只在 i3dm）。
                                           # 与 b3dm_attr_probe 的分工：那个是单文件取证，
                                           # 这个是可以进验收流程的批量判据（有退出码）
node scripts/zero_normal_triage.js <数据集目录|文件> [...] [--rel 1e-6] [--eps 1e-9]
                                           # A-43 定性：零长法线按三角形面积分"退化/真隐患"。
                                           # 主判据是面积 > REL×该 primitive 中位面积（尺度无关），
                                           # 绝对阈值 --eps 只作对照列，两档一起打印
node scripts/make_zero_norm_fixture.js <out.b3dm>
                                           # A-43 阳性对照：造一个已知答案的最小 b3dm
                                           # （真隐患 3 / 退化 2 / 合计 5），用来证明上面那把尺子
                                           # 不是恒返回 0；配 --rel 1e12、--eps 1e12 验单调
node scripts/zero_normal_tile_class.js <数据集目录>
                                           # A-43 取证：含零法线的瓦片对上 report 的
                                           # isProxy/isLeaf/aggregate/errorStatus——面积分档
                                           # 回答不了"是谁产生的"，这一条才能
node scripts/zero_normal_repair_plan.js <数据集目录|文件> [...] [--rel 1e-6]
                                           # A-43 写出侧修复的分档尺子：每个零长度 NORMAL 顶点
                                           # 能不能从自身入射的非退化面拿到法线（A），拿不到时
                                           # 同图元有没有有效面兜底（B），整图元都退化（C）。
                                           # C 档决定"修复要不要编造方向"——v17 实测 A=55 B=140
                                           # C=0，所以 `49b6d3c` 的规则不需要第四档
node scripts/fbx_source_normals_probe.js <模型.fbx> [--detail N]
                                           # A-43 源侧定性：**裸读源 FBX**（按属性名找 Normals、
                                           # encoding==1 时先 inflate）数零长度法线，再按同 mesh 的
                                           # PolygonVertexIndex 真实负下标边界归面。回答的是
                                           # 上面三条都回答不了的那个岔口："源里就有，还是我们写出
                                           # 时变 0 的"——不需要动构建。三重自检（解压长度精确相等 /
                                           # 非零向量里的非单位数 / 角点数==PVI 元素数）任一不过
                                           # 就别信统计数；含内容摘要去重（instance 拷贝会重复计数）
node scripts/i3dm_pose_check.js <数据集目录|文件.i3dm> [--top 12]
                                           # A-41 验收：i3dm 特征表 POSITION 是否落盘、逐条对得上
                                           # inverse(原型世界)·实例世界 的 (x,-z,y) 置换（**不是**
                                           # 绝对平移：绝对帧全命中恰恰是双变换）、GLB 节点有无 TRS
node scripts/path_leak_scan.js <数据集目录>
                                           # A-42 粗筛：裸正则找"盘符+分隔符"的引号串。
                                           # **只用于"该人工看了"**：二进制 payload 里的字节
                                           # 噪声与真泄漏在它眼里同号（v17 的 4 处就是噪声），
                                           # 且只认盘符，漏 UNC / file:// / 前导斜杠。
node scripts/path_leak_structural.js <数据集目录|瓦片文件> [...]
                                           # A-42 验收口径：按 b3dm/i3dm/pnts/cmpt 头切出
                                           # feature table / batch table / GLB 各 chunk，逐个
                                           # JSON.parse 后走字符串，只有 JSON 区里的绝对路径
                                           # 才算泄漏（盘符、前导 / 或 \、file:// 三种形态）。
                                           # 退出码 0=CLEAN / 1=LEAK / **3=尺子失效**——取到 3
                                           # 时那个 0 不算数。PLS_SHOW=1 打印噪声命中的偏移与
                                           # 上下文 hex；PLS_REGIONS=1 打印区表（用来确认 cmpt
                                           # 递归真的进去了）。i3dm 偏移 28 有 4 字节 gltfFormat、
                                           # 本仓 cmpt 偏移 12 实测是内层块数不是字节总长，
                                           # 这两处写错都会造成静默漏检。
node scripts/make_path_leak_fixture.js <源.b3dm> <输出前缀>
                                           # 上面那条判据的对照生成器：同一底本产出"注盘符"与
                                           # "只注相对 uri"两份文件，期望退出码 1 / 0。
                                           # 没有它，"0 处泄漏"可能只是判据恒返回 false。
node scripts/report_tile_diff.js <基线report.json> <候选report.json> [--allow-error-diff]
                                           # 提速改动的"质量中性"闸门：两份 conversion-report
                                           # 的 tiles 按 id 对齐逐字段比。负载/结构字段
                                           # （triangles/contentBytes/uri/refine/hasContent/
                                           # isLeaf/isProxy/aggregate/aggregateSources/
                                           # sourceFeatures）差异必须 0；误差字段在纯性能改动下
                                           # 也必须 0，只有正当的正确性修复才加
                                           # --allow-error-diff（方向仍要人工论证）。
                                           # 同时打印上界变小/变大/不变与 upper<lower 违例数。
                                           # 退出码 0=PASS / 1=违例 / 2=用法 / 3=对齐失败。
                                           # 对照（2026-10-20 实测）：v19 vs 自身 0；
                                           # v17→v19 不带 flag 1、带上 0；v15→v17 判
                                           # contentBytes=693 违例退出 1。
node scripts/check_page_js.js docs/visual-acceptance/index.html
                                           # 改页面后先做内联 script 语法校验
node scripts/table_pipe_check.js <md 文件...>
                                           # 文档表格体检：每行的列数必须等于分隔行，
                                           # 否则 GFM 会**静默断表**（`\|` 不计入列数）。
                                           # 阳性对照：2 列分隔行 + 3 列数据行 → 报违例、rc=1；
                                           # 全 5 份生产文档实测一致、rc=0
node scripts/scan_image_mime.js <数据集目录> [maxTiles]
                                           # 剥 b3dm/i3dm 外壳读 GLB JSON chunk，统计 images[].mimeType
                                           # 与 extensionsUsed/Required。官方 validator 那批 image/ktx2
                                           # warning 判"是校验器不认识 KHR_texture_basisu 还是我们没声明"
                                           # 就看这一条；无退出码，是取证工具
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
最大 34 675 三角形、145 源 feature / 623 occurrences（这串数字出自 `ee6499e`，
早于 A-35 的 `262dc77`：623 是"所有 batch 条目"的旧口径，单一口径应为 571，
见 AUDIT A-35 与 `evidence/manifest_occurrences-a35-boundary.txt`）、
`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0`、
误差 max 168.37 m / p95 100.51 m / mean 15.10 m、certified 58 / boundLoose 41、
30 个远景代理按 A-30 契约建成无纹理版、93.4 s、峰值 RSS 2 401 MiB。
旧基线（bfa43a0，2026-10-16）：69 tiles / 最大 7.91 MiB / 629.9 s / 650 MiB。

> 本轮新增修复：A-30（纹理字节被当硬预算 → 聚合 fallback 永不收敛，
> node 20 估算 32.5 MB ≈ 纯几何 6 倍）。历史三项见 bfa43a0：
> 字节估算低估 4 倍、meshopt 因误差上限过宽不削减、代理只判三角形预算。

**2026-10-21 复测（`longxiang_final512`，未提交二进制 + `-ptw 512`；上面那段 covfix 读数里
"30 个远景代理建成无纹理版"已被 A-48/A-49 取代）**：
129 tiles（深度 12）、127 内容瓦片、`maxContentBytes` 2 539 456、最大 34 675 三角形、
145 源 feature / 571 occurrences（单一口径，A-35 之后）、
`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0`、`proxySkippedCount=0`、
`aggregateProxyCount=10`、`proxyTextureCappedCount=60`、
**内节点代理 58/60 有纹理**（只有 `t73`/`t74` 无纹理，源本就无图；covfix 那轮是 18/60）、
顶层 `t0.b3dm` 903 816 B / 15 张图（covfix：357 944 B / 0 张图）、
峰值 RSS 3 637 MiB、`totalSeconds` 119.16 s。
误差 max **94.52** / p95 **21.75** / mean **3.83** m、certified 2 / boundLoose 60
——**这组与 covfix 那组不可比大小**：A-44/A-45 改的是上界口径（旧数偏高），
逐字段对照见 AUDIT A-45/A-46 与本轮 A-49 的产物级表。
同参数的第二跑 `longxiang_l0b`（只多 A-47 的落盘仪表）**上面每一串数字逐值相同**，
只有 8 个瓦片的 `contentBytes` 与耗时/RSS 随噪声动（A-40，见下）。
> **但这组读数是"部分达标"**：同一份 `longxiang_final512` 的 `glb_prims.js` 判 **rc=1**
> ——6 个内节点代理 25~32 个 glTF primitive，破手册 §3 的 ≤24，也破本条 profile 自己声明的
> `maxPrimitivesPerTile=20`。上面这段没核结构预算，所以当时看不出来。定级与四条方向在
> AUDIT **A-51**（OPEN，需要用户裁定），在它闭合之前这份产物**不可发布**、#16 不算完成。


### `20240529卢沟桥分洪枢纽.fbx`（335 MB）— v15 被 A-41 判废；v16 中途主动终止（用的是"相对帧为空操作"的 22:40 二进制）；v17 已 rc=0 出盘但官方 validator 判 rc=1（26 个内容文件 ERROR，A-43）→ 未过发布闸；**2026-10-20 下午由 `lugouqiao_v19` 取代：4 597.57 s（1.28 h）、rc=0、validator 0 error，见下面 v19 一节**

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
  **真实 FBX 复扫（2026-10-20）**：同一工具在 `lugouqiao_v17` 的 1057 个内容文件上
  违例+解析失败 **0**（`max prims/tile=11`、实例展开最大 119 840）——累计 341+1057 个文件。
- B-10 **`conversion-report.json` 的输入/输出身份与字节四个字段恒空**（本轮发现，
  手册 §13.3 要求交付物自带可追溯统计，未闭合）：`ConversionReport` 声明并序列化了
  `inputFile`/`outputDir`/`inputBytes`/`outputBytes`（`include/3dtiles/production/ProductionProfile.h:114-123`、
  `src/3dtiles/production/ProductionProfile.cpp:93-100`），但全仓（`src/ tools/ tests/`）
  **没有任何一处赋值**。盘上实测 **220 份 `conversion-report.json`**（`F:/wb_3dtiles_out` 全树
  + `docs/model23dtiles-production/runs`）**220 份这四项全为 `""`/`0`，非空 0 份**——
  即这四个字段目前是"看起来有、实际恒空"的仪表，不能当统计用。
  后果：交付物内部**不自带**输入身份（A-42 之后这是有意为之的取舍，manifest `nodePath`
  只留 FBX 文件名，目录不再交付），也**不自带**产物字节总量。
  `BASELINE.md` §4 表格里的"输出字节"是外部 harness `scripts/run_matrix.py` 自己量的
  （`os.path.getsize(asset)` 与 `dir_bytes(outdir)`，第 132/140 行），不是 report 字段，
  两者不要混读。
  **更要紧的是今天这条路也断了**：`run_matrix.py` 是 Python，受 **B-5** 阻塞
  （本轮当场复测 `python -c "print('ok')"` → **rc=49**），现役替代链路
  `scripts/run_matrix.sh` + `scripts/collect_matrix.js` **只读 report 与 tileset，
  没有任何目录字节统计**（`collect_matrix.js:1` 的注释就是这个范围，
  `run_matrix.sh:5-6` 更明写 "collects the metrics the tool writes itself —
  no re-measuring"，即设计上就依赖 report 字段，而那四个恰好是空的）。
  即"输入/输出字节"目前**没有任何可跑的采集路径**：要么把 B-10 的四个字段接进管线，
  要么给 node 版矩阵补 `du` 级统计，二选一，否则手册 §13.3 的这条统计在文档里
  只有 2026-10-16 那批历史数字撑着。
  **修法**：把这四项接进管线（`outputBytes` 遍历 staging 的最终文件清单，别把
  `.wb-staging` 算两遍；`inputFile` 按 A-42 口径只写文件名），或者干脆从
  `ConversionReport` 删掉这四个字段并在文档里声明未实现——**留一个恒空的仪表比没有更坏**，
  它会让人以为统计已经存在。工作区里那份未提交的 `micro*` 计时（见下一节）如果落地，
  正好是同一处接线点。

## 工作区里那份**未提交**的改动（2026-10-20 记录，不属于本轮任何 commit）

`git status --short` 现在有 **7** 个已跟踪源文件被改但未提交（本轮没有编辑它们；但见本节末尾——
10:30 为验证 A-43 修复做过一次增量构建，这 6 份改动被一起编进了 `test_production.exe`）：

```
include/3dtiles/production/GeometryErrorEvaluator.h  +13
include/3dtiles/production/ProductionPipeline.h     +20
include/3dtiles/production/ProxyMeshBuilder.h        +6
src/3dtiles/production/GeometryErrorEvaluator.cpp   +171/-57
src/3dtiles/production/ProductionPipeline.cpp       +81/-10
src/3dtiles/production/ProxyMeshBuilder.cpp         +10
tests/production/main.cpp                          +156
```

（上面这组数是 2026-10-20 下午"#6/#7 实施 + 两处正确性修复"**之后**用
`git diff --numstat -- include src tests` 复测的；比 10:30 那版大得多，差额就是那次会话
落进未提交区的实现与用例，见下一节。提交前请再核对一次包含关系。）

> **2026-10-20 下午补记（提交裁定前置信息）**：`git diff --numstat` 复测仍是这 7 份、
> 数字**逐值未变**（`+13/+20/+6/+171-57/+81-10/+10/+156`），工作区没有再被并发改动。
> **`lugouqiao_v19` 就是用这 7 份改动编出来的二进制出的盘**，
> 所以"要不要提交"的判据已经齐了：1~2 h 目标达成（6.2×）、A-43 产物级复验通过、
> 逐字段对照证明只动误差侧、七项自研电池 + 官方 validator 全绿。
> 唯一还没过的是**视觉验收**（v19 一份都没看）。
> 本轮另外还改了 4 份文档（`STATUS` / `AUDIT` / `VALIDATION_REPORT` / `PERFORMANCE_REPORT`）
> 并新增 1 个未跟踪脚本 `scripts/report_tile_diff.js`，
> 它们与源码改动是同一批待提交内容。
> **一处需要接手人注意的时间戳异常**：`include/3dtiles/production/TileFeatureMapper.h`
> 的 mtime 是 **15:42:43**，晚于 v19 用的 exe（`Release/model23dtiles.exe` **13:09:09**），
> 但 `git status` 显示它与 HEAD 逐字节相同（最后一次改它的是 `262dc77`，10-19 16:39）。
> 其余 7 份源码的 mtime 都 ≤ 13:08:24，与那次构建自洽。
> 所以：**没有证据说明 v19 的二进制与当前源码不一致**，但也不能排除"构建后有人改了它又改回来"，
> 提交前如在意可重跑一次 `bash scripts/wb_build.sh model23dtiles` 确认无差异。

**这份清单在本轮会话期间自己变过一次**：09:53 记录时是 5 个文件（`+17/+6/+22-4/+30/+10`），
提交 validator 结论前复测 `git diff --numstat -- include src` 变成 6 个——多出
`GeometryErrorEvaluator.h` 的 `+2`（`ErrorResult::bvhBuildSeconds` 与
`DirectionResult::bvhBuildSeconds`，注释写"仪表：目标侧建树耗时，不参与判定"），
同时 `.cpp` 从 +22/-4 涨到 +27/-4、`ProductionPipeline.h/.cpp` 各涨 3/8 行。
**本轮没有任何构建或写源码的动作**，所以这是工作区被并发修改，不是记错——后续接手请
按"共享工作区"对待，任何提交前都要重新 `git diff --numstat` 核对包含关系。

> **2026-10-21 复测（提交裁定前置信息，仍**未提交**）**：`git status --short` 现在是
> **16 个已跟踪文件** = 12 份代码/测试 + 4 份文档。`git diff --numstat`：
>
> ```
> include/3dtiles/production/GeometryErrorEvaluator.h  +15
> include/3dtiles/production/ProductionPipeline.h      +90/-2
> include/3dtiles/production/ProductionProfile.h       +29
> include/3dtiles/production/ProxyMeshBuilder.h        +23/-3
> include/utils/GltfOptimizer.h                        +21/-2
> src/3dtiles/production/GeometryErrorEvaluator.cpp    +241/-71
> src/3dtiles/production/ProductionPipeline.cpp        +409/-51
> src/3dtiles/production/ProductionProfile.cpp         +11
> src/3dtiles/production/ProxyMeshBuilder.cpp          +39/-26
> src/utils/GltfOptimizer.cpp                          +137/-10
> tests/production/main.cpp                            +488
> tools/model23dtiles/main.cpp                         +11/-2
> docs: AUDIT +332/-5 · PERFORMANCE_REPORT +199/-1 · STATUS +272/-11 · VALIDATION_REPORT +132/-6
> ```
>
> **用户已裁定的拆分**（"纹理那条单独提交"）：
> - **提交 A（L0 仪表 + L1 遍历）**：`GeometryErrorEvaluator.{h,cpp}`（近孩子优先 +
>   `bvhBuildSeconds`）、`ProductionProfile.{h,cpp}`（`counters`、进程 CPU/壁钟秒）、
>   `ProductionPipeline.{h,cpp}` 的计时/报表部分、`tests` 里的 `bvhOrder` 与 5 条接线断言、
>   `GltfOptimizer.{h,cpp}` 里的 `micro_texture_export*` 落盘仪表。
> - **提交 B（内节点代理纹理，A-48/A-49/A-50）**：`ProxyMeshBuilder.{h,cpp}`（按 `osg::Image*`
>   去重的字节账）、`GltfOptimizer.{h,cpp}` 的 `resizeImageToPowerOfTwo` 改"返回私有副本"、
>   `ProductionPipeline.cpp` 的 cap 判定与 `proxyTextureMaxEdgePx` 传递、`tools/main.cpp` 的 `-ptw`、
>   `tests` 里的 `proxyTexture`/`proxyTextureLadder`/`texResize`。
> - **所以 `ProductionPipeline.cpp`、`GltfOptimizer.{h,cpp}`、`tests/production/main.cpp`
>   三份在两个提交里都有份**，必须按 hunk 拆（`git add -p`），**整文件 `git add` 会把两条线混进一个提交**。
>   文档四份同理（A-47 与 A-48/A-49/A-50 的条目混在同一批 diff 里）。
> - **哪个产物出自哪个二进制**（判"要不要提交"时别看错对象）：
>   `longxiang_final512`（report mtime 10:38:49）与 `longxiang_final2048`（10:41:21）
>   用**加落盘仪表之前**的二进制出的盘，含 A-48/A-49/A-50、**不含** A-47 的第四项仪表；
>   `longxiang_l0b`（11:01:02）用 **10:56 重建**的 exe，两组改动都在里面；
>   **`lugouqiao_v19` 是 10-20 13:09 的 exe 出的盘，不含本轮任何一条改动**，
>   它只能当 A-43/A-44/A-45/A-46 的验收产物，不能当这 16 份改动的验收产物。
> - 未跟踪内容（`.workbuddy/`、`build_*.log`、`RelWithDebInfo/`、`crash_events.txt` 等）
>   与本轮无关，**不要一起 add**。

读码得到的两点（第 1 点**仍未接线**，第 2 点同日已实施并验证完毕，见下一节）：

1. **子阶段计时仪表**：`ProxyResult::Metrics`（simplify / aggregate 秒数与候选数）+
   `TileHeavyWork` / 管线成员里的 `micro*` 累加，实测字段为
   `microProxySimplifySec`、`microProxyAggregateSec`、`microCollectSec`、
   `microEvaluateSec`、`microEvaluateSamples`、`microFinalizeSec`、
   `microAggregateCandidates`，本轮并发新增 `microBvhBuildSec`（配套
   `ErrorResult::bvhBuildSeconds` / `DirectionResult::bvhBuildSeconds`）。
   **半件状态（结论于同日下午作废）**：`ConversionReport` 那侧没有 `micro*` 命名字段
   （`grep -c micro src/3dtiles/production/ProductionProfile.cpp include/3dtiles/production/ProductionProfile.h`
   → **0 / 0**，2026-10-20 复测，这条 grep 本身仍然成立），
   **但"所以读不到数"是错的**：仪表改走通用的 `_report.addStage("micro_*", 秒)`
   （`ProductionPipeline.cpp:1219-1226`）落进 `stages` 数组，
   `lugouqiao_v19` 就是首份读数（PERFORMANCE §2.2）。
   教训：**别只按字段名 grep 就断言"读不到"**，要顺着写入路径找；
   这个误判把"6 千秒去哪了"推迟了一轮，也让 §2 那条待做项被重复记了一次。
   现在真正读不到的是另外三件事，见 AUDIT **A-47**。
2. **误差评估器改了上界与 `BOUND_LOOSE` 口径**（`evaluateDirection`）：
   未采样的三角形不再进堆，改为按解析量计入 `upper = max(upper, globalLower)`；
   已收敛的 patch（`maxSampleDist + radius <= globalLower + tolerance`）不再入堆；
   `budgetExhausted` 从"`samples >= max` 即算耗尽"改成"堆未空且预算耗尽才算，
   但有未采样三角形时仍算"。**这不是纯性能改动**——它动的是手册 9.3 第 6/9 条的
   保守上界语义与 `errorQualityStatus` 分布，`nodesBoundLoose` 计数会变。
   当时这条写的是"**若采纳**，需要：阳性对照（同一输入新旧上界逐节点对比、不许变小）、
   `test_production` 误差用例复跑、以及 12 资产矩阵的 `cert/loose` 对照，
   并且**必须重新出盘**才能进验收"。**这四件事已在同日（2026-10-20 下午）全部做完**，
   而且对照过程反过来查出两处缺陷（新口径自身的覆盖半径不成立 + `MeshBVH` 叶子索引错位
   使历史误差数整体虚高）。读数、阳性对照与遗留的判据问题见下一节；
   **v17 及之前所有产物仍不是这份代码出的**。

### 本轮（2026-10-20 10:30）的一次构建，把这 6 份未提交改动一起编了进去

A-43 写出侧修复以 **`49b6d3c`** 提交（`include/3dtiles/production/SourceModel.h` +17、
`src/3dtiles/production/SourceModel.cpp` +110/-1、`tests/production/main.cpp` +128、
`scripts/zero_normal_repair_plan.js` 新增）。提交只 `git add` 这 4 个路径，上面那 6 个文件
**仍在未提交区、未被编辑**。

- 命令：`cmake --build build_wb --config Release --target test_production`（只建测试目标，
  没碰正被 v18 占用的 `Release/model23dtiles.exe`；日志 `F:/wb_3dtiles_out/build_a43norm3.log`，rc=0）
- 读数：`nrmRepair` 单独跑 **17/17**，全量 **passed=167 failed=0**，退出码 **0**
  （在 `F:/wb_3dtiles_out/a43_nrm_test/` 下运行，避免覆盖仓库根的 `test_production_progress.txt`）
- **口径提醒**：这份二进制的翻译单元列表实测含 `ProductionPipeline.cpp`、`ProxyMeshBuilder.cpp`、
  `GeometryErrorEvaluator.cpp` 三份未提交改动，所以"167 条通过"**同时**是这 6 份改动的一次
  未分级观察：`error` 那 9 条在新 `evaluateDirection` 口径下没报错，但它既不构成上面要求的
  新旧上界逐节点对照，也不构成 12 资产 `cert/loose` 对照——别把它当成那两件事已做。

## 2026-10-20（下午）：#6 乱序收割 + #7 节点内并行落地，并顺路查出误差侧正确性缺陷（AUDIT A-44/A-45/A-46）

本轮目标是手册 §13 里剩下的两条**实测有效**的杠杆（BVH 缓存、聚合剪枝两条已分别实测否决，
见 PERFORMANCE 与未闭合清单）。两处实现都落在未提交区，**没有提交**（提交与否待用户裁定）。

### 实现

1. **#6 瓦片级乱序收割 + 顺序提交**（`ProductionPipeline.cpp` +81/-10）：
   原来"按瓦片 id 顺序 `wait` 第 i 个 future"会让主线程被最慢的那个瓦片钉住
   （straggler）。改成 `std::async` 滑动窗口：任务按**子树三角形数降序**投递，
   任何 job 完成就先进 `ready`（乱序收割），**写出仍严格按瓦片 id 顺序**；
   窗口 = `jobs × 2`，并且 `inflight + ready <= 窗口` 合并限流以保持内存中性
   （`WB_JOBS` 仍为 8）。判定/编码本身没动，只动了等待与提交次序。
2. **#7 节点内并行**（`GeometryErrorEvaluator.cpp`）：初始采样那一刀（每三角形 7 点、
   单节点可达数十万点）按 65 536 三角形分块并行查 BVH；线程数 =
   `hardware_concurrency / activeTileTasks`，由 `TileTaskScope`（新 RAII，见头文件 +13）
   维护活跃瓦片计数，所以 #6 的 8 个瓦片并行时每个只用 3 线程，不会互相踩踏。
   `WB_EVAL_THREADS=0/1` 显式关闭，用于同时段 A/B。
   **确定性来自设计而非运气**：并行部分只算"每块的距离数组"，结果仍按原始顺序
   串行入堆，所以与单线程**逐位相同**；`quantileKeep=262 144` 用确定性蓄水池采样。

### 计时口径（重要，别再拿 wall 比）

同一会话内交替跑 off/adp，主指标用**进程 CPU 秒**
（PowerShell `Get-Process TotalProcessorTime`，脚本 `F:/wb_3dtiles_out/cpu_ab.sh`，
原始读数 `F:/wb_3dtiles_out/cpuab.log`，同时记录 `Win32_Processor.LoadPercentage`）——
本机 wall 时间受外部负载漂移 2~3 倍，只有 CPU 秒可比。
脚本坑一处，已修：`mode=adp` 必须 `Remove-Item Env:WB_EVAL_THREADS` 而不是设成空串，
空串 `atoi` 得 0 会**静默关掉**并行、把 A/B 做成同配置对照。

| 资产 | 配置 | CPU 秒（两次重复） | wall（同轮） | 验收字段 |
|---|---|---|---|---|
| t03（7 瓦片） | off | 453.3 / 425.9 | 184.34 / 160.40 s | `errMax=105.5606 loose=3` |
| t03 | adp | 460.0 / 436.1 | **112.54 / 96.42 s** | 同上，逐字相同 |
| t04（127 瓦片） | off | 107.4 / 110.7 | 37.77 / 39.38 s | `errMax=17.3600 loose=63` |
| t04 | adp | 105.1 / 107.2 | **34.66 / 35.64 s** | 同上 |
| longxiang.fbx（129 瓦片，真实输入） | off | 606.4 / 637.8 | 144.46 / 146.53 s | `errMax=94.5155 cert=2 loose=60 warn=52` |
| longxiang | adp | 557.0 / 616.5 | 133.20 / 144.50 s | 同上 |

读法：**t03 的 wall 1.64×/1.66× 是在 CPU 秒持平（453→460、426→436）的前提下拿到的**，
即纯并行收益、不是把工作量搬到了别处；t04 与 longxiang 提升 8~11 %，说明这两个数据集
的耗时不集中在"单节点初始采样"上，而且**没有出现超线程互相抢 CPU 的退化**
（CPU 秒不升）。三组的验收字段在 off/adp 之间逐字相同，与上面"确定性来自设计"一致。
**335 MB 的 `lugouqiao` 已在同日 15:36 用这份二进制跑完（`lugouqiao_v19`）**，
实测 `totalSeconds=4 597.57`（v17 = 28 485.37，**6.2×**），1~2 h 目标**达成**，
逐字段对照与产物级验收见下面"v19 全量重跑"一节。

### 对照过程中查出的缺陷（都不是本轮新写的并行导致的）

- **D1｜覆盖半径口径在采样被降级时不成立**（= **AUDIT A-45**，HEAD 里就存在）。
  口径靠"patch 覆盖半径"撑上界，半径系数原先一律取 `L/3`，那**只有在全 7 点集
  （3 顶点 + 3 边中点 + 质心）都采到**时才成立；采样密度被预算压到只剩顶点时只保证
  `L/√3`（锐角 `L/(2 sin θ_max)`，θ_max∈[60°,90°) → ≤ L/√3；钝角外接半径 `L/2`）。
  HEAD 的密度自适应还能把 `perTri` 一路降到 **1**，连三个顶点都不保证查全。
  已改成：顶点永远查（`perTri = 3 + extra`）+ 系数按实际密度取（`extra == 4` 才用 `1/3`）。
  阳性对照（`testErrorBoundSoundness` 第 1 段：内接等边三角形最长边 `L = √3`，
  `maxSamplesPerNode=3` ⇒ 只剩 3 顶点、细分预算 0）**在修复前精确失败**：
  `upper=0.577350` = `L/3`，而顶点集合口径要求 `L/√3 = 1.000 ≥ 0.98` ——
  **这是"把上界写小"，方向正是手册禁止的那一侧**。
  同时把"剪枝"改成**只省算力、不改口径**：被剪掉的 patch 记进 `prunedUpper`，
  深度上限 patch 置 `depthCapped` 并计入 `upper`。
- **D2｜`MeshBVH` 叶子按位置取三角形**（= **AUDIT A-44**）。
  `buildRecursive` 用 `_tris[order[i]]` 算节点 AABB，但查询的叶子循环读的是 `_tris[i]`——
  重排只落在 `order` 上、从未落到 `_tris`，于是叶子读到的是**不属于这个盒子**的面片。
  只会偏大（同 soup 内别的面片），不会偏小，所以**没有**"把上界写小"的违规，
  但实测能把最近距离放大到 **3.7 倍**：`geometricError`、`errorMaxMeters`、p95 **全部虚高**，
  代理被判得比实际粗糙，收敛判定因此从来不成立。修法是建树后把置换真正落到数组上
  （`sorted[i] = _tris[order[i]]`）。回归对照 `testMeshBVHMatchesBruteForce`：
  64 三角形 × 6 探针，oracle 取"每面一个单三角形 BVH（必为叶子 ⇒ 精确）"的最小值，
  **修复前 6 个探针 4 个失败**。
  > **这条改变历史数字的可信度**：2026-10-20 之前所有 `conversion-report.json` 的误差字段
  > （`errorMaxMeters` / `geometricError` / p95）都被放大了 1~3 个数量级，
  > **跨版本比误差数字没有意义**；但结构类字段（瓦片数、`maxTrianglesInTile`、
  > `aggregateProxyCount`）和产物字节基线（`ab_ctrl` / `ab_par_off` / `mx_sfx`）
  > 对"结构/负载不变"仍然有效。
- **D3｜`maxSamplesPerNode = 0` 会把"一个样本都没采"报成"误差 0"**（= **AUDIT A-46**，
  防御性加固，不是现场缺陷）。预算为 0 时旧实现的上界是 0 —— 又是"把上界写小"那条方向。
  现在 `budget` 下限锁 1、`perTri` 下限锁 3、未采样面按 `globalLower` 进账。
  **历史 12 资产与三份真实 FBX 用的都是默认 400 000，没有出现过 0**，所以不改写任何既有结论。

### 已做完的验证（对应上一节那条"若采纳需要"清单）

- `./Release/test_production.exe` → **passed=181 failed=0**，退出码 **0**
  （含新增 `bvh`、`errorSoundness` 两组用例与它们的阳性对照；上一节的 167 是修复前那版）。
- **新旧上界逐节点对照**（12 资产矩阵 `F:/wb_3dtiles_out/mx_bvh`，总耗时 ≈148 s）：
  215 个瓦片里 **100 个自身上界变小、0 个变大、115 个不变**，`upper < lower` 违例 **0**，
  b3dm/i3dm **内容文件改动 0 个**（这几处修复都只影响误差侧，不动几何负载）。
- `node scripts/tileset_bounds.js F:/wb_3dtiles_out/mx_bvh` → **12/12 数据集干净**。
- `bash scripts/run_validator.sh F:/wb_3dtiles_out/mx_bvh F:/wb_3dtiles_out/validation_mx_bvh`
  → 12 个 tileset **errors=0**；warnings 全 0，只有 t12_big_texture 那 1 条
  （KTX2 `KHR_texture_basisu` 老面孔，口径见 VALIDATION_REPORT §7），
  逐份报告存 `F:/wb_3dtiles_out/validation_mx_bvh/*.validator.json`。

### 一个我没有"顺手修掉"的后果，需要用户裁定

距离算对之后，**12 个小资产矩阵的 `nodesCertified` 全部变成 0**（t04 从 63 → 0），
所有节点都是 `BOUND_LOOSE`。**但"全部归零"不适用于 335 MB 档**：`lugouqiao_v19` 实测
`nodesCertified` 450 → **63**、`nodesBoundLoose` 332 → 464，逐节点状态迁移是
`CERTIFIED→BOUND_LOOSE 387`、`CERTIFIED→CERTIFIED 63`、`LOOSE→LOOSE 15`、
`EXACT_LEAF 466`、`NOT_EVALUATED 128` —— 是**资产相关**的，不是普适归零。
原因是诚实的：`CERTIFIED` 判据是"上界 − 下界 ≤ 1 mm
绝对值、或 ≤ 上界的 2 %（`certifyRatio`）"，而上界与下界的差就是覆盖半径 ≈ patch 尺寸，
在 40 万样本预算内数学上够不着。**这不是回归，是把以前虚高的"合格证"还原成诚实标注**——
产物仍然安全（上界只可能偏大），但 `errorQualityStatus` 分布全变。
合法杠杆只有两条：① 加大 `maxSamplesPerNode`（纯烧时间，直接和"1~2 小时"目标冲突）；
② 接受诚实的 `BOUND_LOOSE`。**③ 调低 `toleranceMeters` / 放宽 `certifyRatio` 属于放宽验收，
按手册禁止，我没有动，也不建议在没决定的情况下动。**

## 2026-10-20（下午 14:15→15:36）：`lugouqiao_v19` 全量重跑，1~2 h 目标达成 + A-43 产物级复验通过

出盘命令与监视：`F:/wb_3dtiles_out/run_lugouqiao_v19.sh`（PID 22714）、
`F:/wb_3dtiles_out/mon_lugouqiao_v19.ps1`（脱离 harness 超时约束的 detached 监视），
读数在 `F:/wb_3dtiles_out/lugouqiao_v19/`、`F:/wb_3dtiles_out/validation_v19/`、
`F:/wb_3dtiles_out/validation_v17_control/`。二进制 = 上一节那份**未提交**改动编出来的
`Release/model23dtiles.exe`（含 #6/#7 与 A-43/A-44/A-45/A-46 四处修复）。

### 速度：目标达成，不是外推

| 指标 | v17（2026-10-20 00:35→08:35） | v19 | 判读 |
|---|---|---|---|
| `totalSeconds` | 28 485.37 s（7.91 h） | **4 597.57 s（1.28 h）** | **6.2×**，落在 1~2 h 目标内 |
| `peakRssBytes` | 9 830 379 520（9.15 GiB） | **7 445 286 912（6.93 GiB）** | 并发上去后内存反而**降 24 %** |
| 进程 CPU 秒（监视器末样本累计） | 未采 | ≈ 12 168.6 s / 4 549 s wall ⇒ **平均 2.7 核**（机器 24 逻辑核） | 并发度远未吃满，见下面"还剩多少余量" |
| `errors` | `[]` | `[]`，出盘 `rc=0`（`$?` 不经管道取） | — |

### 质量：逐字段对照证明"只动了误差侧"

`conversion-report.json` 的 1 059 条瓦片记录按 `id` 对齐后
（`node scripts/report_tile_diff.js`，本轮新加的对照尺）：

- **`triangles` / `contentBytes` / `uri` / `refine` / `hasContent` / `isLeaf` / `isProxy` /
  `aggregate` / `aggregateSources` / `sourceFeatures` 差异数全为 0** —— 瓦片层级、负载字节、
  分发文件名、身份与覆盖范围与 v17 **逐值相同**，运行时细化行为没有任何改变。
- 只有 `errorUpperM`（465）、`errorLowerM`（461）、`geometricErrorM`（466）、
  `errorStatus`（387）、`measuredProxyErrorM`（465）这五个**误差字段**变了。
- 自身上界方向：**450 个变小、15 个变大、594 个不变，`upper < lower` 违例 0**。
  变小来自 A-44（距离被系统性放大，机制上是"子集最小值 ≥ 真最小值"，只会偏大），
  变大的 15 个来自 A-45（覆盖半径系数从不该用的 `L/3` 改回顶点集合口径 `L/√3`）——
  两个方向都有机制证明，不是抽查。
- 顶层误差统计：`errorMaxMeters` 8 857.99 → **2 475.43**、`errorP95Meters` 3 502.99 → **878.01**、
  `errorMeanMeters` 117.70 → **26.45**、`tilesetGeometricError` 与 `errorMaxMeters` 仍然逐值相等。
  **这些是修复后的诚实读数，不能与 v17 及更早的任何误差数字比大小**（见 D2 那条口径注）。
- 其余结构/硬限字段（`tileCount=1059`、`contentTileCount=1057`、`instanceTileCount=126`、
  `maxDepth=18`、`maxTrianglesInTile=119 840`、`maxContentBytes=3 237 360`、
  `aggregateProxyCount=122`、`sourceFeatureCount=2729`、454 条 warning、
  `proxySkippedCount=0`、`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0`）
  **与 v17 逐字相同**。

### 产物级验收（七项自研尺 + 官方 validator，全部同轮、同机、逐条取退出码）

| 判据 | v19 读数 | rc |
|---|---|---|
| 官方 3d-tiles-validator（`validation_v19/`） | **numErrors=0**；叶层展开 ERROR=**0**、`unit length`=**0** | 0 |
| 同一 validator 对 **v17** 复跑（`validation_v17_control/`，阳性对照） | 头 26 errors / 叶层 **195** 条 `Vector3 at accessor indices #..# is not of unit length: 0`，全部落在 `attributes/NORMAL` | 1 |
| `zero_normal_triage.js` | 零长度 NORMAL 顶点 **0**（v17 同尺同轮读数 **195** = 退化 140 + 非退化 55） | 0 |
| `zero_normal_tile_class.js` | `files=0 zeros=0` | 0 |
| `i3dm_pose_check.js` | 相对帧 **1787/1787** 对上、残差最大 1.85e-5 m、绝对帧 0 命中、POSITION 全 0 的瓦片 0/126、GLB 节点 TRS 0、原型 `\|v\|` 最大 383.48 m | 0 |
| `accessor_extents_scan.js` | 931 b3dm / 126 i3dm / 4430 个 VEC3 accessor：声明≠实算 0、非有限分量 0、`>1e6` 0/0；`\|c\|` 最大 4 337 m（b3dm）/ 383.5 m（i3dm） | 0 |
| `tileset_bounds.js` | 父不覆盖子 0、NaN box 0、负半轴 0、非法 ge 0、子级误差大于父级 0 | 0 |
| `glb_prims.js` | 1057 个内容文件 违例+解析失败 0；`max prims/tile=11`（`t53.b3dm`）、实例展开最大 **119 840**（`i1042.i3dm`）与 `report.maxTrianglesInTile` 对上、去重侧 37 885、展开合计 11 350 646 | 0 |
| `check_manifest_assertions.js` | `aggProxies=122 / aggBuilt=122 / aggSkipped=0`、`occurrences=4250`、`maxTris/tile=119 840` → PASS | 0 |
| `path_leak_structural.js` | 1057 瓦片 + 3 旁证：**CLEAN**，尺子失效 0；裸正则 4 处命中**全部落在 `glbBIN_1`**（GLB 二进制 chunk 字节噪声） | 0 |
| ↳ 同一尺子的阳性/阴性对照（用 v19 的 `t0.b3dm` 现造 fixture） | 注入盘符 → `rc=1 LEAK（1 处）`、命中区 `glbJSON_0`；只注相对 uri → `rc=0 CLEAN` | 1 / 0 |
| ↳ 本轮新增对照尺 `report_tile_diff.js`（三方对照，证明它不恒绿） | v19 vs 自身 → PASS（差异 0）；v17 vs v19 不带 `--allow-error-diff` → **FAIL on 5 个误差字段**；带上 → PASS（负载/结构差异 0）；**v15 vs v17 → FAIL on `contentBytes=693`**（那轮修的是负载，正应该被拦），同轮误差字段差异 0（1059 条上界逐值不变） | 0 / 1 / 0 / 1 |

> `path_leak_scan.js` 对 v19 报的 `t688.b3dm`/`t840.b3dm` "1 处命中"**不是泄漏**：它的裸正则
> 在二进制 float 里会撞出 `"q:/…"` 这种伪路径，脚本自己的头注就写明"那种命中不能当证据"。
> 证据口径是 `path_leak_structural.js` 的分区判定（上面两行）。**这条悬了三轮的歧义到此关闭。**

**A-43 的产物级复验到此闭合**：修复前 195 条 / 26 个文件，修复后 0 条 / 0 个文件，
且 validator 的其余叶层计数在两份产物之间**逐类相同**（1969 / 1569 / 1569 / 691 / 7），
说明这轮改动只把零长度法线换成面法线，没有顺带改变别的东西。
`mx_bvh` 那份不能当这个的证据（小资产本来就没有零长度法线）——这一条以前写在
VALIDATION_REPORT §7，现在由 v17/v19 对照补上了。

### 还没做完的（不写成"已验收"）

- **视觉验收**：v19 一份都没看。判据本身还在重做（未闭合第 7 条）。
  产物级全绿 ≠ 可发布。
- **`micro_*` 仪表的坑**：`micro_evaluate_samples`（438 681 617）和 `micro_aggregate_candidates`（122）
  是**计数**，却写进了 `stages` 这个**秒**数组（`ProductionPipeline.cpp:1219-1226`）。
  读表时这两行不是耗时；已经记为待修（PERFORMANCE §6）。
- **还剩多少余量**：`micro_evaluate=7 510.16 s`（跨线程累加）对 4 592.42 s 的阶段 wall
  ⇒ 评估平均只吃到 **1.63 核**；进程总 CPU 秒 ≈ 12 168.6 s / 24 核 ⇒ **机器 89 % 是空的**。
  同时 `micro_*` 累加只有 ≈8 240 s，与 12 168 s 差 **≈3 900 s（32 %）没有归属**——
  `processTileHeavy` 里叶瓦片的 `_model.materialize()`（`ProductionPipeline.cpp:613`）
  **没有计时器**，代理 `build()` 也只有 simplify/aggregate 两项。下一轮提速必须先补这个洞，
  否则改错了地方。口径与杠杆清单见 PERFORMANCE §2.2。

### 两条 harness 教训（这一轮踩的）

- `run_lugouqiao_v19.sh` 那版 bash 包装**跑出 rc=127 并留下孤儿 `model23dtiles.exe`（PID 36528）**：
  包装脚本自身在 `wait` 之前退出，退出码就没了。改 PowerShell detached 监视
  （`$p.HasExited` 之后读 `$p.ExitCode`）才拿到可信的 `rc=0`。
- 监视表里的 `rssMiB` 一列**全程钉在 2527**（`WorkingSet64` 快照被缓存），不可信；
  内存只看 `report.peakRssBytes`。


## 2026-10-21：L0 仪表收口 + L1 BVH 遍历 + 内节点代理"顶层无贴图"（A-47/A-48/A-49/A-50，产物级复验又破出一条 **A-51 OPEN**）

**未提交**（16 个文件：4 份文档 + 12 份代码/测试）。三件事按用户裁定串在一起做：
L0/L1 属于"先补仪表再谈提速"，代理纹理是"顶层无贴图是不可接受的"这条硬要求。

### L0：A-47 的三个洞补完了，判据①闭合之后**结论换了对象**

- 加了 ①叶瓦片 `materialize` 计时、②逐瓦片 `timing`（129/129 瓦片有
  `evaluateSec/evaluateCpuSec/evaluateWorkerCpuSec/evaluateSamples/heavySec/
  materializeSec/proxyBuildSec/collectSec/bvhBuildSec/finalizeSec/writeSec`）、
  ③`ConversionReport::counters`（`micro_evaluate_samples` 48 963 414、
  `micro_aggregate_candidates` 10 搬出 `stages` 这个秒数组）。
- **补了第四个洞才闭合判据①**：`GltfOptimizer::textureExportStats()` 量纹理落盘的
  壁钟 / 调用线程 CPU 秒 / **进程** CPU 秒之差（`micro_texture_export*`）。
  `longxiang_l0b` 实测：写出阶段调用线程只花 64.77 s，同窗口进程花 **257.75 s**
  ⇒ **192.98 s（全进程 CPU 的 35.6 %）是 libktx 写 `.ktx2` 时自起的 basis 压缩线程**
  （`threadCount = hardware_concurrency`，`LoadTextureKTX.cpp:630`），
  生产侧所有 `micro_*` 都是线程口径，一秒也记不到它。
  归属合计 547.41 s 对进程 542.91 s ⇒ **残差 −0.8 %**，判据①（≤ 5 %）在 65 MB 档成立。
- **上一轮排错了对象**：`materialize` 只值 **0.09 CPU 秒**，不是那 32 %/35 % 的嫌疑人。
- 判据①只在 65 MB 档实测闭合，**335 MB 档要等 `lugouqiao_v20` 用新二进制出盘复测**，
  所以 A-47 的状态写成"FIXED（65 MB 档实测，335 MB 档待复测）"。
- **计时口径本轮有了硬数据**：同一份 `longxiang.fbx` 的 8 跑里
  `micro_evaluate_samples` 恒为 48 963 414（工作量完全相同），
  `micro_evaluate_cpu` 却在 **145.22 ~ 310.73 s**（2.14×）之间跳、wall 在 122 ~ 436 s（3.6×）跳。
  ⇒ **单跑读数不能判任何杠杆**；L1 那对 `310.73 → 151.02` 因此**不是提速证据**（见下）。

### L1：MeshBVH 最近孩子优先遍历

判据是"可证明产物逐位相同"：只改遍历顺序、不改被选中的样本集合，
回归测试 `bvhOrder` 钉住"同一批样本、任意遍历顺序、结果逐位一致"。
**提速收益目前没有证据**（口径就是上面那段 2.14× 的抖动），
要等 335 MB 档 ≥3 次交错重复才有读数。

### 内节点代理纹理：从"整片剥成灰模"到"按预算降分辨率"

用户的判据：**顶层无贴图是不可接受的**。修前三步（A-48）+ 修完自己引入的串瓦片缺陷
（A-49）+ 两条"产物自证不了自己"的观测缺陷（A-50），全部在
`docs/model23dtiles-production/AUDIT.md` 有条目、有产物级复验表。

产物级读数（`longxiang.fbx`，129 瓦片 / 60 个有内容的内节点代理）：

| 指标 | 修前 `longxiang_covfix` | A-48 后 `longxiang_ptw2` | A-49 后 `longxiang_final512` |
|---|---|---|---|
| 内节点代理**有纹理** | 18 / 60 | 58 / 60 | **58 / 60** |
| 无纹理内节点 | 42（含顶层 `t0`） | 2（`t73`/`t74`，源本就无图） | **2** |
| 顶层 `t0.b3dm` | 357 944 B / 0 张图 | 903 816 B / 15 张图 | **903 816 B / 15 张图** |
| 叶瓦片合计字节 | 42 036 KB | **33 817 KB（被代理带小 19.5 %）** | **42 021 KB** |
| 内节点合计字节 | 41 874 KB | 43 917 KB | 43 706 KB |
| 内节点 >8 MiB 硬限 | 0 | 0 | **0**（最大单文件 2480 KB） |
| 峰值 RSS | 2 401 MB | 3 631 MB | 3 637 MB |

`-ptw 512` vs `-ptw 2048` 同二进制两跑：内节点 43 706 KB vs 119 737 KB（上限省 63 %），
叶瓦片 64/67 逐字节相同。**这个"相同"要跟噪声地板一起读，而地板本轮又涨了**：
`final512` vs `l0b`（只多落盘仪表的功能等价二进制）差 **8 个瓦片**
（已知的 3 张叶 `t40`/`t78`/`t119` + 5 张代理 `t25`/`t43`/`t62`/`t105`/`t106`），
但每瓦片 `triangles`、五个误差字段、结构计数与 `errorMax/p95/mean` **逐值不变**
（总字节差 −9 920 B = 0.01 %）⇒ A-40 家族的噪声，不是回归；
`report_tile_diff.js` 在这种纯观测改动下判 FAIL(`contentBytes=8`)，
**是噪声**，所以 PERFORMANCE §2.2 的"每根杠杆重复 ≥3 次"从叶扩到代理一起对照。

RSS 从 2 401 MB（covfix）跳到 3 631 MB 这一档**不是本轮纹理改动吃的**：中间还夹着
A-41/A-42/A-43、#6 乱序收割、#7 节点内并行三批改动，未做单独归因，记为**未证实**。

### 产物级复验又破出一条：A-51（OPEN，阻塞本条验收）

上面的表只核了**字节**预算（内节点 >8 MiB = 0 张）。本轮补核 **primitive/材质结构预算**
（`scripts/glb_prims.js` + 逐瓦片脚本 `F:\wb_3dtiles_out\per_tile_prims.js`）之后：

| 数据集 | 叶 prims max/mean | 代理 prims max/mean | 代理 图去重后 max/mean | 代理 prims>24 | 代理无纹理 |
|---|---|---|---|---|---|
| `longxiang_covfix`（修前） | 11 / 3.6 | 10 / 2.6 | 6 / 1.1 | 0 | 42 |
| `longxiang_final512` | 11 / 3.6 | **32 / 12.7** | 18 / 7.6 | **6** | 2 |
| `longxiang_final2048` | 11 / 3.6 | 31 / 12.7 | 19 / 7.6 | 6 | 2 |
| `longxiang_l0b` | 11 / 3.6 | 32 / 12.7 | 18 / 7.6 | 6 | 2 |

`glb_prims.js` 对 `final512` 判 **rc=1**（`t0`=27/`t1`=26/`t21`=28/`t29`=25/`t79`=32/`t100`=28 > 24），
对 `covfix` 判 rc=0（全库最大 11）。叶瓦片四份**逐值相同** ⇒ 退化完全在代理侧，
与 A-49（代理串改叶贴图）是两条独立的事。**纹理保住的代价是把远层 LOD 的 draw call
从 1~10 抬到 25~32**，三角形/实例/字节都没变，所以七项自研尺子里只有 `glb_prims.js` 看得见它。
机制、四条方向与各自上限见 AUDIT **A-51**；那条同时记下了一条通用缺口：
`maxPrimitivesPerTile`/`maxMaterialsPerTile` 只在源侧切分被消费，**代理路径与 `selfValidate`
都没有对应的闸**（A-32/A-33 同族）。

### 单元测试

`test_production` **277/277**（2026-10-21 晚 A-52 回归用例落地后重编重跑；上一读数 263 是
A-51 方案 A 那轮拿的，新增 `atlasCanvas` **14** 条。再往前：263 = 228 + 两组共 **35** 条 ——
`foldUv` **26** 条〔折叠算式、原始 UV
未被污染、同纹素等价、还原后指针同一、共用宿主必须全可折、BASECOLOR/NORMAL 不一致拒绝、
`texCoord!=1` 拒绝、恒等折叠仍可撤销〕+ `budgetGate` **9** 条〔假 b3dm 打正反两侧：真实产物
解析得动、正好卡上限放行、多 1 个 primitive / 多 1 个材质各自必拒〕，条数按
`tests/production/main.cpp` 里两个函数体的 `check(` 调用数实测，26+9 正好等于 263−228；
涉及上一轮工作的四组仍是 `proxyTexture` 13 条、`proxyTextureLadder` 6 条、`texResize` 11 条、
`pipelineCoverage` 5 条）。
**`atlasCanvas`（A-52）第一版同样是空过，靠负对照才抓出来**：它只在 pack 前塞一个 1 MiB 的
`0xFF` vector，想着"堆会把它回收给画布"。实测把 `TexturePacker.cpp` 的清零去掉重编，
这条用例仍然 **10/0 通过** —— 70×50+60×40 这种小画布直接向系统要新虚拟页，天然全 0，
脏 vector 根本没被复用。改成先用第一轮探出画布真实字节数、再把**同尺寸**堆块涂成 `0xFF`
并释放、连跑 6 轮取和之后：无清零 → `dirty=600/3600`，**3/3 次 FAIL（rc=1）**；
装回清零 → **14/0**，3/3 次 PASS。所以这条用例现在有判别力，判据是负对照而不是我的推理。
**`budgetGate` 差点是空跑**：它最初把 `selfValidate` 指向 `outputDir/tileset.json`，而那份
只有 `finalize()` 成功时才会写出来，测试里根本没调 `finalize` ⇒ 三条"超标必拒"全过是因为
校验器压根没读到文件。改成测试自己 `writeTilesetJson(.wb-staging/tileset.json)` 之后，
阳性对照①②才真的有判别力。教训同上一轮：**0 结果必须配阳性对照，阳性对照本身也要能说话**。
接线断言只保证"字段存在且计数没漏进秒数组"，**数值闭合靠产物级复跑**——
这条本身就是为了上一轮"仪表在算但没接进 report"的教训。

### CLI 新增

`-ptw <px>`：内节点（远景 LOD）代理的纹理边长上限，默认 **512**；
低于 `proxyTextureFloorEdgePx`(64) 由 `profile.validate()` 直接报错，不静默钳制。
`-tw/-th` 帮助文案里写错的默认值（256）一并改成实测的 2048。

### 下一轮（按证据强度，不是按好写）

L0 已经改变结论：新增 **L7 = 纹理落盘的 basis 线程配额**（35.6 % 进程 CPU 在仪表之外，
`threadCount` 是否改变 ETC1S 压缩结果**未验证**，动它之前要产物对照 + validator + 视觉）。
2026-10-22 复核：**这一格仍然维持"未验证"** —— 当日上午宣称"已回答"的那对 A/B 用的是不含
`WB_KTX2_THREADS` 的旧 `osgdb_ktx.dll`，属无效对照；理由与重做条件见本文下方
"提速线（L7 前置问题：原对照实验无效，待重做）"。
L5（写出并行）在 65 MB 档的上限是 67 % 而不是 12 %（资产相关，别把 335 MB 档的比例当通用），
**但 L5 不能排在 L7 前面做**：写出并行会把"1 张图 × 24 线程"变成"N 张图 × 24 线程"。
先等 v20 带新仪表复测判据①与这两根杠杆的真实占比。

## 未闭合（写在这里，不靠记忆）

1. **带 2b 闸的二进制已在 12 资产矩阵与 335 MB 真实 FBX（`lugouqiao_v17`）上实测通过**。闸门**第一版是错的**：
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
   `20240529卢沟桥分洪枢纽.fbx` **已以 `lugouqiao_v17` 跑通并落盘**（10-20 00:35→08:35，
   28 485 s，rc=0，1057 个分发文件，七项自研判据逐条 rc=0，见本节第 5 条与
   VALIDATION_REPORT §6.2 的"v17 电池读数"表）——所以"真实 FBX 用带闸 + 修好参考系 +
   修好位姿的二进制跑通"**这一条已闭合**。官方 validator 本轮（2026-10-20）也跑了，
   **结果不是通过**：`bash scripts/run_validator.sh F:/wb_3dtiles_out/lugouqiao_v17` → **rc=1，
   26 个内容文件 / 叶层 195 条 ERROR**，消息全是 `Vector3 … is not of unit length: 0`
   （`/meshes/*/primitives/0/attributes/NORMAL`），与自研 `zero_normal_triage.js` 的
   195 = 140（只落退化三角形）+ 55（落非退化、涉及 19 文件）**逐顶点对上**，即 A-43 的签名；
   同法对 `matrix_a42fix` 12/12 与 `longxiang_a42` 判 **0 error**，所以不是校验器在乱报。
   **A-43 写出侧修复已在 `49b6d3c` 落地**（`SourceModel::repairZeroNormals`，物化出口按三角形
   拓扑重建，`nrmRepair` 17 条 + 全量 167 条 `test_production` 通过），但这只是**代码级**证据：
   尺子分档实测 v17 的 195 条 = 55 个可由自身入射非退化面修复 + 140 个退到本图元平均面法线 +
   **0 个无解**（`scripts/zero_normal_repair_plan.js F:/wb_3dtiles_out/lugouqiao_v17`，
   读数存 `F:/wb_3dtiles_out/v17_checks/zero_normal_repair_plan.txt`），
   所以修复不需要编造方向。**产物级复验还没做**——需要一个用 `49b6d3c` 之后二进制出的盘。
   本条剩下的未闭合项因此是**这份产物的视觉验收**与**修复后的重新出盘 + validator 复跑**。
   > **v18 不是修复后的那一轮**（10:16 起在跑，`Release/model23dtiles.exe` 时间戳 **10:00**，
   > 早于 `49b6d3c`）：命令 `model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/20240529卢沟桥分洪枢纽.fbx
   > -o F:/wb_3dtiles_out/lugouqiao_v18`。10:35 观测：进程仍在跑（CPU 9036 s、RSS 6.8 GB），
   > 输出目录只有 `textures/`（2 个文件），`tiles/`、`tileset.json`、两份 json 都还没出现。
   > 它出盘后如果 validator 仍报同样 195 条零法线，**不是回归**，
   > 是这轮本来就没带修复；也别把 v18 当成 A-43 的验收产物。
   另两份真实输入的现状（本轮盘上实测，旧文档写的
   "未跑／历史产物"都不准确）：`芜湖水厂总装.fbx` 只有 `wuhu_v1`，是 A-41/A-42 之前的
   二进制产物，`path_leak_structural` 报 226 处瓦片泄漏、`i3dm_pose_check` 报 5/5 个
   i3dm POSITION 全 0，**判废、需用当前 HEAD 复跑**；`jianzhu+tietu.FBX`（433 MB）
   盘上 5 个目录**从未产出过 `tiles/` 或 `tileset.json` 或 report**，只有 `textures/`
   和 `.wb-staging/tiles/` 里的半成品 b3dm（5 次都停在写出中途，且残留说明没走到
   §14.17 的失败清理——终止原因未查）。
   连"旧二进制产物"都谈不上。
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
   **第三条候选（FBX 读取器侧）已单独存档**：`evidence/fbx_reader_candidate-mechanism.txt`
   ——`getVertexIndex:74-102` 的 `default` 分支只 warn 一句就带 `index = 0` 继续、
   `getElement:124-131` 的 `GetAt` 不校验上界（SDK 头明写越界不抛错）。本轮按源文件实测
   **缩小**了它的适用范围：法线属性头是 `ByPolygonVertex` + `Direct`，该组合在
   `getVertexIndex` 里直接返回 `nMeshVertex`，两个可疑分支都走不到（这也顺手把 A-43
   与它分开）；仍会走到那条路的是 UV/材质一类 `IndexToDirect` 元素。
   **未证实之前不要给 `default` 加"显式失败"**——那会掩盖真正成因，并把"改完就好了"
   的假象送进验收记录。
4. **`lugouqiao_v15` 已跑完，验收未做**。15:47 起跑、**21:52:17 结束**（21 621 s ≈ 6 h），
   产出 **1059 个瓦片**（1057 个 tile 文件：931 `.b3dm` + 126 `.i3dm`），
   `errors: []`、`featuresMissingFromLeaves=0`、`proxySkippedCount=0`、
   `proxyDroppedFeatureCount=0`、`maxTrianglesInTile=119840`（在 12 万硬上限内）、
   `maxContentBytes=3238136`、`maxDepth=18`、`aggregateProxyCount=122`、
   `nodesBoundLoose=332`、`nodesCertified=450`、`peakRssBytes=9.84 GB`、454 条 warning。
   终盘全量对账改用仓库工具 `scripts/accessor_extents_scan.js`（100 % 覆盖，
   1057 文件 / 4404 个 VEC3 accessor，不再是一次性脚本）：**声明 accessor min/max
   与实数据逐 accessor 全等、非有限坐标 0、`|coord|>1e6` 在 931 个 b3dm
   （A-38 的作用面）上 0 命中，最大只有 4 337 m**；污染字节与量级离散那两条
   仍是 `b3dm_attr_probe.js` / `evidence/a38-forensics.txt` 的口径，**0 命中**结论不变。
   4 处 >1e6 命中全在 i3dm（`i1017/i1018`，最大 1.223e8 m），那是 **A-41 的实例烘焙错位**，
   按 `--abs 1e4` 复扫得到 6 个 i3dm 文件，与下一段 A-41 记录的
   "6 个瓦片烘焙坐标 >1e4、最坏 |v|=1.22e8 m"逐个对上。同一条判据在 **124 个数据集**（修复前后各一批）上实测
   **声明≠实算 = 0**，唯一 A-38 命中仍是 `matrix_a34/t02` 的 3 个瓦片，
   见 `evidence/accessor_extents-baseline.txt`。即这份产物**没有** A-38 签名；
   但有 **195 条被索引引用的零长度 NORMAL**（分布在 **26 个**瓦片，常以连续 3 顶点成组，
   如 `mesh0 v24/v25/v26`）。**口径注**：26 数的是"图元里含零法线的文件"；"真隐患"（落在
   非退化三角形上）那 55 条涉及 **19 个**文件。此前记录的"19 个文件"与本次 26 不是同一口径，
   未回溯确认它当年按哪一档数。定级已单独立项为 **A-43**：本轮把 `scripts/zero_normal_triage.js`
   的主判据从绝对面积阈值（1e-8↔1e-9 之间"真隐患"档在 36↔55 之间跳，判别力不足）改成
   `面积 > REL × 该 primitive 中位面积`，绝对阈值降为对照列，并用新增的
   `scripts/make_zero_norm_fixture.js`（已知答案：真隐患 3 / 退化 2）做阳性对照。
   重扫同一份 v15：**退化 140 / 非退化 55**；按瓦片类别关联 report 后
   **代理 134 / `EXACT_LEAF` 叶瓦片 61 / 聚合代理 24**——4 个叶瓦片各 3 条全落在非退化三角形上，
   所以**不是简化器独有的**；值确定且跨 LOD 同签名，所以**不像 A-38 那一族**。
   **源侧还是写出侧这一岔口本轮已判掉，而且没动构建**：新增
   `scripts/fbx_source_normals_probe.js` 裸读**源** FBX 的法线数组（`encoding==1` 必须先 inflate，
   不解压直接数会得出"24.7 万个零"那种荒谬数），实测这份 335 MB 源文件里有
   **63 个零角点 / 21 个整面全零三角形**（按 `PolygonVertexIndex` 的真实负下标边界归面，
   不是"3 角点/面"假设；散落单角点 0），按内容去重只有 **7 个** mesh 带零（原始 36 个数组里
   29 个是逐字节相同的 instance 拷贝）。最硬的一条是两条不相干读路径撞上同一组数：
   源侧某数组 14 454 角点 = **4 818** 面、1 个整面全零 ↔ 产物 `i952`/`t162`/`t166` 的
   `#mesh0` 各自"该图元三角形 4 818、零法线 3 条"。据此 A-43 **成因定级低**（源数据缺陷 + 忠实透传），
   处置改为写出时把这类零法线**重算为其所在非退化三角形的面法线**（邻接平均在这里没有可用的
   非零邻居）；证据 `evidence/a43-source-side-fbx-probe.txt`。
   本条只定**成因层面**，没有做逐顶点归因（需要 mesh 级源↔产物 id 映射，未做）。
   **"不按拒绝发布处理"这半句本轮被实测推翻**：2026-10-20 官方 validator 逐资产跑完，
   `lugouqiao_v17` **rc=1、26 个内容文件、叶层 195 条 `is not of unit length: 0` ERROR**，
   逐顶点数与自研 triage 的 195 = 140 + 55（19 文件）对上；同一工具对
   `matrix_a42fix` 12/12 和 `longxiang_a42` 判 **0 error**。也就是说源侧起源能解释"不是构建造的"，
   但**解释不了"能发布"**：官方发布闸按 ERROR 计，v17 在写出侧处置（上面那条重算面法线）
   落地并重跑之前**不可发布**。禁止用"剔除含零法线的图元 / 不写 NORMAL 属性 /
   把 ERROR 降级"这类手段凑成通过。
   **写出侧修复已于同日 `49b6d3c` 落地（代码级），产物级复验还没做**：`SourceModel::repairZeroNormals`
   在 `materialize()` 装 `setNormalArray` 之前重建，`test_production nrmRepair` 17/17、全量 167/0、rc=0。
   上面"邻接平均没有可用的非零邻居"那句只说对了形态、说轻了后果——按
   `scripts/zero_normal_repair_plan.js` 对 v17 全量分档实测 **A=55（入射面可算）/ B=140（自身全退化、
   同图元有有效面可兜）/ C=0（整图元都退化）**，C 档为 0 才让"不编造方向"成立；兜不到面的分支保留为零
   并 `OSG_WARN`。**在跑的 `lugouqiao_v18` 不算验收轮**（exe 时间戳 10:00，早于该 commit）。
   **仍未做**：A-33 口径的 i3dm 展开三角形核对、`aggregateProxies` 复核
   （这两项本轮已由 `glb_prims.js` / `check_manifest_assertions.js` 在 v17 上补做，读数见
   VALIDATION_REPORT §6.2 的"v17 电池读数"表），官方 validator 也已跑（即上面的 FAIL）。
   **已完成并有结论的一项**：A-41 对这份产物的影响评估 = **全废**。`scripts/i3dm_pose_check.js`
   实测 126/126 个 i3dm 瓦片 POSITION 全 0、1787 条实例位姿 0 命中、原型 `|v|` 被累乘到
   1.22e8 m；这份产物作为"可发布数据集"已经不成立，上面几项待办只对**修复后的 v17** 有意义
   （v16 在写出阶段终止，盘上没有 `tileset.json`，见 A-41 记录）。
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
   **v17 已落盘**（00:35→08:35，28 485 s；1057 个分发文件 / 931 b3dm + 126 i3dm，471 个 `.ktx2`）。
   验收电池（**逐条取 `$?` 再管道**，`| tail` 会把退出码盖成 `tail` 的 0——本轮踩过）：
   - `node scripts/i3dm_pose_check.js` → **rc=0**（126 个 i3dm / 1787 条实例位姿在**相对帧**全对上，
     绝对帧比对 0 命中）
   - `node scripts/tileset_bounds.js` → **rc=0**；`node scripts/check_manifest_assertions.js` → **rc=0**
   - `node scripts/accessor_extents_scan.js` → **rc=0**（b3dm >1e6 **和** i3dm >1e6 **都是 0**：
     后者是 A-41 的签名，v15 实测 6 个 i3dm 命中）
   - `node scripts/zero_normal_triage.js` + `zero_normal_tile_class.js` → **rc=0**
     （仍是 ~195 条、同样签名——A-43 已定性为源侧自带，**不是回归信号**，
     见 `evidence/a43-source-side-fbx-probe.txt`）。**但这个 rc=0 不是发布信号**：
     自研尺子只统计不拦，官方 validator 把同一批顶点逐条判 **ERROR**（下一条）
   - `bash scripts/run_validator.sh F:/wb_3dtiles_out/lugouqiao_v17` → **rc=1**（报告在
     `validation-2026-10-20/`；本轮先写默认目录再迁出，`validation/` 里 10-19 那批原件已用
     `git checkout --` 还原，没有被覆盖）
     官方 `3d-tiles-validator@0.6.1` 报 **26 个内容文件 / 叶层 195 条**
     `Vector3 … is not of unit length: 0`（全为 A-43 零长度 NORMAL），
     682 warning 全是 `image/ktx2`（validator 自己不认识 `KHR_texture_basisu`：v17 叶层
     **691 条** `Cannot validate an extension as it is not supported by the validator`
     的 info，`longxiang_a42` 81 条）。同法 `matrix_a42fix` 12/12、`longxiang_a42`
     **0 error** → 尺子没有乱报，v17 就是没过官方闸。读数明细见 VALIDATION_REPORT §7
   - `node scripts/path_leak_structural.js` → **rc=0**（1057 瓦片 + 3 旁证全 clean，尺子失效 0）。
     注意 `path_leak_scan.js` 在 v17 报 **rc=1 / 4 处**，那 4 处经结构定位全在 GLB BIN chunk 里，
     是字节噪声不是泄漏；已把它降级为粗筛，验收口径改用结构版。
     证据 `evidence/a42-path-leak-v17-adjudication.txt`
   - `node scripts/glb_prims.js` → **rc=0**（1057 个内容文件违例+解析失败 **0**：
     `max prims/tile=11` 在 `t53.b3dm`；A-33 口径的实例展开三角形最大 **119 840**
     = `i1042.i3dm`，与 `report.maxTrianglesInTile` 逐值对上；去重侧 37 885，展开合计
     11 350 646）——**A-33 的展开核对与 B-9 的 primitive 回读就此在真实 FBX 上闭合**
   - `node scripts/check_manifest_assertions.js` → **rc=0**，同一行给出
     `aggProxies=122 / aggBuilt=122 / aggSkipped=0`、`occurrences=4250`
     （= `report.manifestOccurrences`），**`aggregateProxies` 复核就此闭合**
   - **仍未做**：视觉验收（判据本身在重做，见本条第 7 项）。官方 validator **本轮已跑**，
     结论是 **rc=1 未过闸**（上一条），不再是"从没跑过"。
     VALIDATION_REPORT §6.2 的"v17 电池读数"表已按上面七项逐条登记。
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
   ② 相机判据**整批不可用**（本轮把机制查清并已修，见下）。加上 A-41 之后所有 i3dm
   截图都是错版，视觉验收整批重做。

   **② 的准确机制**（我原先写成"flag 报 true 而相机是 null"，逐条核对产物后确认**不是**这样）：
   把 `docs/visual-acceptance/runs/*` 里 720 条 `kind:"shot"` 记录全扫一遍——
   `cameraFinite` **字段缺失 547 条**（含全部 `ax_*`/`mx_*`/`full_*` 批次，那些页版本压根没写它），
   而 `va_paths.js` 判的是 `r.cameraFinite === false`，"缺字段"因此被读成"没问题"→ camBad 恒 0；
   另有 **39 条** `camera.lon/lat/height` 落盘成 `null`（NaN 经 `JSON.stringify` 静默变 null），
   其中 `ax_*` 5 个会话各 6/12 帧。**没有任何一条**是"flag=true 而相机非有限"。
   漏报来自**字段缺失 + 判据只查 `positionWC`/`directionWC`、没查真正写进记录的经纬度**。
   修法两条：页面新增 `cameraFiniteNow()`（同时查 WC 位置/视线方向与 cartographic 的
   经纬度高程，`c` 缺失时判 false 而不再抛异常）+ 所有落盘数值走 `fin()`（NaN→null，
   但此时 flag 必为 false）；`va_paths.js` 增加 `camNaN`/`noFlag` 两列，直接按记录里的相机值
   补判，**旧批次不重跑也能看出损伤面**——
   `node scripts/va_paths.js ax_` 现报 `shots=60 holes=60 camBad=0 camNaN=30 noFlag=60`。
   `camBad` 列语义未改，历史引用数字仍然可读。
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
9. ~~**"1~2 小时"这个目标至今没有实测数据**~~ —— **2026-10-20 下午闭合**：
   `lugouqiao_v19` 实测 `totalSeconds=4 597.57`（1.28 h，v17 的 6.2×），
   逐字段对照证明只有误差字段变化，产物级验收（含 A-43 复验）全绿。
   见上面"lugouqiao_v19 全量重跑"一节。
   仍然成立的限制：这份二进制**同时**含 A-43 写出侧修复（`49b6d3c`，已提交）+ #6/#7 +
   D1/D2/D3 三处误差侧修复（未提交），所以 6.2× **无法单独归因**给某一条改动，
   要归因得再出对照二进制（比如只带 #6/#7 的那份）。
   `lugouqiao_v18` 盘上那份 `textures/` 半成品仍是废件，不是任何验收产物。
10. **下一根杠杆：并发余量远没用完，但先补仪表再动手**（2026-10-20 下午改写）。
    `micro_*` 仪表**已经接进 `ConversionReport` 了**（v19 读到了，见 PERFORMANCE §2.2），
    原先"读不到占比"那句作废。现在的问题是三件别的：
    ① 叶瓦片的 `_model.materialize()`（`ProductionPipeline.cpp:613`）**没有计时器**，
       代理 `build()` 只有 simplify/aggregate 两项 ⇒ `micro_*` 累加 8 240 s 对
       进程 CPU 秒 12 168 s，**≈3 900 s（32 %）无归属**；
    ② 瓦片级 `micro_evaluateSec` 只汇总、没进 `report.tiles` ⇒ **读不出关键路径是哪个瓦片**；
    ③ `micro_evaluate_samples` / `micro_aggregate_candidates` 两个**计数**混在秒数组里。
    先修这三条，再按 PERFORMANCE §2.2 的排序动杠杆（提高 `WB_JOBS`、
    `inflight+ready ≤ window` 与 `intraNodeThreads()` 除法、细化堆循环并行、
    最近孩子优先的 BVH 遍历）。`finalizeTileFromWork` 里 `writeContent` 仍是主线程串行，
    实测上限只有 564.71 s，排在上述几条之后。
    > **2026-10-21 更新**：①②③ 三条已由 A-47 补完并接线（单测 5 条断言 + `longxiang_l0b` 产物读数）。
    > 补完之后**嫌疑对象换了**：无归属的那 32 % 不是 `materialize`（实测只值 0.09 CPU 秒），
    > 是 libktx 写 `.ktx2` 时自起的 basis 压缩线程（进程 CPU 的 35.6 %）⇒ 新的 L7。
    > 判据①只在 65 MB 档闭合，335 MB 档待 `lugouqiao_v20` 用新二进制复测。

11. **A-51（2026-10-21 新发现，阻塞级，OPEN）：保住代理纹理之后，内节点瓦片的 glTF primitive 冲破手册 §3 的 ≤24，而生产侧对 primitive/材质没有任何闸。**
    `scripts/glb_prims.js F:/wb_3dtiles_out/longxiang_final512` → **rc=1**，6 张代理超限
    （`t0`=27 / `t1`=26 / `t21`=28 / `t29`=25 / `t79`=32 / `t100`=28），修前基线
    `longxiang_covfix` 全库最大 11 / rc=0；叶瓦片四份产物**逐值相同**（max 11），
    退化完全在代理侧。机制三条 + 未拦住的一条，都在 AUDIT A-51：代理 prims == 源 feature
    的 distinct 材质数（剥纹理时纯色 StateSet 会被 `MERGE_GEOMETRY` 并掉，保纹理后不并了）；
    图集路径被 `GltfOptimizer.cpp:1089-1099` 的"任一 UV 分量 >1 就不打包"判据整体挡掉，
    而本资产 sampler 是 `REPEAT`、UV 越界是**合法**的；共用同一张图的材质各自带不同
    `KHR_texture_transform`，所以**按材质签名去重实测零收益**（代理 mean 12.7 → 12.7）。
    `maxPrimitivesPerTile`(20)/`maxMaterialsPerTile`(24) 只被 `SpatialPartitioner.cpp:16-17`
    的源侧切分消费，代理路径与 `selfValidate` 都不看它（A-32/A-33 同族）。
    **需要用户裁定的四条方向**（均未实施）：A 把 `KHR_texture_transform` 折叠进 TEXCOORD_0
    再按图合并（实测上限 max 18 / mean 7.6，两道预算都能满足，但改的是贴图坐标、等价性未实测）；
    B 给代理加真闸并超限再细分（`tileCount`/`maxDepth` 会变）；C 只把超限的 6 张剥纹理
    （`t0`/`t1` 是顶层，撞"顶层无贴图是不可接受的"）；D 上调 §3 上限（手册禁止，我没这个权限）。
    在方向选定前：`longxiang_final512` **不是可发布产物**，#16 不算完成，A-48 状态写成
    "代码 FIXED / 验收 OPEN"，提交裁定也不成立。
    > **2026-10-21 下午更新（本节上面那段是裁定前的备选，现在 A 已实施）**：用户选 **A**，
    > 实测代理 prims 32/12.7 → **24/9.8**、`>24` 违例 **6 → 0**、纹理保住；上面写的
    > "上限 max 18 / mean 7.6" 是**预测**，实测 24（预测取的是图去重数，合并后上限另有其值）；
    > "等价性未实测"也已补上（450 组 / 505 880 顶点量化多重集 0 组有差 + 逐像素截图差 ≤5/255）。
    > 详见文末"2026-10-21 下午"一节与 AUDIT §A-51 落地、VALIDATION §8c。

## 2026-10-21 下午：A-51 方案 A 落地（代理折叠）+ 补 primitive/材质预算闸，验收过程中修出两条新问题

### 做了什么

1. **折叠本体**：`GltfOptimizer::foldTextureTransformsIntoUv` / `restoreFoldedTextureUvs`
   （`src/utils/GltfOptimizer.cpp`）。代理写出前把图集子矩形的 `KHR_texture_transform`
   烘进 `TEXCOORD_0` 并清掉 enable 键，共用同一张图的 drawable 于是落到同一个
   `tinygltf::Material`，由写出侧 `GltfMerger::mergeMeshes()` 融合图元。
   **只对代理开**（`writeContent(..., !isLeaf)`）：叶瓦片按手册 7.4 只允许无损重建，改 UV 属有损。
   共用宿主（贴图/材质）必须**所有引用者都可折**才允许清键，否则一个宿主牵连全库。
2. **预算闸**：`selfValidate` 解析每个内容文件的 glTF 容器数 primitive / 材质，超预算即失败。
   配套 `budgetGate` 单测用假 b3dm 打正反两侧。
3. **作用域守卫**：见下面"查出的缺陷"第 1 条。

### 机制更正（A-51 原来把合并的执行者写错了）

`SHARE_DUPLICATE_STATE` / `MERGE_GEOMETRY` **不在** `EXPORT_GLTF_OPTIMIZATIONS`
（`include/utils/GltfOptimizer.h:155-163` 实际是 `INDEX_MESH | REDUCE_DRAWCALL_OPTIMIZATIONS`）。
代理图元的合并全在写出侧：`Osg2Gltf::getCurrentMaterial` 用 `tinygltf::Material::operator==`
（**比较 `extensions`**）去重材质，`GltfMerger::mergeMeshes()`（`src/osgdb_gltf/GltfMerger.cpp:80`）
按 `world matrix × material × 属性名集合` 分桶融合。折叠之所以有效，是因为它把
`KHR_texture_transform` 这项 `extensions` 差异消掉了。等价性是构造级的：写出侧读 enable 键、
键关则用 `(0,0,1,1)`，随后**无条件** `flipGltfTextureYAxis`，
所以未折叠 `V = 1-oy-sy·v` 与折叠 `V = 1-(sy·v+oy)` 同式，实测最大逐顶点偏差 2.98e-8。

### 验收过程中查出的两条缺陷

1. **折叠态跨瓦片外泄**（本轮修）。`optimize()`（建图集）在 `foldBeforeWrite()` 之前，
   而还原只发生在下一次 `fold()` 开头和超限阶梯内 ⇒ 瓦片 N 清掉的变换键会被**瓦片 N+1 的
   图集装配**读到。原代码在阶梯处已经写了"折叠态绝不能进下一轮图集"，但只在同一瓦片内守住。
   修法：`ProductionPipeline.cpp:545-556` 的作用域守卫，`writeContent` 一返回就还原。
   **守卫为瓦片隔离（正确性）而保留，不是为性能**。先前本节记过"加守卫 ≈2.4× CPU"，
   那组归因**已撤回**（详见 AUDIT §A-51 落地第 5 条）：守卫拿掉的 17:00 同时段对照
   （`micro_evaluate` 260.1 s / `micro_write` 250.5 s / 墙钟 329.1 s）与 16:00、16:07 两次
   带守卫运行同档，而**贴图写出上游**的 `micro_evaluate` 也一起翻倍了——工作量计数器
   （48 963 414 样本 / 121 贴图文件 / 129 瓦片）五次逐项相同，差的是机器速率不是代码。
   ⇒ **守卫的真实代价：未测**；要量必须在同一时间窗内交替跑两个二进制（归 #10/#11）。
   结论不变的那半：不加守卫时"折叠版只要 120 s"至少一部分是**靠污染后续瓦片的贴图决策省下的
   编码**，与 A-48 立的"本瓦片之外的瓦片绝不能被带小"同族，所以守卫不能被"优化"掉。
2. **图集页跨运行不逐位相同**（**A-52**，已修，串行档复证通过）。现象：同一二进制同一参数连跑两遍，
   `fold512` vs `fold512_b` 差 5 组图集页 + 3 个没走折叠的叶瓦片字节，页**尺寸**两侧逐值相同。
   **原归因（`GltfOptimizer.cpp:2004/:2026` 的 `unordered_map<osg::Geometry*,…>` 指针哈希桶序）已推翻**：
   `WB_ATLAS_TRACE` 打全长键 + 逐像素内容哈希后实测，`lx_tr2_a`/`lx_tr2_b`（`WB_JOBS=1`）138 轮输入
   **逐字节相同**、归置结果与页尺寸相同，只有页名不同 ⇒ 漂的是画布上**从没被写过的那些字节**：
   `TexturePacker::pack` 用 `allocateImage` 建画布后不清零（`TexturePacker.cpp:100`），子矩形铺不满，
   缝隙留着堆上残留，而页名与 KTX2 都是按整张画布算的。修复 = 分配后 `memset` 清零。
   复证：`lx_z_a` vs `lx_z_b` trace 差 **0 行**、产物 **246/246 逐字节相同**。
   ⇒ **验收方法的连带影响反转**：产物逐字节对照从"只能当阴性对照"恢复为可用主尺子，
   量化坐标多重集（`E:/tmp/diag_all.js`）降为交叉验证。**默认并发档（出货配置）也复证通过**：
   `lx_p_a` vs `lx_p_b` 各 rc=0、**246/246 逐字节相同、trace 差 0 行**；同一把尺子在清零前的
   `lx_tr2_a`/`lx_tr2_b` 上仍判 FAIL（6↔7 孤儿页），所以这个 PASS 不是量尺失灵。

### 判据读数（同机、同参数 `-dc 24 -ptw 512`；计时**不入表**，理由见上第 1 条）

| 判据 | 结果 |
|---|---|
| 代理 prims max/mean | 32 / 12.7（不折叠）→ **24 / 9.8**（折叠，守卫前后一致） |
| 代理 `prims>24` | **6 → 0** |
| 代理无纹理 | 2 → 2（**纹理一条没少**，满足"顶层无贴图不可接受"） |
| 叶瓦片 prims max/mean | 11 / 3.6 不变 |
| 内容字节合计 | 87 793 600 → **87 563 536**（−230 064 B，−0.26 %） |
| 最终采样坐标等价 | 450 组 / 505 880 顶点，量化 1e-5 后**有差的组 = 0** |
| OGC validator | 头 0/121/0；**真叶** 1272 WARN / 837 INFO / **0 ERROR**（口径见下）。先前记的"叶层 1514 WARN"里含 242 个带 `causes` 的结构节点（121 个告警文件 ×2），**不是真叶数**；ERROR=0 不受这个口径影响。当前二进制的 `lx_p_a` 与折叠期两档（`longxiang_guard512`、`longxiang_final512`）三份报告的真叶读数**逐项相同**（1272/837/0、告警文件 121）⇒ A-52 清零没有改变校验结果 |
| 预算闸开火 | 默认 `-dc 20` → **rc=3**，拒 `t0`=24、`t1`=22 |
| 预算闸开火（抬默认后复证） | 不加 `-dc` 的默认运行多次 **rc=0**（`lx_det_a/b`、`lx_tr_a/b`，抬默认后的构建）；当前构建显式 `-dc 23` → **rc=3**，拒 `tiles/t0.b3dm (24 primitives > 23)`，且**产物不提交**（`F:/wb_3dtiles_out/lx_dc23/` 无 tileset.json、`tiles/` 空，只剩 textures/ 暂存）。⇒ 抬默认是把闸门放到 24，不是把闸门关掉 |
| 视觉（逐像素） | 30.00R 差 **0**；11.42R 平均 0.007 / 最大 5；3.15R 平均 0.096 / 最大 5；>8/255 占比 0.000 %。<br>**更正**：`30.00R 差 0` 是**两个未加载的洞相减**（两侧 coverage=0），不构成证据；近景帧与修正口径见 2026-10-22 一节 |
| 单测 | **277 / 0**（`foldUv` 26 + `budgetGate` 9 + `atlasCanvas` 14）；清零 + 回归用例落地后的新二进制重跑 **277/0 ×2**，rc 直接取 |

### 没做完 / 需要用户裁定

- ~~**`-dc` 默认 20 vs 手册 §3 的 24**~~：**已裁定并落地**（用户 2026-10-21：抬到 24）。`main.cpp:500` 默认 24，
  `:666` 无条件覆盖 `profile.budget.maxPrimitivesPerTile`，闸门开火证明见上表最后一行。
  遗留副作用已记进代码注释：不带 `-sp` 的 HLOD 路径（`main.cpp:761`）也继承 24，
  `TreeBuilder.cpp:579-580` 的 `min((level+1)*InitDrawcallCommandCount, -dc)` 上界由 20 抬到 24。
- ~~**近景视觉验收缺**：折叠侧第 4 帧 `dist_1.20R` 因超时未截到，折叠最相关的近景 LOD 边界
  没有像素级证据；且这三个机位 `chromaFraction ≈ 0.0002`（近乎灰度），灵敏度本来就低。~~
  **已于 2026-10-22 补齐**（同二进制匹配的 m100 一对，见本节末尾）。
  `chromaFraction ≈ 0.0002` 这条灵敏度限制**仍然成立**（本轮四帧依旧 0.0002～0.0003）。
  ~~**发布资产 `lugouqiao` 的视觉验收仍未做。**~~ ⇒ 由用户裁定**停做**（A-54 走明确失败）。
- **守卫的真实代价未测**（不是"2.4× 是否接受"——那个数已撤回）。这一轮只证明了守卫**不是**
  16:00 之后那批运行变慢的原因；它自己那一次 UV 还原值多少秒，要同时段交替 A/B 才有数（归 #10/#11）。
- **A-52 已结案为"画布未初始化"并修掉**（用户 2026-10-21 裁定"改成确定性排序"→ 实测发现排序不是变量，
  真正的自由度是缝隙字节）。串行 + 默认并发两档都 246/246 逐字节相同。
  ~~**剩：给画布清零加回归测试**~~：**已补**（`atlasCanvas`，14 条），回归不再只靠产物级 A/B。
  过程里翻了一次车：只污染 1 MiB 堆的第一版在**去掉清零**的情况下仍然 10/0 通过（小画布拿到的是
  全新虚拟页，天然为 0）—— 靠"去掉修复重编一遍"的负对照才暴露。改成污染**同尺寸**堆块、6 轮取和后，
  负对照 `dirty=600/3600` 3/3 FAIL、装回修复 14/0 3/3 PASS。
- **A-53（新记，OPEN）**：`test_production.exe` 在 `giant` 用例上间歇性 STATUS_HEAP_CORRUPTION
  （`0xc0000374`，模块 ntdll）。含清零 + 回归用例的当前二进制：全量 **277/0 ×2**、单跑 `giant`
  **20/20 通过**（无并发负载、串行 20 次，`F:/` 无其他任务）；旧二进制是"263/0 → rc=127 → rc=0"交替。
  样本仍小、且当年是在并发重负载窗口观测到的，**不宣布消失**，详见 AUDIT A-53。
- **未提交**：本轮全部改动仍在工作区，提交裁定归用户。

## 2026-10-21 晚（20:08→20:38）：发布资产 `lugouqiao_v20` 被 primitive 预算闸拒绝 ⇒ **A-54 OPEN**

### 跑的是什么

当前二进制（含 A-51 折叠、A-48 代理纹理、A-52 画布清零、`-dc` 默认 24），命令
`-sp -i D:\BaiduNetdiskDownload\20240529卢沟桥分洪枢纽.fbx -o F:\wb_3dtiles_out\lugouqiao_v20`
（不带 `-dc`/`-ptw`，全默认）。包装 `F:/wb_3dtiles_out/v20_run.ps1`，20:08:41 起、20:38:42 结束，
墙钟约 **30 min**（v19 是 4 597.57 s = 1.28 h；**这台机器此刻还有并发**，两个数不可比，见
[[benchmark-timing-drift]] 同族纪律 ⇒ 本轮不拿它当提速/回归证据）。

### 结果：闸门按手册正确开火，产物未提交

```
ERROR: selfValidate: content over primitive budget: tiles/t8.b3dm (26 primitives > 24)
Failed converted … (production pipeline, output not committed).
```

- stderr 全文只有这一条 ERROR（`F:/wb_3dtiles_out/lugouqiao_v20.err.log` 88 字节）。
  `ProductionPipeline.cpp:1952` 是 `push_back` 不是 `return`，`main.cpp:735` 打印**全部** failures
  ⇒ **越界瓦片集合 = {t8}**，不是"只报了第一条"。
- `finalize()` 在 `selfValidate` 失败处 `discardStaging()` 并返回（`:2329`），
  `tiles/` 已被删除：`lugouqiao_v20/` 现在只剩 `textures/` 939 张暂存图集页，
  **没有 tileset.json、也没有 conversion-report.json**（报告写在 selfValidate 之后）。
- 顺带一条 WARNING：`tile 54: far-LOD proxy built without textures, even the 64px texture floor
  does not fit budget (bytes=1146144)` —— 与 A-48 的下限失败关闭口径一致，未降级成静默。

### 为什么这是新出现的（对照读数）

同一份资产、`lugouqiao_v19`（10-20 13:09 的二进制，**不含 A-48/A-51**）用
`node scripts/glb_prims.js` 扫全量：**1057 个瓦片，max prims/tile = 11（`t53`），违例 0**。
⇒ 26 不是折叠算错，而是 A-48"代理保住贴图"把图集宿主数抬上来之后，A-51 折叠仍没能把 `t8`
压进 24。两条已生效的用户裁定在这里顶上了：**"顶层无贴图是不可接受的"** vs **"-dc 改成 24"（手册 §3 上界）**。

### 待裁定（不得私自降上界）

①显式抬 `-dc`（等于放宽手册 §3，要批准）；②对 `t8` 这类瓦片继续按预算降代理纹理分辨率 /
合并宿主，压到 24（牺牲远景清晰度）；③按 §7.3 末档走"明确失败" ⇒ 该资产当前不可发布。

### 诊断跑（仅为测量，不作验收产物）

`-dc` 只能判"第一个越界的是谁"，看不到分布 ⇒ `F:/wb_3dtiles_out/v20prims_run.ps1`
用 `-dc 100` 重跑一份（`lugouqiao_v20prims`，20:42 起）。**这次改的是单次运行的开关，
不是默认值、不是判据**；产物只用来读 prims 分布和 `t8` 构成。
另修了一条监视缺陷：`Start-Process -PassThru` 的 `$p.ExitCode` 实测为空（v20 的 mon 里
`end … rc=` 就是这个），诊断脚本改用 `System.Diagnostics.Process.WaitForExit()` 取码。

**结果：仍然拒绝，rc=3（21:06:41），换了一条闸**：

```
ERROR: selfValidate: content over material budget: tiles/t8.b3dm (26 materials > 24)
```

三条由此钉死的事实：

1. **`prims == mats == 26`（严格 1:1）** —— A-51 认定的"代理侧材质合并零收益"的形态在
   发布资产上同样成立：这 26 个 drawable 各自挂不同宿主，折叠合不掉。
2. **越界瓦片集合 = {t8}**：`-dc 24` 那次报 primitive 越界、`-dc 100` 这次报 material 越界，
   两次 stderr 都只有一行，而 `ProductionPipeline.cpp:1952-1965` 是 `push_back` + `main.cpp:735`
   打印全部 ⇒ 不是"只报了第一条"。
3. **抬 `-dc` 单独不够**：`maxMaterialsPerTile` 恒为 24（`ProductionProfile.h:34`），
   CLI **没有**开关（`main.cpp:661-679` 只用 `-dc/-tri/-mb/-ptw` 覆盖别的字段）。
   所以"①抬 -dc"这条路还必须同时动材质闸，而那只能改代码或加开关 ⇒ 更需要明确批准。

**这条诊断跑的口径限制（别当成出货配置的切片）**：`main.cpp:666-668` 把
`maxPrimitivesPerTile` 与 `targetPrimitivesPerTile` 一起改了，而 `SpatialPartitioner.cpp:16-17`
的切分比例**同时消费这两个字段** ⇒ `-dc 100` 不只放宽闸门，还改了切分决策。
所以"`-dc 100` 下 t8 = 26 材质"与"`-dc 24` 下 t8 = 26 primitive"是**两次不同切分**里的同名瓦片，
方向一致但不能当同一块内容读。

顺带把 v19 的对照读出来（`node /e/tmp/glb_mats.js`，解析器用 `glb_prims.js` 的
"t53 = 11 prims"做过阳性对照，实测两脚本一致）：v19 的 `t53.b3dm` = 11 prims / 11 mats /
10 texs / 10 imgs（带 `KHR_texture_transform`），v19 的 `t8.b3dm` = **1 prim / 0 material**
（老二进制那侧代理根本没贴图）。v19↔v20 的 t8 同样不是同一块内容，只能读成
"越界是 A-48 保贴图这一侧换来的"，不能读成"t8 从 1 长到 26"。



## 2026-10-22 上午（09:02→09:45）：A-54 裁定落地 + longxiang 近景视觉验收补齐

### A-54 的裁定（用户，2026-10-22）

**走 ③「明确失败」，先交付 longxiang。** ⇒ 上界一条都不动（`-dc` 保持 24、`maxMaterialsPerTile`
保持 24、不加材质开关、不用纹理杠杆去凑过闸），`lugouqiao` 的验收链**就地停住**：
不再重跑、不截视觉帧、不跑 validator。`lugouqiao_v20` 保持"被闸拒、产物未提交"的现场
（`tiles/` 已 `discardStaging`，只剩暂存 `textures/`），这就是当前二进制对该资产的诚实结论。
以后要把这档资产救回来，需要的是**重新裁定判据**，不是再跑一次。

### longxiang 视觉验收：近景帧补上了（缺口 ② 闭合）

匹配对照换成**同二进制、同 `-dc 100`、只差折叠入口**的一对（`lx_fold_m100` / `lx_nofold_m100`），
四次截图落在 09:06→09:39 同一时间窗：`dist_1.20R` 平均 0.0564 / **最大 10** /
`>8/255` 通道 0.0033 % / `>16/255` **0 条**，coverage 与 `projPx` 两侧逐值相同
⇒ **折叠在近景 LOD 边界上视觉中性**，明细见 VALIDATION_REPORT §8c 末尾的表。
发布产物 `lx_p_a`（当前二进制、全默认：`-dc 24` 与 `-ptw 512` 都是默认值）对同窗重截的 `longxiang_guard512`
近景**最大 5、`>8/255` 通道 0 条** ⇒ A-52 清零与其后的改动无视觉变化。

### 一把尺子的缺陷（影响所有历史截图读数）

同一份 `longxiang_guard512`：10-21 16:35 截的 `silLumAvg` = 141.8 / 138.0 / 131.8，
10-22 09:39 重截 = 91.3 / 80.2，而 coverage、`projPx`、可见瓦片清单（3.15R 各 51 块，
两侧 `only in` 均空）逐项相同 ⇒ 差在**渲染曝光**不在产物。
⇒ **逐像素判据只在同一时间窗内有效**，跨窗绝对亮度不可比；历史表里"最大差 5"仍成立
（两侧同窗），但不能与本轮表做趋势。这条已写进 VALIDATION_REPORT §8c。

### 发布产物 `lx_p_a` 的七项自研电池（本轮补齐，逐条取 rc 再管道）

先前只登记过它的 validator 读数，自研七项**没在这份产物上跑过**。2026-10-22 全跑一遍：
**7/7 rc=0**（`glb_prims` 127 文件违例 0 / `max prims/tile=24` 正好在上界；`i3dm_pose_check`
10 条相对帧 100 %、残差 ≤7.11e-6 m、绝对帧 0 命中；`check_manifest_assertions` 571 条 PASS；
`accessor_extents_scan` 1 662 访问器 0 违例；`zero_normal_triage` 零长度 NORMAL **0**；
`path_leak_structural` CLEAN；`tileset_bounds` 五列全 0）。明细表见 VALIDATION_REPORT §8c。
其中一条是**解锁性质的**：`i3dm_pose_check` 过 ⇒ §9 那条"任何带 `.i3dm` 的既有产物一律不可发布"
对当前二进制的 longxiang 不再成立（`lx_p_a` 有 6 个 i3dm）。

### 提速线（L7 前置问题：原对照实验无效，待重做）

按"跑完以后再看怎么在不影响质量的前提下提速"这条，先做**风险解除**而不是改代码 ——
但 2026-10-22 上午那对 `WB_KTX2_THREADS=1` vs `=24` 的"风险解除"**没有成立**：
当时加载的 `Release/osgdb_ktx.dll`（前缀 `1feb96f2…`，10-20 18:17）里**没有** `WB_KTX2_THREADS`
这个 getenv（`git show HEAD:src/osgdb_ktx/LoadTextureKTX.cpp` 查不到、`git log -S'WB_KTX2_THREADS'` 也无结果），
两跑执行的是同一份代码 ⇒ **"244/244 个产物文件 sha256 逐位相同"只能证明同码两跑相同，证明不了
`threadCount` 的产物中性**；由它派生的"24 线程比 1 线程多烧 79.89 s 进程 CPU（+11.8 %）"
"⇒ L7 的价值是把核还给别的活"也一并撤回 —— 差值出现在同一份代码的两跑之间，只能是机器漂移。
原表与原始读数保留在 PERFORMANCE_REPORT 杠杆表下方，标注为历史记录。
**根因是 MODULE 插件不在 `model23dtiles` 的依赖图里**：`--target model23dtiles` 不重建 `osgdb_ktx`，
所以带 knob 的源码从未进入实际使用的 DLL。
**2026-10-22 11:41:33 显式重建 `osgdb_ktx` 后**（新 DLL 前缀 `1124d517…`，含 knob），
用 `WB_KTX2_THREADS` 做对照才第一次有效。同日重跑得到的**新证据**（两侧各 n=1）：
默认跑进程峰值 **43** 线程、`=1` 跑 **20** ⇒ **已确认参数影响运行时线程数**（这是行为学证据，不是"探针存在即生效"）；
两跑各 129 tiles、`errors=[]`，`tiles/`+`textures/` 244 个文件逐位相同，连同 `tileset.json`、
`feature-manifest.json` 共 **246 个非报告文件逐位相同**（每侧 247 个文件，唯一不同的是
`conversion-report.json`；剔除计时/RSS/时间戳字段后两报告差异 0 行）⇒ 属**初步证据**。
**线程数下降不等于性能提升**：wall 118.06 vs 213.77 s 各只有 1 个样本，不能判快慢；
"线程配额不改产物"与"跨运行可复现"都**待验证**（需 ≥3 轮交替 A/B）。
**335 MB 档的 L7/L1 复测拿不到**：当前二进制在该档出不了盘（A-54 预算闸未绕过、预算未改），
而报告写在 `selfValidate` 之后 ⇒ 该档**未被验证**。
另注：`LoadTextureKTX.cpp:635-638` 的源码注释仍写着"实测 1 与 24 线程 244/244 逐位相同"，
其口径引用了本段已作废的实验、且只数了 `tiles/`+`textures/`；**源码注释属源码，本轮未改动**，待批准后更正。
