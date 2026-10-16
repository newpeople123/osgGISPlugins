# STATUS.md — 分阶段执行状态

| 阶段 | 内容 | 状态 | commit |
|---|---|---|---|
| Phase 0 | 审计与基线（BASELINE.md / AUDIT.md / 构建脚本 / 测试资产 / 基线运行） | 完成 | a53d419 |
| Phase 1 | 源几何 IR 与 Feature Registry、BatchId 修复、manifest | 完成 | d18175e |
| Phase 2 | 自适应空间二叉 BVH（对象/图元/三角形级） | 完成 | 2c3d597 |
| Phase 3 | 预算驱动代理生成 + 聚合 fallback | 完成（随 Phase 1/2 交付） | d18175e |
| Phase 4 | Seam Registry 与 feature 保留 | 完成（策略：跨瓦片对象不裁剪，归属单 tile） | d18175e |
| Phase 5 | GeometryErrorEvaluator（双向 Hausdorff） | 完成 | 2c3d597 |
| Phase 6 | RefinementPolicy 与 tile tree writer | 完成 | dcda2a9 |
| Phase 7 | 纹理 KTX2 / b3dm / i3dm | 完成（S1/S3 场景 + 真实 FBX 186 KTX2/6 实例瓦片实测）；cmpt 未触发（无混合实例场景） | 0d8bf02 |
| Phase 8 | Bounds、validator、端到端测试、原子化输出 | 完成（含真实 FBX 端到端 0 ERROR） | dcda2a9 |
| Phase 9 | 性能优化与最终视觉验收 | 页面/截图链路完成；真机 GPU 验收 BLOCKED-VISUAL；真实 FBX 完成 | bfa43a0 |

交付文档：BASELINE.md / AUDIT.md / DESIGN.md / STATUS.md /
VALIDATION_REPORT.md / PERFORMANCE_REPORT.md（均在 `docs/model23dtiles-production/`），
视觉验收：`docs/visual-acceptance/`（index.html + README + runs/）。

## 可用命令

```bash
bash scripts/wb_build.sh model23dtiles     # 构建 Release 工具
bash scripts/wb_build.sh test_production   # 构建单元测试（44 断言）
./Release/test_production.exe              # 跑单元测试
python scripts/run_matrix.py --out <dir>   # 12 资产回归矩阵
python scripts/run_extra_scenarios.py      # KTX2/超限/i3dm/b3dm 四场景
python scripts/tileset_tool.py validate <tileset.json> --budget 8388608
python scripts/visual_capture.py --tileset <tileset.json> --out <dir>  # 视觉截图
```

## commit 清单

| commit | 内容 |
|---|---|
| a53d419 | Phase 0：可复现基线与代码审计 |
| d18175e | Phase 1：源几何 IR + 空间划分 + HLOD 代理 + 误差管线 |
| 2c3d597 | Phase 2/5：分区器语义修正 + 单元测试 + TBB 关闭构建 |
| dcda2a9 | 原子化输出与失败清理（14.17）+ `-mb` 开关 |
| 0d8bf02 | 测试矩阵补齐 + 视觉验收页面 + BatchIdVisitor 重跑修复 |

## 真实 FBX 端到端（B-1 已解除，2026-10-16）

输入：`D://BaiduNetdiskDownload//longxiang.fbx`（66 MB，123 张纹理）。

**结果：0 ERROR，validator 0 error**。69 tiles（深度 11）、67 内容瓦片全部
≤ 8 MiB（最大 7.91 MB）、145 feature / 1013 occurrences、误差
max=168.4 m / p95=78.4 m / mean=19.7 m、certified 30 / boundLoose 23、
耗时 630 s、峰值 RSS 650 MiB。完整统计见 VALIDATION_REPORT.md §6。

调试过程中定位并修复的三个真实缺陷（commit bfa43a0）：
1. 内容字节估算低估 ~4 倍 → 分区器不切分，17 MB 瓦片超限；
2. 大节点 meshopt_simplify 因 targetError=diag*0.01 过宽完全不削减 → sloppy 兜底；
3. 代理简化只判三角形预算、漏掉字节预算 → 双预算判据。
另修复失败信息走 OSG notify 被缓冲吞掉、表现为"无声崩溃"的问题（改双写 cerr）。

## BLOCKED

- B-1 ~~真实 FBX 样本~~ 已解除（用户提供 `D:\BaiduNetdiskDownload\*.fbx`，
  端到端调试进行中）
- B-2 `osgdb_osgt.dll` 未部署（已改用 `.osg`）
- B-3 OGC 3D Tiles Validator（需 npm，未安装；以 `scripts/tileset_tool.py` 等价校验）
- B-4 ~~CesiumJS 视觉验收页面~~ 页面/脚本已交付并实测 53 帧；
  仅"物理 GPU 人眼验收"留待有显示输出的机器（BLOCKED-VISUAL）
