# STATUS.md — 分阶段执行状态

| 阶段 | 内容 | 状态 | commit |
|---|---|---|---|
| Phase 0 | 审计与基线（BASELINE.md / AUDIT.md / 构建脚本 / 测试资产 / 基线运行） | 完成 | 见下表 |
| Phase 1 | 源几何 IR 与 Feature Registry、BatchId 修复、manifest | 进行中 | |
| Phase 2 | 自适应空间二叉 BVH | 未开始 | |
| Phase 3 | 预算驱动代理生成 + 聚合 fallback | 未开始 | |
| Phase 4 | Seam Registry 与 feature 保留 | 未开始 | |
| Phase 5 | GeometryErrorEvaluator（双向 Hausdorff） | 未开始 | |
| Phase 6 | RefinementPolicy 与 tile tree writer | 未开始 | |
| Phase 7 | 纹理图集 / KTX2 / b3dm / i3dm / cmpt | 未开始 | |
| Phase 8 | Bounds、validator、端到端测试 | 未开始 | |
| Phase 9 | 性能优化与最终视觉验收 | 未开始 | |

## 可用命令

```bash
bash scripts/wb_build.sh                 # 构建 model23dtiles (Release)
bash scripts/wb_build.sh test_3dtiles    # 构建并跑单元测试目标
python scripts/gen_test_assets.py        # 生成确定性测试资产
python scripts/run_matrix.py --out <dir> # 跑全资产矩阵 + 校验
python scripts/tileset_tool.py validate <tileset.json> --budget 8388608
```

## BLOCKED

- B-1 真实 FBX 样本（无 `.fbx` 文件）
- B-2 `osgdb_osgt.dll` 未部署（已改用 `.osg`）
- B-3 OGC 3D Tiles Validator（需 npm，未安装）
- B-4 CesiumJS 视觉验收页面 + 浏览器
